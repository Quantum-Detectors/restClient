#include "restApi.h"

#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>

#include <epicsStdio.h>
#include <epicsTime.h>

#include "jsonDict.h"

#define EOL                     "\r\n"      // End of Line
#define EOL_LEN                 2           // End of Line Length
#define EOH                     EOL EOL     // End of Header
#define EOH_LEN                 (EOL_LEN*2) // End of Header Length

#define DATA_NATIVE             "application/json"
#define DATA_HTML               "text/html"

#define MAX_HTTP_RETRIES        1
#define MAX_MESSAGE_SIZE        8192
#define MAX_BUF_SIZE            256
#define MAX_JSON_TOKENS         100

#define DEFAULT_TIMEOUT_CONNECT 1

#define ERROR(message) \
        { \
            std::stringstream ss; \
            ss << message; \
            setError(functionName, ss.str()); \
        }

// Requests

#define REQUEST_GET\
    "GET %s%s HTTP/1.1" EOL \
    "Host: %s" EOL\
    "Content-Length: 0" EOL \
    "Accept: " DATA_NATIVE EOH

#define REQUEST_PUT\
    "PUT %s%s HTTP/1.1" EOL \
    "Host: %s" EOL\
    "Accept-Encoding: identity" EOL\
    "Content-Type: " DATA_NATIVE EOL \
    "Content-Length: %lu" EOH

const std::string RestAPI::PARAM_VALUE           = "";
const std::string RestAPI::PARAM_TYPE            = "";
const std::string RestAPI::PARAM_MIN             = "";
const std::string RestAPI::PARAM_MAX             = "";
const std::string RestAPI::PARAM_ENUM_VALUES     = "";
const std::string RestAPI::PARAM_ACCESS_MODE     = "";
const std::string RestAPI::PARAM_CRITICAL_VALUES = "";

using std::string;


RestAPI::RestAPI (string const & hostname, int port, size_t numSockets) :
    mHostname(hostname), mPort(port), mNumSockets(numSockets),
    mSockets(new socket_t[numSockets]), mErrorFilter(new ErrorFilter())
{
      memset(&mAddress, 0, sizeof(mAddress));

    if(hostToIPAddr(mHostname.c_str(), &mAddress.sin_addr))
        throw std::runtime_error("invalid hostname");

    mAddress.sin_family = AF_INET;
    mAddress.sin_port = htons(port);

    for(size_t i = 0; i < mNumSockets; ++i)
    {
        mSockets[i].closed = true;
        mSockets[i].fd = -1;
        mSockets[i].retries = 0;
    }
}
RestAPI::~RestAPI()
{
    delete[] this->mSockets;
    delete this->mErrorFilter;
}

int RestAPI::connectedSockets()
{
  int connected = 0;
  for(size_t i = 0; i < mNumSockets; ++i){
    if (!mSockets[i].closed){
      connected++;
    }
  }
  return connected;
}

// Private members

int RestAPI::connect (socket_t *s)
{
    const char *functionName = "connect";

    if(!s->closed)
        return EXIT_SUCCESS;

    s->fd = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(s->fd == INVALID_SOCKET)
    {
        ERROR("Couldn't create socket");
        return EXIT_FAILURE;
    }

    setNonBlock(s, true);

    if(::connect(s->fd, (struct sockaddr*)&mAddress, sizeof(mAddress)) < 0)
    {
        // Connection actually failed
        if(errno != EINPROGRESS)
        {
            char* error = new char[MAX_BUF_SIZE];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            ERROR("Failed to connect to " << mHostname.c_str() << ":" << mPort <<
                  " [" << error << "]");
            epicsSocketDestroy(s->fd);
            delete[] error;
            return EXIT_FAILURE;
        }
        // Server didn't respond immediately, wait a little
        else
        {
            fd_set set;
            struct timeval tv;
            int ret;

            FD_ZERO(&set);
            FD_SET(s->fd, &set);
            tv.tv_sec  = DEFAULT_TIMEOUT_CONNECT;
            tv.tv_usec = 0;

            // Wait for the socket to change state
            ret = select(s->fd + 1, NULL, &set, NULL, &tv);
            // Check if the select call failed / timed out
            if(ret <= 0) {
                const char* error = ret == 0 ? "TIMEOUT" : "select() failed";
                ERROR("Failed to connect to " << mHostname.c_str() << ":" << mPort <<
                      " [" << error << "]");
                epicsSocketDestroy(s->fd);
                return EXIT_FAILURE;
            }
            // Socket changed state - check if we are now connected
            else if(::connect(s->fd, (struct sockaddr*)&mAddress, sizeof(mAddress)) < 0) {
                // Still not connected; socket state changed straight to `Failed`
                char* error = new char[MAX_BUF_SIZE];
                epicsSocketConvertErrnoToString(error, sizeof(error));
                ERROR("Failed to connect to " << mHostname.c_str() << ":" << mPort <<
                      " [" << error << "]");
                epicsSocketDestroy(s->fd);
                delete[] error;
                return EXIT_FAILURE;
            }
        }
    }

    setNonBlock(s, false);
    s->closed = false;
    return EXIT_SUCCESS;
}

int RestAPI::setNonBlock (socket_t *s, bool nonBlock)
{
    int flags = fcntl(s->fd, F_GETFL, 0);
    if(flags < 0)
        return EXIT_FAILURE;

    flags = nonBlock ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
    return fcntl(s->fd, F_SETFL, flags) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int RestAPI::doRequest (const request_t *request, response_t *response, int timeout)
{
    const char *functionName = "doRequest";
    int status = EXIT_SUCCESS;
    int received, ret;
    int errcode;
    struct timeval recvTimeout;
    struct timeval *pRecvTimeout = NULL;
    fd_set fds;

    socket_t *s = NULL;
    bool gotSocket = false;

    for(size_t i = 0; i < mNumSockets && !gotSocket; ++i)
    {
        s = &mSockets[i];
        if(s->mutex.tryLock())
            gotSocket = true;
    }

    if(!gotSocket)
    {
        ERROR("No available socket");
        status = EXIT_FAILURE;
        goto end;
    }

    if(s->closed)
    {
        if(connect(s))
        {
            ERROR("Failed to reconnect socket");
            status = EXIT_FAILURE;
            goto end;
        }
    }

    // Flush the socket prior to write/read
    this->setNonBlock(s, true);
    received = 1;
    while (received > 0){
      received = recv(s->fd, response->data, response->dataLen, 0);
      errcode = errno;
    }
    if (received < 0){
      // If the error code is EWOULDBLOCK then we have simply got
      // nothing to flush.  Any other error code needs to be dealt
      // with by closing the socket
      if (errcode != EWOULDBLOCK){
        ERROR("Failed to flush");
        epicsSocketDestroy(s->fd);
        s->closed = true;
        status = EXIT_FAILURE;
        goto end;
      }
    }
    this->setNonBlock(s, false);

    if(send(s->fd, request->data, request->actualLen, 0) < 0)
    {
        if(s->retries++ < MAX_HTTP_RETRIES)
            goto retry;
        else
        {
            ERROR("Failed to send");
            epicsSocketDestroy(s->fd);
            s->closed = true;
            status = EXIT_FAILURE;
            goto end;
        }
    }

    if(timeout >= 0)
    {
        recvTimeout.tv_sec = timeout;
        recvTimeout.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(s->fd, &fds);
        pRecvTimeout = &recvTimeout;
    }

    ret = select(s->fd+1, &fds, NULL, NULL, pRecvTimeout);
    if(ret <= 0)
    {
        std::string error = ret ? "select() failed" : "Timed out";
        ERROR(error);
        status = EXIT_FAILURE;
        goto end;
    }

    if((received = recv(s->fd, response->data, response->dataLen, 0) <= 0))
    {
        if(s->retries++ < MAX_HTTP_RETRIES)
            goto retry;
        else
        {
            ERROR("Failed to recv");
            status = EXIT_FAILURE;
            goto end;
        }
    }

    response->actualLen = (size_t) received;
    response->data[MAX_MESSAGE_SIZE] = 0; // add null character at end

    if((status = parseHeader(response)))
    {
        ERROR("Failed to parseHeader");
        goto end;
    }

    if(response->reconnect)
    {
        close(s->fd);
        s->closed = true;
    }

    if (response->contentLength > MAX_MESSAGE_SIZE) {
        // We stored MAX_MESSAGE_SIZE bytes into our buffer and the rest could not fit
        // The buffer contents will be malformed by an abrupt cut-off and will not be parseable
        status = EXIT_FAILURE;
        goto end;
    }

    // We successfully completed a request, so connection to server must be OK
    // Clear any errors so they are printed again if the reoccur
    mErrorFilter->clearErrors();

end:
    s->retries = 0;
    s->mutex.unlock();
    return status;

retry:
    close(s->fd);
    s->closed = true;
    s->mutex.unlock();
    return doRequest(request, response, timeout);
}

int RestAPI::parseHeader (response_t *response)
{
    int scanned;
    char *data = response->data;
    char *eol;

    response->contentLength = 0;
    response->reconnect = false;

    scanned = sscanf(data, "%*s %d", &response->code);
    if(scanned != 1)
        return EXIT_FAILURE;

    data = strstr(data, EOL);
    if(!data)
        return EXIT_FAILURE;

    data += EOL_LEN;

    eol = strstr(data, EOL);
    while(eol && data != eol)
    {
        char *key, *colon;

        key   = data;
        colon = strchr(data, ':');

        if(!colon)
            return EXIT_FAILURE;

        *colon = '\0';

        if(!strcasecmp(key, "content-length"))
            sscanf(colon + 1, "%lu", &response->contentLength);
        else if(!strcasecmp(key, "connection"))
        {
            char* value = new char[MAX_BUF_SIZE];
            sscanf(colon + 1, "%s", value);
            response->reconnect = !strcasecmp(value, "close");
            delete[] value;
        }

        data = eol + EOL_LEN;
        eol = strstr(data, EOL);
    }

    if(!eol)
        return EXIT_FAILURE;

    response->content = data + EOL_LEN;
    response->headerLen = response->content - response->data;

    return EXIT_SUCCESS;
}

int RestAPI::put (std::string subSystem, string const & param,
        string const & value,string * reply, int timeout)
{
    int status = basePut(subSystem, param, value.c_str(), value.length(), reply, timeout);
    return status;
}

int RestAPI::put(std::string subSystem, const std::string & param,
                 const std::string & key, const std::string & value,
                 std::string * reply, int timeout)
{

  char* valueBuf = new char[MAX_BUF_SIZE];

  JsonDict valueDict = JsonDict(key, value.c_str());

  int valueLen = epicsSnprintf(valueBuf, sizeof(valueBuf),
                               valueDict.str().c_str(), key.c_str(), value.c_str());

  int rc = basePut(subSystem, param, valueBuf, valueLen, reply, timeout);
  delete[] valueBuf;
  return rc;
}

int RestAPI::get(std::string subSystem, string const & param, string & value, int timeout)
{
    request_t request = {};
    char* requestBuf = new char[MAX_MESSAGE_SIZE];
    request.data = requestBuf;
    request.dataLen = MAX_MESSAGE_SIZE;
    request.actualLen = epicsSnprintf(request.data, request.dataLen,
            REQUEST_GET, subSystem.c_str(), param.c_str(), mHostname.c_str());

    response_t response = {};
    char* responseBuf = new char[MAX_MESSAGE_SIZE];
    response.data = responseBuf;
    response.dataLen = MAX_MESSAGE_SIZE;

    if(doRequest(&request, &response, timeout))
    {
        delete[] requestBuf;
        delete[] responseBuf;
        return EXIT_FAILURE;
    }

    if(response.code != 200)
    {
        delete[] requestBuf;
        delete[] responseBuf;
        return EXIT_FAILURE;
    }

    value = string(response.content, response.contentLength);
    delete[] requestBuf;
    delete[] responseBuf;
    return EXIT_SUCCESS;
}

int RestAPI::basePut(std::string & subSystem, const std::string & param,
                     const char * valueBuf, int valueLen, string * reply, int timeout)
{
  int headerLen;
  char* header = new char[MAX_BUF_SIZE];
  headerLen = epicsSnprintf(header, MAX_BUF_SIZE, REQUEST_PUT,
                            subSystem.c_str(), param.c_str(), mHostname.c_str(),
                            (size_t)valueLen);

  request_t request = {};
  char* requestBuf = new char[headerLen + valueLen];

  request.data      = requestBuf;
  request.dataLen   = headerLen + valueLen;
  request.actualLen = request.dataLen;

  response_t response = {};
  char* responseBuf = new char[MAX_MESSAGE_SIZE];

  response.data    = responseBuf;
  response.dataLen = MAX_MESSAGE_SIZE;

  memcpy(request.data, header, headerLen);
  memcpy(request.data + headerLen, valueBuf, valueLen);

  if(doRequest(&request, &response, timeout))
  {
    delete[] responseBuf;
    delete[] requestBuf;
    delete[] header;
    return EXIT_FAILURE;
  }

  if(response.code != 200)
  {
    delete[] responseBuf;
    delete[] requestBuf;
    delete[] header;
    return EXIT_FAILURE;
  }

  if(reply)
    *reply = string(response.content, response.contentLength);
  delete[] responseBuf;
  delete[] requestBuf;
  delete[] header;

  return EXIT_SUCCESS;
}

void RestAPI::setError(const char* functionName, std::string error)
{
  std::stringstream errorMessage;
  errorMessage << "RestAPI::" << functionName << ": " << error << "\n";

  if (mErrorFilter->newError(errorMessage.str())) {
    fprintf(stderr, errorMessage.str().c_str());
  }
}
