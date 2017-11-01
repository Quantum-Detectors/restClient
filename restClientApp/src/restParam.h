#ifndef REST_PARAM_H
#define REST_PARAM_H

#include <string>
#include <vector>
#include <map>
#include <asynPortDriver.h>
#include <frozen.h>

#include "restDefinitions.h"
#include "restApi.h"

class RestParamSet;

class RestParam
{

private:
    RestParamSet *mSet;
    std::string mAsynName;
    asynParamType mAsynType;
    std::string mSubSystem;
    std::string mName;
    bool mRemote;

    int mAsynIndex;
    rest_param_type_t mType;
    rest_access_mode_t mAccessMode;
    rest_min_max_t mMin, mMax;
    std::vector <std::string> mEnumValues, mCriticalValues;
    double mEpsilon;
    bool mCustomEnum;

    std::vector<std::string> parseArray (struct json_token *tokens,
            std::string const & name = "");
    int parseType (struct json_token *tokens, rest_param_type_t & type);
    int parseAccessMode (struct json_token *tokens,
            rest_access_mode_t & accessMode);
    int parseMinMax (struct json_token *tokens, std::string const & key,
            rest_min_max_t & minMax);

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

    int getParam (int & value);
    int getParam (double & value);
    int getParam (std::string & value);

    int setParam (int value);
    int setParam (double value);
    int setParam (std::string const & value);

    int baseFetch (std::string & rawValue, int timeout = DEFAULT_TIMEOUT);
    int basePut (std::string const & rawValue, int timeout = DEFAULT_TIMEOUT);

public:
    RestParam (RestParamSet *set, std::string const & asynName,
            asynParamType asynType, std::string subSystem = "",
            std::string const & name = "");

    void setCommand();
    void setEpsilon (double epsilon);
    int getIndex (void);
    void setEnumValues (std::vector<std::string> const & values);

    // Get the underlying asyn parameter value
    int get (bool & value);
    int get (int & value);
    int get (double & value);
    int get (std::string & value);

    // Fetch the current value from the device
    // Update underlying asyn parameter and return the value
    int fetch (void);
    int fetch (bool & value,        int timeout = DEFAULT_TIMEOUT);
    int fetch (int & value,         int timeout = DEFAULT_TIMEOUT);
    int fetch (double & value,      int timeout = DEFAULT_TIMEOUT);
    int fetch (std::string & value, int timeout = DEFAULT_TIMEOUT);

    // Put the value both to the device (if it is connected to a device
    // parameter) and to the underlying asyn parameter if successful. Update
    // other modified parameters automatically.
    int put (bool value,                int timeout = DEFAULT_TIMEOUT);
    int put (int value,                 int timeout = DEFAULT_TIMEOUT);
    int put (double value,              int timeout = DEFAULT_TIMEOUT);
    int put (std::string const & value, int timeout = DEFAULT_TIMEOUT);
    int put (const char *value,         int timeout = DEFAULT_TIMEOUT);
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

    RestParam *create(std::string const & asynName, asynParamType asynType,
            std::string subSystem = "", std::string const & name = "");

    void addToConfigMap(std::string const & name, RestParam *p);

    asynPortDriver *getPortDriver (void);
    RestAPI *getApi (void);
    RestParam *getByName (std::string const & name);
    RestParam *getByIndex (int index);
    asynUser *getUser (void);
    int fetchAll (void);

    int fetchParams (std::vector<std::string> const & params);
};
#endif
