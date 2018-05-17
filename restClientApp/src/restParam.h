#ifndef REST_PARAM_H
#define REST_PARAM_H

#include <string>
#include <vector>
#include <map>
#include <asynPortDriver.h>
#include <frozen.h>
#include <stdlib.h>

#include "restDefinitions.h"
#include "restApi.h"
#include "errorFilter.h"

class RestParamSet;

class RestParam
{

private:
    ErrorFilter* mErrorFilter;
    RestParamSet *mSet;
    std::string mAsynName;
    asynParamType mAsynType;
    int mAsynIndex;
    std::string mSubSystem;
    std::string mName;
    bool mRemote;
    bool mPushAll;

    rest_param_type_t mType;
    rest_access_mode_t mAccessMode;
    rest_min_max_t mMin, mMax;
    std::vector <std::string> mEnumValues, mCriticalValues;
    double mEpsilon;
    int mTimeout;
    bool mCustomEnum;
    size_t mArraySize;

    bool mInitialised, mStrictInitialisation;
    std::vector<bool> mConnected;

    asynStatus bindAsynParam();

    std::vector<std::string> parseArray (struct json_token *tokens,
            std::string const & name = "");
    int parseType (struct json_token *tokens, rest_param_type_t & type);
    int parseAccessMode (struct json_token *tokens,
            rest_access_mode_t & accessMode);
    int parseMinMax (struct json_token *tokens, std::string const & key,
            rest_min_max_t & minMax);
    int initialise(struct json_token * tokens);

    int parseValue (struct json_token *tokens, std::string & rawValue);
    int parseValue (std::string const & rawValue, bool & value);
    int parseValue (std::string const & rawValue, int & value);
    int parseValue (std::string const & rawValue, double & value);

    std::string toString (bool value);
    std::string toString (int value);
    std::string toString (double value);
    std::string toString (std::string const & value);

    int getEnumIndex (std::string const & value, size_t & index);
    bool isCritical (std::string const & value);

    int getParam(int& value,               int address = 0);
    int getParam(double& value,            int address = 0);
    int getParam(std::string& value,       int address = 0);

    int setParam(int value,                int address = 0);
    int setParam(double value,             int address = 0);
    int setParam(const std::string& value, int address = 0);

    int setConnectedStatus(int status);
    int setConnectedStatus(std::vector<int> status);
    int setParamStatus(int status, int address = 0);

    int baseFetch (std::string & rawValue);
    int baseFetch(std::vector<std::string>& rawValue);
    int basePut (std::string const & rawValue, int index = -1);

    void setError(const char* functionName, std::string error, int index = -1);

public:
    // Asyn type constructor
    RestParam(RestParamSet *set, std::string const & asynName, asynParamType asynType,
              std::string subSystem = "", std::string const & name = "");
    // REST type constructor
    RestParam(RestParamSet * set, const std::string& asynName, rest_param_type_t restType,
              const std::string& subSystem = "", const std::string& name = "",
              size_t arraySize = 0, bool strict = false);

    void setCommand();
    void setEpsilon (double epsilon);
    void setTimeout(int timeout);
    int getIndex (void);
    std::string getName();
    void setEnumValues (std::vector<std::string> const & values);

    // Get the underlying asyn parameter value
    int get(bool& value,                      int address = 0);
    int get(std::vector<bool>& value);
    int get(int& value,                       int address = 0);
    int get(std::vector<int>& value);
    int get(double& value,                    int address = 0);
    int get(std::vector<double>& value);
    int get(std::string& value,               int address = 0);
    int get(std::vector<std::string>& value);

    // Fetch the current value from the device
    // Update underlying asyn parameter and return the value
    int fetch();
    int fetch(bool& value);
    std::vector<int> fetch(std::vector<bool>& value);
    int fetch(int& value);
    std::vector<int> fetch(std::vector<int>& value);
    int fetch(double& value);
    std::vector<int> fetch(std::vector<double>& value);
    int fetch(std::string & value);
    std::vector<int> fetch(std::vector<std::string>& value);

    void disablePushAll();
    bool canPushAll();

    // Re-send the current value to the device
    int push();

    // Put the value both to the device (if it is connected to a device
    // parameter) and to the underlying asyn parameter if successful. Update
    // other modified parameters automatically.
    int put (bool value,                int index = -1);
    int put (int value,                 int index = -1);
    int put (double value,              int index = -1);
    int put (std::string const & value, int index = -1);
    int put (const char *value,         int index = -1);
};

typedef std::map<std::string, RestParam*> rest_param_map_t;
typedef std::map<int, RestParam*> rest_asyn_map_t;

class RestParamSet
{
private:
    asynPortDriver *mPortDriver;
    RestAPI *mApi;
    asynUser *mUser;

    rest_param_map_t mConfigMap;
    rest_asyn_map_t mAsynMap;

public:
    RestParamSet (asynPortDriver *portDriver, RestAPI *api, asynUser *user);

    // Asyn type create
    RestParam * create(std::string const & asynName, asynParamType asynType,
                       std::string subSystem = "", std::string const & name = "");
    // REST type create
    RestParam * create(const std::string& asynName, rest_param_type_t restType,
                       const std::string& subSystem = "", const std::string& name = "",
                       size_t arraySize = 0, bool strict = false);

    void addToConfigMap(std::string const & name, RestParam *p);

    asynPortDriver *getPortDriver (void);
    RestAPI *getApi (void);
    RestParam *getByName (std::string const & name);
    RestParam *getByIndex (int index);
    asynUser *getUser (void);
    int fetchAll (void);
    int pushAll (void);

    int fetchParams (std::vector<std::string> const & params);
};
#endif
