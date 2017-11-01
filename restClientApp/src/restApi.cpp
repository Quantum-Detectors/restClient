#include "restApi.h"

#include <stdexcept>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <frozen.h>     // JSON parser

#include <epicsStdio.h>
#include <epicsTime.h>

#include <fcntl.h>

#define EOL                     "\r\n"      // End of Line
#define EOL_LEN                 2           // End of Line Length
#define EOH                     EOL EOL     // End of Header
#define EOH_LEN                 (EOL_LEN*2) // End of Header Length

#define DATA_NATIVE             "application/json; charset=utf-8"
#define DATA_HTML               "text/html"

#define MAX_HTTP_RETRIES        1
#define MAX_MESSAGE_SIZE        512
#define MAX_BUF_SIZE            256
#define MAX_JSON_TOKENS         100

#define DEFAULT_TIMEOUT_CONNECT 1

#define ERR_PREFIX  "RestApi"
#define ERR(msg) fprintf(stderr, ERR_PREFIX "::%s: %s\n", functionName, msg)

#define ERR_ARGS(fmt,...) fprintf(stderr, ERR_PREFIX "::%s: " fmt "\n", \
    functionName, __VA_ARGS__)

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

using std::string;


RestAPI::RestAPI (string const & hostname, int port, size_t numSockets) :
    mHostname(hostname), mPort(port), mNumSockets(numSockets),
    mSockets(new socket_t[numSockets])
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

// Private members

int RestAPI::connect (socket_t *s)
{
    const char *functionName = "connect";

    if(!s->closed)
        return EXIT_SUCCESS;

    s->fd = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(s->fd == INVALID_SOCKET)
    {
        ERR("couldn't create socket");
        return EXIT_FAILURE;
    }

    setNonBlock(s, true);

    if(::connect(s->fd, (struct sockaddr*)&mAddress, sizeof(mAddress)) < 0)
    {
        // Connection actually failed
        if(errno != EINPROGRESS)
        {
            char error[MAX_BUF_SIZE];
            epicsSocketConvertErrnoToString(error, sizeof(error));
            ERR_ARGS("failed to connect to %s:%d [%s]", mHostname.c_str(),
                    mPort, error);
            epicsSocketDestroy(s->fd);
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

            ret = select(s->fd + 1, NULL, &set, NULL, &tv);
            if(ret <= 0)
            {
                const char *error = ret == 0 ? "TIMEOUT" : "select failed";
                ERR_ARGS("failed to connect to %s:%d [%s]", mHostname.c_str(),
                        mPort, error);
                epicsSocketDestroy(s->fd);
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
        ERR("no available socket");
        status = EXIT_FAILURE;
        goto end;
    }

    if(s->closed)
    {
        if(connect(s))
        {
            ERR("failed to reconnect socket");
            status = EXIT_FAILURE;
            goto end;
        }
    }

    if(send(s->fd, request->data, request->actualLen, 0) < 0)
    {
        if(s->retries++ < MAX_HTTP_RETRIES)
            goto retry;
        else
        {
            ERR("failed to send");
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
        ERR(ret ? "select() failed" : "timed out");
        status = EXIT_FAILURE;
        goto end;
    }

    if((received = recv(s->fd, response->data, response->dataLen, 0) <= 0))
    {
        if(s->retries++ < MAX_HTTP_RETRIES)
            goto retry;
        else
        {
            ERR("failed to recv");
            status = EXIT_FAILURE;
            goto end;
        }
    }

    response->actualLen = (size_t) received;

    if((status = parseHeader(response)))
    {
        ERR("failed to parseHeader");
        goto end;
    }

    if(response->reconnect)
    {
        close(s->fd);
        s->closed = true;
    }

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
            char value[MAX_BUF_SIZE];
            sscanf(colon + 1, "%s", value);
            response->reconnect = !strcasecmp(value, "close");
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
    const char *functionName = "put";

    int valueLen = 0;
    char valueBuf[MAX_BUF_SIZE] = "";
    if(!value.empty())
        valueLen = epicsSnprintf(valueBuf, sizeof(valueBuf), "{\"value\": %s}",
                value.c_str());

    int headerLen;
    char header[MAX_BUF_SIZE];
    headerLen = epicsSnprintf(header, sizeof(header), REQUEST_PUT,
            subSystem.c_str(), param.c_str(), mHostname.c_str(),
            (size_t)valueLen);

    request_t request = {};
    char requestBuf[headerLen + valueLen];
    request.data      = requestBuf;
    request.dataLen   = headerLen + valueLen;
    request.actualLen = request.dataLen;

    response_t response = {};
    char responseBuf[MAX_MESSAGE_SIZE];
    response.data    = responseBuf;
    response.dataLen = sizeof(responseBuf);

    memcpy(request.data, header, headerLen);
    memcpy(request.data + headerLen, valueBuf, valueLen);

    if(doRequest(&request, &response, timeout))
    {
        ERR_ARGS("[param=%s] request failed", param.c_str());
        return EXIT_FAILURE;
    }

    if(response.code != 200)
    {
        ERR_ARGS("[param=%s] server returned error code %d",
                param.c_str(), response.code);
        return EXIT_FAILURE;
    }

    if(reply)
        *reply = string(response.content, response.contentLength);
    return EXIT_SUCCESS;
}

int RestAPI::get (std::string subSystem, string const & param, string & value, int timeout)
{
    const char *functionName = "get";

    request_t request = {};
    char requestBuf[MAX_MESSAGE_SIZE];
    request.data      = requestBuf;
    request.dataLen   = sizeof(requestBuf);
    request.actualLen = epicsSnprintf(request.data, request.dataLen,
            REQUEST_GET, subSystem.c_str(), param.c_str(), mHostname.c_str());

    response_t response = {};
    char responseBuf[MAX_MESSAGE_SIZE];
    response.data    = responseBuf;
    response.dataLen = sizeof(responseBuf);

    if(doRequest(&request, &response, timeout))
    {
        ERR_ARGS("[param=%s] request failed", param.c_str());
        return EXIT_FAILURE;
    }

    if(response.code != 200)
    {
        ERR_ARGS("[param=%s] server returned error code %d",
                param.c_str(), response.code);
        return EXIT_FAILURE;
    }

    value = string(response.content, response.contentLength);
    return EXIT_SUCCESS;
}
