#ifndef REST_API_H
#define REST_API_H

#include <string>
#include <epicsMutex.h>
#include <osiSock.h>

#include "restDefinitions.h"

#define DEFAULT_TIMEOUT     20      // seconds

// Structure definitions
typedef struct socket
{
  SOCKET fd;
  epicsMutex mutex;
  bool closed;
  size_t retries;
} socket_t;

typedef struct request
{
  char *data;
  size_t dataLen, actualLen;
} request_t;

typedef struct response
{
  char *data;
  size_t dataLen, actualLen, headerLen;
  bool reconnect;
  char *content;
  size_t contentLength;
  int code;
} response_t;

class RestAPI
{
protected:
    std::string mHostname;
    int mPort;
    struct sockaddr_in mAddress;
    size_t mNumSockets;
    socket_t *mSockets;

    int connect (socket_t *s);
    int setNonBlock (socket_t *s, bool nonBlock);

    int doRequest (const request_t *request, response_t *response, int timeout = DEFAULT_TIMEOUT);
    int parseHeader (response_t *response);

public:
    static const std::string PARAM_VALUE;
    static const std::string PARAM_TYPE;
    static const std::string PARAM_MIN;
    static const std::string PARAM_MAX;
    static const std::string PARAM_ENUM_VALUES;
    static const std::string PARAM_ACCESS_MODE;
    static const std::string PARAM_CRITICAL_VALUES;

    RestAPI (std::string const & hostname, int port = 80, size_t numSockets=5);

    int get (std::string subSystem, std::string const & param, std::string & value, int timeout = DEFAULT_TIMEOUT);
    int put (std::string sys, std::string const & param, std::string const & value = "", std::string * reply = NULL, int timeout = DEFAULT_TIMEOUT);

    virtual int lookupAccessMode(
          std::string subSystem, rest_access_mode_t &accessMode) = 0;
};
#endif
