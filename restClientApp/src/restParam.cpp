#include <stdlib.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <sstream>

#include <frozen.h>
#include <math.h>
#include "restParam.h"

// Error message formatters
#define ERR(msg) asynPrint(mSet->getUser(), ASYN_TRACE_ERROR,\
    "Param[%s]::%s: %s\n", \
    mAsynName.c_str(), functionName, msg)

#define ERR_ARGS(fmt,...) asynPrint(mSet->getUser(), ASYN_TRACE_ERROR, \
    "Param[%s]::%s: "fmt"\n", mAsynName.c_str(), functionName, __VA_ARGS__);

// Flow message formatters
#define FLOW(msg) asynPrint(mSet->getUser(), ASYN_TRACE_FLOW, \
    "Param[%s]::%s: %s\n", \
    mAsynName.c_str(), functionName, msg)

#define FLOW_ARGS(fmt,...) asynPrint(mSet->getUser(), ASYN_TRACE_FLOW, \
    "Param[%s]::%s: "fmt"\n", mAsynName.c_str(), functionName, __VA_ARGS__);


#define MAX_BUFFER_SIZE 128
#define MAX_MESSAGE_SIZE 512
#define MAX_JSON_TOKENS 100

using std::string;
using std::vector;
using std::map;
using std::pair;

vector<string> RestParam::parseArray (struct json_token *tokens,
        string const & name)
{
    vector<string> arrayValues;
    struct json_token *t;
    if(name.empty())
        t = tokens;
    else
        t = find_json_token(tokens, name.c_str());

    if(t)
    {
        if(t->type == JSON_TYPE_ARRAY)
        {
            arrayValues.reserve(t->num_desc);
            for(int i = 1; i <= t->num_desc; ++i)
                arrayValues.push_back(string((t+i)->ptr, (t+i)->len));
        }
        else if(t->type == JSON_TYPE_STRING)
            arrayValues.push_back(string(t->ptr, t->len));
    }
    return arrayValues;
}

int RestParam::parseType (struct json_token *tokens, rest_param_type_t & type)
{
    const char *functionName = "parseType";

    // Find value type
    std::string key = mSet->getApi()->PARAM_TYPE;
    struct json_token *token = find_json_token(tokens, key.c_str());
    if(token == NULL)
    {
        ERR_ARGS("unable to find '%s' json field", key.c_str());
        return EXIT_FAILURE;
    }
    string typeStr(token->ptr, token->len);

    // Check if this parameter is an enumeration
    token = find_json_token(tokens, mSet->getApi()->PARAM_ENUM_VALUES.c_str());
    if(token)
        typeStr = "enum";

    // Check if this parameter is write only (command)
    token = find_json_token(tokens, mSet->getApi()->PARAM_ACCESS_MODE.c_str());
    if(token && token->ptr[0] == 'w')
        typeStr = "command";

    // Map type to asynParamType
    switch(typeStr[0])
    {
    case 's': type = REST_P_STRING;  break;
    case 'f': type = REST_P_DOUBLE;  break;
    case 'b': type = REST_P_BOOL;    break;
    case 'u': type = REST_P_UINT;    break;
    case 'i': type = REST_P_INT;     break;
    case 'e': type = REST_P_ENUM;    break;
    case 'c': type = REST_P_COMMAND; break;
    default:
        ERR_ARGS("unrecognized value type '%s'", typeStr.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::parseAccessMode (struct json_token *tokens,
        rest_access_mode_t & accessMode)
{
    const char *functionName = "parseAccessMode";

    struct json_token *t = find_json_token(tokens, mSet->getApi()->PARAM_ACCESS_MODE.c_str());
    if(!t)
        return EXIT_FAILURE;

    string accessModeStr(t->ptr, t->len);
    if(accessModeStr == "r")
        accessMode = REST_ACC_RO;
    else if(accessModeStr == "w")
        accessMode = REST_ACC_WO;
    else if(accessModeStr == "rw")
        accessMode = REST_ACC_RW;
    else
    {
        ERR_ARGS("invalid access mode '%s", accessModeStr.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RestParam::parseMinMax (struct json_token *tokens, string const & key,
        rest_min_max_t & minMax)
{
    const char *functionName = "parseMinMax";

    struct json_token *t = find_json_token(tokens, key.c_str());

    if((minMax.exists = (t != NULL)))
    {
        struct json_token *type = find_json_token(tokens, mSet->getApi()->PARAM_TYPE.c_str());
        if(!type)
        {
            ERR("failed to find 'value_type'");
            return EXIT_FAILURE;
        }

        string value(t->ptr, t->len);
        if(type->ptr[0] == 'i' || type->ptr[0] == 'u')
        {
            if(sscanf(value.c_str(), "%d", &minMax.valInt) != 1)
            {
                ERR_ARGS("failed to parse '%s' as integer", value.c_str());
                return EXIT_FAILURE;
            }
        }
        else if(type->ptr[0] == 'f')
        {
            if(sscanf(value.c_str(), "%lf", &minMax.valDouble) != 1)
            {
                ERR_ARGS("failed to parse '%s' as double", value.c_str());
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

int RestParam::initialise(struct json_token * tokens)
{
  const char *functionName = "initialise";

  mInitialised = false;

  if (mSet->getApi()->lookupAccessMode(mSubSystem, mAccessMode)) {
    if (parseAccessMode(tokens, mAccessMode)) {
      mAccessMode = REST_ACC_RO;
    }
  }

  if (mType == REST_P_UNINIT && parseType(tokens, mType)) {
    ERR_ARGS("[param=%s] unable to parse parameter type\n", mName.c_str());
    return EXIT_FAILURE;
  }

  if (mType == REST_P_ENUM) {
    if (!mCustomEnum) {
      mEnumValues = parseArray(tokens, mSet->getApi()->PARAM_ENUM_VALUES);
    }
  }

  if (mStrictInitialisation) {

    mCriticalValues = parseArray(tokens, mSet->getApi()->PARAM_CRITICAL_VALUES);

    if(mType == REST_P_INT || mType == REST_P_UINT || mType == REST_P_DOUBLE) {
      if(parseMinMax(tokens, mSet->getApi()->PARAM_MIN, mMin)) {
        ERR_ARGS("[param=%s] unable to parse min limit\n", mName.c_str());
        return EXIT_FAILURE;
      }

      if(parseMinMax(tokens, mSet->getApi()->PARAM_MAX, mMax)) {
        ERR_ARGS("[param=%s] unable to parse max limit\n", mName.c_str());
        return EXIT_FAILURE;
      }
    }
    else if (mType == REST_P_ENUM) {
      mMin.exists = true;
      mMax.exists = true;
      mMin.valInt = 0;
      mMax.valInt = (int) (mEnumValues.size() - 1);
    }
  }

  mInitialised = true;
  return EXIT_SUCCESS;
}

int RestParam::parseValue (struct json_token *tokens, string & rawValue)
{
    const char *functionName = "parseValue";

    std::string key;
    if (!mSet->getApi()->PARAM_VALUE.empty()) {
        key = mSet->getApi()->PARAM_VALUE;
    }
    else {
        key = mName;
    }
    struct json_token *token = find_json_token(tokens, key.c_str());
    if(token == NULL)
    {
        ERR_ARGS("unable to find '%s' json field", key.c_str());
        return EXIT_FAILURE;
    }
    rawValue = string(token->ptr, token->len);
    return EXIT_SUCCESS;
}

int RestParam::parseValue (string const & rawValue, bool & value)
{
    const char *functionName = "parseValue";

    if(rawValue == "true")
        value = true;
    else if(rawValue == "false")
        value = false;
    else
    {
        ERR_ARGS("couldn't parse value '%s' as boolean", rawValue.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::parseValue (string const & rawValue, int & value)
{
    const char *functionName = "parseValue";

    if(sscanf(rawValue.c_str(), "%d", &value) != 1)
    {
        ERR_ARGS("couldn't parse value '%s' as integer", rawValue.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::parseValue (string const & rawValue, double & value)
{
    const char *functionName = "parseValue";

    if(sscanf(rawValue.c_str(), "%lf", &value) != 1)
    {
        ERR_ARGS("couldn't parse value '%s' as double", rawValue.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

std::string RestParam::toString (bool value)
{
    if(mType == REST_P_ENUM)
        return toString(mEnumValues[(int) (!value)]);

    return value ? "true" : "false";
}

std::string RestParam::toString (int value)
{
    if(mType == REST_P_ENUM)
        return toString(mEnumValues[value]);

    std::ostringstream os;
    os << value;
    return os.str();
}

std::string RestParam::toString (double value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

std::string RestParam::toString (std::string const & value)
{
    std::ostringstream os;
    os << "\"" << value << "\"";
    return os.str();
}

int RestParam::getEnumIndex (std::string const & value, size_t & index)
{
    const char *functionName = "getEnumIndex";
    for(index = 0; index < mEnumValues.size(); ++index)
        if(mEnumValues[index] == value)
            return EXIT_SUCCESS;

    ERR_ARGS("[param=%s] can't find index of value %s", mAsynName.c_str(),
            value.c_str());
    return EXIT_FAILURE;
}

bool RestParam::isCritical (std::string const & value)
{
    size_t i;
    for(i = 0; i < mCriticalValues.size(); ++i)
        if(mCriticalValues[i] == value)
            return true;
    return false;
}

int RestParam::getParam(int& value, int address)
{
    return (int) mSet->getPortDriver()->getIntegerParam(address, mAsynIndex, &value);
}

int RestParam::getParam(double& value, int address)
{
    return (int) mSet->getPortDriver()->getDoubleParam(address, mAsynIndex, &value);
}

int RestParam::getParam(std::string& value, int address)
{
    return (int) mSet->getPortDriver()->getStringParam(address, mAsynIndex, value);
}

int RestParam::setParam(int value, int address)
{
    return (int) mSet->getPortDriver()->setIntegerParam(address, mAsynIndex, value);
}

int RestParam::setParam(double value, int address)
{
    return (int) mSet->getPortDriver()->setDoubleParam(address, mAsynIndex, value);
}

int RestParam::setParam(const std::string& value, int address)
{
    return (int) mSet->getPortDriver()->setStringParam(address, mAsynIndex, value);
}

RestParam::RestParam(RestParamSet *set, std::string const & asynName, asynParamType asynType,
                     std::string subSystem, std::string const & name)
    : mSet(set),
      mAsynName(asynName), mAsynType(asynType), mAsynIndex(-1),
      mSubSystem(subSystem), mName(name), mRemote(!mName.empty()),
      mAccessMode(), mMin(), mMax(), mEnumValues(), mCriticalValues(), mEpsilon(0.0),
      mCustomEnum(false)
{
    const char *functionName = "RestParam<asynType>";

    if(mName.empty())
    {
        switch(asynType)
        {
        case asynParamInt32:   mType = REST_P_INT;    break;
        case asynParamFloat64: mType = REST_P_DOUBLE; break;
        case asynParamOctet:   mType = REST_P_STRING; break;
        default:
            ERR_ARGS("[param=%s] invalid asyn type %d", mAsynName.c_str(), (int)asynType);
            throw std::runtime_error(mAsynName);
        }
    }

  bindAsynParam();
}

RestParam::RestParam(RestParamSet * set, const std::string& asynName, rest_param_type_t restType,
                     const std::string& subSystem, const std::string& name, size_t arraySize,
                     bool strict)
    : mSet(set),
      mAsynName(asynName), mAsynType(asynParamNotDefined), mAsynIndex(-1),
      mSubSystem(subSystem), mName(name), mRemote(!mName.empty()), mType(restType),
      mAccessMode(), mMin(), mMax(), mEnumValues(), mCriticalValues(), mEpsilon(0.0),
      mCustomEnum(false), mArraySize(arraySize), mInitialised(false), mStrictInitialisation(strict)
{
    const char *functionName = "RestParam<restType>";

    switch(restType)
    {
        case REST_P_INT:
        case REST_P_UINT:
        case REST_P_BOOL:
        case REST_P_ENUM:
        case REST_P_COMMAND:
            mAsynType = asynParamInt32;
            break;
        case REST_P_DOUBLE:
            mAsynType = asynParamFloat64;
            break;
        case REST_P_STRING:
            mAsynType = asynParamOctet;
            break;
        default:  // Includes REST_P_UNINIT
            ERR_ARGS("[param=%s] invalid REST type %d", mAsynName.c_str(), (int)restType);
            throw std::runtime_error(mAsynName);
    }

    bindAsynParam();
}

asynStatus RestParam::bindAsynParam()
{
    const char *functionName = "bindAsynParam";
    asynStatus status = asynSuccess;
    // Check if asyn parameter already exists, create if it doesn't
    if(mSet->getPortDriver()->findParam(mAsynName.c_str(), &mAsynIndex)) {
        FLOW_ARGS("[param=%s] creating param", mAsynName.c_str());
        status = mSet->getPortDriver()->createParam(mAsynName.c_str(), mAsynType, &mAsynIndex);
        if(status) {
            ERR_ARGS("[param=%s] failed to create param", mAsynName.c_str());
            throw std::runtime_error(mAsynName);
        }
    }
    else {
        RestParam* param = mSet->getByIndex(mAsynIndex);
        if (param != NULL) {
            ERR_ARGS("[param=%s] param is already bound to %s",
                     mAsynName.c_str(), param->mName.c_str());
            throw std::runtime_error(mAsynName);
        }
        else {
            FLOW_ARGS("[param=%s] binding to existing param", mAsynName.c_str());
        }
    }
    return status;
}

void RestParam::setCommand()
{
    mType = REST_P_COMMAND;
    mAccessMode = REST_ACC_WO;
}

void RestParam::setEpsilon (double epsilon)
{
    mEpsilon = epsilon;
}

int RestParam::getIndex (void)
{
    return mAsynIndex;
}

std::string RestParam::getName()
{
  return mName;
}

void RestParam::setEnumValues (vector<string> const & values)
{
    mEnumValues = values;
    mCustomEnum = true;
}

int RestParam::get(bool& value, int address)
{
    if(mAsynType == asynParamInt32)
    {
        int tempValue;
        getParam(tempValue, address);
        value = (bool) tempValue;
    }
    else if(mAsynType == asynParamOctet && mEnumValues.size() == 2)
    {
        int tempValue;
        if(get(tempValue, address))
            return EXIT_FAILURE;
        value = (bool) tempValue;
    }
    else
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int RestParam::get(std::vector<bool>& value)
{
  int status = 0;
  value.resize(mArraySize);
  for (int index = 0; (size_t) index != value.size(); ++index) {
    int temp;
    status |= getParam(temp, index);
    value[index] = (bool) temp;
  }
  return status;
}

int RestParam::get(int& value, int address)
{
    if(mAsynType == asynParamInt32)
        getParam(value, address);
    else if(mAsynType == asynParamOctet && mEnumValues.size())
    {
        string tempValue;
        getParam(tempValue, address);

        size_t index;
        if(getEnumIndex(tempValue, index))
            return EXIT_FAILURE;

        value = (int) index;
    }
    else
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int RestParam::get(std::vector<int>& value)
{
  int status = 0;
  value.resize(mArraySize);
  for (int index = 0; (size_t) index != value.size(); ++index) {
    status |= getParam(value[index], index);
  }
  return status;
}

int RestParam::get(double& value, int address)
{
    return (int) getParam(value, address);
}

int RestParam::get(std::vector<double>& value)
{
  int status = 0;
  value.resize(mArraySize);
  for (int index = 0; (size_t) index != value.size(); ++index) {
    status |= getParam(value[index], index);
  }
  return status;
}

int RestParam::get(std::string& value, int address)
{
    if(mType == REST_P_ENUM && mAsynType == asynParamInt32)
    {
        int index;
        int status = getParam(index, address);
        value = mEnumValues[index];
        return status;
    }

    return (int) getParam(value, address);
}

int RestParam::get(std::vector<std::string>& value)
{
  int status = 0;
  value.resize(mArraySize);
  for (int index = 0; (size_t) index != value.size(); ++index) {
    status |= getParam(value[index], index);
  }
  return status;
}

int RestParam::baseFetch (string & rawValue, int timeout)
{
    const char *functionName = "baseFetch";
    if(!mRemote)
    {
        ERR_ARGS("[param=%s] can't fetch local parameter", mAsynName.c_str());
        return EXIT_FAILURE;
    }

    if(mAccessMode == REST_ACC_WO)
        return EXIT_SUCCESS;

    string buffer;
    mSet->getApi()->get(mSubSystem, mName, buffer, timeout);

    // Parse JSON
    struct json_token tokens[MAX_JSON_TOKENS];
    int err = parse_json(buffer.c_str(), buffer.size(), tokens, MAX_JSON_TOKENS);
    if(err < 0)
    {
        const char *msg = "unable to parse json response";
        ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
        return EXIT_FAILURE;
    }

    if (!mInitialised) {
        if (initialise(tokens)) {
            const char *msg = "unable to initialise param from response";
            ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
            return EXIT_FAILURE;
        }
    }

    if(parseValue(tokens, rawValue))
    {
        const char *msg = "unable to parse raw value";
        ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
        return EXIT_FAILURE;
    }

    FLOW_ARGS("%s", rawValue.c_str());
    return EXIT_SUCCESS;
}

int RestParam::baseFetch(std::vector<std::string>& rawValue, int timeout)
{
    const char *functionName = "baseFetch<vector>";
    if(!mRemote)
    {
        ERR_ARGS("[param=%s] can't fetch local parameter", mAsynName.c_str());
        return EXIT_FAILURE;
    }

    if(mAccessMode == REST_ACC_WO)
        return EXIT_SUCCESS;

    std::string buffer;
    mSet->getApi()->get(mSubSystem, mName, buffer, timeout);

    // Parse JSON
    struct json_token tokens[MAX_JSON_TOKENS];
    int err = parse_json(buffer.c_str(), buffer.size(), tokens, MAX_JSON_TOKENS);
    if(err < 0)
    {
        const char *msg = "unable to parse json response";
        ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
        return EXIT_FAILURE;
    }

    if (!mInitialised) {
        if (initialise(tokens)) {
            const char *msg = "unable to initialise param from response";
            ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
            return EXIT_FAILURE;
        }
    }

    std::vector<std::string> valueArray = parseArray(tokens, mSet->getApi()->PARAM_VALUE);
    if(valueArray.empty())
    {
        const char *msg = "unable to parse raw value array";
        ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, buffer.c_str());
        return EXIT_FAILURE;
    }

    rawValue.resize(valueArray.size(), "");
    for (int index = 0; (size_t) index != valueArray.size(); ++index) {
        rawValue[index] = valueArray[index];
    }

    return EXIT_SUCCESS;
}

int RestParam::fetch (bool & value, int timeout)
{
    const char *functionName = "fetch<bool>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        string rawValue;
        if(baseFetch(rawValue))
        {
            ERR_ARGS("[param=%s] underlying baseFetch failed",
                    mAsynName.c_str());
            return EXIT_FAILURE;

        }
        if(mType == REST_P_BOOL)
        {
            if(parseValue(rawValue, value))
                return EXIT_FAILURE;
        }
        else if(mType == REST_P_ENUM)
        {
            if(mEnumValues.size() != 2)
            {
                ERR_ARGS("[param=%s] can't fetch non-binary enum as bool\n",
                        mAsynName.c_str());
                return EXIT_FAILURE;
            }

            size_t index;
            if(getEnumIndex(rawValue, index))
                return EXIT_FAILURE;

            value = (bool) index;
            if(setParam((int) index))
            {
                ERR_ARGS("[param=%s] failed to set asyn parameter",
                        mAsynName.c_str());
                return EXIT_FAILURE;
            }
        }
        else
        {
            ERR_ARGS("[param=%s] unexpected type %d", mAsynName.c_str(),
                    mType);
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%d", value);
    return get(value);
}

int RestParam::fetch(std::vector<bool>& value, int timeout)
{
    const char *functionName = "fetch<vector<bool>>";
    if(mRemote && mType != REST_P_COMMAND) {
        std::vector<std::string> rawValue;
        if (baseFetch(rawValue)) {
            ERR_ARGS("[param=%s] underlying baseFetch failed", mAsynName.c_str());
            return EXIT_FAILURE;
        }

        value.resize(mArraySize);
        for (int index = 0; (size_t) index != rawValue.size(); ++index) {
            if (mType == REST_P_BOOL) {
                bool temp;
                if (parseValue(rawValue[index], temp))
                    return EXIT_FAILURE;
                value[index] = temp;
            }
            else if(mType == REST_P_ENUM) {
                if(mEnumValues.size() != 2) {
                    ERR_ARGS("[param=%s] can't fetch non-binary enum as bool\n", mAsynName.c_str());
                    return EXIT_FAILURE;
                }

                size_t eIndex;
                if (getEnumIndex(rawValue[index], eIndex))
                    return EXIT_FAILURE;

                value[index] = (bool) eIndex;
                if (setParam((int) eIndex)) {
                    ERR_ARGS("[param=%s] failed to set asyn parameter", mAsynName.c_str());
                    return EXIT_FAILURE;
                }
            }
            else {
                ERR_ARGS("[param=%s] unexpected type %d", mAsynName.c_str(), mType);
                return EXIT_FAILURE;
            }
            FLOW_ARGS("%d", (bool) value[index]);
        }
    }
    return EXIT_SUCCESS;
}

int RestParam::fetch (int & value, int timeout)
{
    const char *functionName = "fetch<int>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        string rawValue;
        if(baseFetch(rawValue))
        {
            ERR_ARGS("[param=%s] underlying baseFetch failed",
                    mAsynName.c_str());
            return EXIT_FAILURE;

        }

        if(mType == REST_P_ENUM)
        {
            size_t index;
            if(getEnumIndex(rawValue, index))
                return EXIT_FAILURE;

            value = (int) index;
        }
        else if(mType == REST_P_BOOL)
        {
            bool tempValue;
            if(parseValue(rawValue, tempValue))
                return EXIT_FAILURE;
            value = (int) tempValue;
        }
        else if(mType == REST_P_INT || mType == REST_P_UINT)
        {
            if(parseValue(rawValue, value))
                return EXIT_FAILURE;
        }
        else
        {
            ERR_ARGS("[param=%s] unexpected type %d", mAsynName.c_str(),
                    mType);
            return EXIT_FAILURE;
        }

        if(setParam(value))
        {
            ERR_ARGS("[param=%s] failed to set asyn parameter",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%d", value);
    return get(value);
}

int RestParam::fetch(std::vector<int>& value, int timeout)
{
    const char *functionName = "fetch<vector<int>>";
    if (mRemote && mType != REST_P_COMMAND) {
        std::vector<std::string> rawValue;
        if (baseFetch(rawValue)) {
            ERR_ARGS("[param=%s] underlying baseFetch failed", mAsynName.c_str());
            return EXIT_FAILURE;
        }

        value.resize(mArraySize);
        for (int index = 0; (size_t) index != rawValue.size(); ++index) {
            if (mType == REST_P_ENUM) {
                size_t eIndex;
                if (getEnumIndex(rawValue[index], eIndex)) {
                    return EXIT_FAILURE;
                }

                value[index] = (int) eIndex;
            }
            else if (mType == REST_P_BOOL) {
                bool tempValue;
                if (parseValue(rawValue[index], tempValue)) {
                    return EXIT_FAILURE;
                }
                value[index] = (int) tempValue;
            }
            else if (mType == REST_P_INT || mType == REST_P_UINT) {
                if (parseValue(rawValue[index], value[index]))
                    return EXIT_FAILURE;
            }
            else {
                ERR_ARGS("[param=%s[%d]] unexpected type %d", mAsynName.c_str(), index, mType);
                return EXIT_FAILURE;
            }

            if (setParam(value[index], index)) {
                ERR_ARGS("[param=%s[%d]] failed to set asyn parameter", mAsynName.c_str(), index);
                return EXIT_FAILURE;
            }
            ERR_ARGS("%d", value[index]);
        }
    }
    return get(value);
}

int RestParam::fetch (double & value, int timeout)
{
    const char *functionName = "fetch<double>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        if(mType != REST_P_DOUBLE && mType != REST_P_UNINIT)
        {
            ERR_ARGS("[param=%s] unexpected type %d", mAsynName.c_str(),
                    mType);
            return EXIT_FAILURE;
        }

        string rawValue;
        if(baseFetch(rawValue))
        {
            ERR_ARGS("[param=%s] underlying baseFetch failed",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }

        if(parseValue(rawValue, value))
            return EXIT_FAILURE;

        if(setParam(value))
        {
            ERR_ARGS("[param=%s] failed to set asyn parameter",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%lf", value);
    return get(value);
}

int RestParam::fetch(std::vector<double>& value, int timeout)
{
    const char *functionName = "fetch<double>";

    value.resize(mArraySize);
    if(mRemote && mType != REST_P_COMMAND) {
        if(mType != REST_P_DOUBLE && mType != REST_P_UNINIT) {
            ERR_ARGS("[param=%s] unexpected type %d", mAsynName.c_str(), mType);
            return EXIT_FAILURE;
        }

        std::vector<std::string> rawValue;
        if(baseFetch(rawValue)) {
            ERR_ARGS("[param=%s] underlying baseFetch failed", mAsynName.c_str());
            return EXIT_FAILURE;
        }

        for (size_t index = 0; index != rawValue.size(); ++index) {
            if (parseValue(rawValue[index], value[index])) {
                return EXIT_FAILURE;
            }

            if (setParam(value[index], index)) {
                ERR_ARGS("[param=%s[%zu]] failed to set asyn parameter", mAsynName.c_str(), index);
                return EXIT_FAILURE;
            }
            FLOW_ARGS("%lf", value[index]);
        }
    }
    return get(value);
}

int RestParam::fetch (string & value, int timeout)
{
    const char *functionName = "fetch<string>";

    if(mRemote && mType != REST_P_COMMAND)
    {
        if(mType != REST_P_STRING && mType != REST_P_ENUM &&
           mType != REST_P_UNINIT)
            return EXIT_FAILURE;

        if(baseFetch(value))
        {
            ERR_ARGS("[param=%s] underlying baseFetch failed",
                    mAsynName.c_str());
            return EXIT_FAILURE;

        }

        // TODO: check if it is critical

        if(setParam(value))
        {
            ERR_ARGS("[param=%s] failed to set asyn parameter",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    FLOW_ARGS("%s", value.c_str());
    return get(value);
}

int RestParam::fetch(std::vector<std::string>& value, int timeout)
{
    const char *functionName = "fetch<vector<string>>";

    if(mRemote && mType != REST_P_COMMAND) {
        if(mType != REST_P_STRING && mType != REST_P_ENUM && mType != REST_P_UNINIT) {
            return EXIT_FAILURE;
        }

        if(baseFetch(value)) {
            ERR_ARGS("[param=%s] underlying baseFetch failed", mAsynName.c_str());
            return EXIT_FAILURE;
        }

        for (size_t index = 0; index != value.size(); ++index) {
            if (setParam(value[index], index)) {
                ERR_ARGS("[param=%s[%zu] failed to set asyn parameter", mAsynName.c_str(), index);
                return EXIT_FAILURE;
            }
            FLOW_ARGS("%s", value[index].c_str());
        }
        return EXIT_SUCCESS;
    }
    return get(value);
}

int RestParam::fetch()
{
    switch(mAsynType)
    {
    case asynParamInt32:
    {
        if (mArraySize) {
            std::vector<int> dummy;
            return fetch(dummy);
        }
        else {
            int dummy;
            return fetch(dummy);
        }
    }
    case asynParamFloat64:
    {
        if (mArraySize) {
            std::vector<double> dummy;
            return fetch(dummy);
        }
        else {
            double dummy;
            return fetch(dummy);
        }
    }
    case asynParamOctet:
    {
        if (mArraySize) {
            std::vector<std::string> dummy;
            return fetch(dummy);
        }
        else {
            std::string dummy;
            return fetch(dummy);
        }
    }
    default:
        break;
    }
    return EXIT_SUCCESS;
}

int RestParam::basePut (const std::string & rawValue, int timeout)
{
    const char *functionName = "basePut";
    FLOW_ARGS("'%s'", rawValue.c_str());
    if(mAccessMode == REST_ACC_RO)
    {
        ERR_ARGS("[param=%s] can't write to read-only parameter", mAsynName.c_str());
        return EXIT_FAILURE;
    }

    string reply;
    if(mSet->getApi()->put(mSubSystem, mName, rawValue, &reply, timeout))
    {
        ERR_ARGS("[param=%s] underlying RestAPI put failed", mAsynName.c_str());
        return EXIT_FAILURE;
    }

    // Parse JSON
    if(!reply.empty())
    {
        struct json_token tokens[MAX_JSON_TOKENS];
        int err = parse_json(reply.c_str(), reply.size(), tokens, MAX_JSON_TOKENS);
        if(err < 0)
        {
            const char *msg = "unable to parse json response";
            ERR_ARGS("[param=%s] %s\n[%s]", mName.c_str(), msg, reply.c_str());
            return EXIT_FAILURE;
        }

        mSet->fetchParams(parseArray(tokens));
    }
    return EXIT_SUCCESS;
}

int RestParam::put (bool value, int timeout)
{
    const char *functionName = "put<bool>";
    FLOW_ARGS("%d", value);
    if(!mRemote)
    {
        setParam((int)value);
        return EXIT_SUCCESS;
    }

    if (!mInitialised && fetch()) {
        return EXIT_FAILURE;
    }

    if(mType != REST_P_BOOL && mType != REST_P_ENUM)
        return EXIT_FAILURE;

    int status;
    if(mType == REST_P_ENUM)
    {
        if(mEnumValues.size() != 2)
            return EXIT_FAILURE;

        // XOR with mReversedEnum
        status = basePut(toString(value), timeout);
    }
    else
        status = basePut(toString(value), timeout);

    if(status)
        return EXIT_FAILURE;

    if(setParam(value))
    {
        ERR_ARGS("[param=%s] failed to set asyn parameter",
                mAsynName.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RestParam::put (int value, int timeout)
{
    const char *functionName = "put<int>";
    int status;
    FLOW_ARGS("%d", value);
    if(mRemote)
    {
        if (!mInitialised && fetch()) {
            return EXIT_FAILURE;
        }

        if(mType != REST_P_BOOL && mType != REST_P_INT &&
           mType != REST_P_UINT && mType != REST_P_ENUM &&
           mType != REST_P_COMMAND)
        {
            ERR_ARGS("[param=%s] expected bool, int, uint or enum",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }

        // Clamp value
        if(mMin.exists && value < mMin.valInt)
            value = mMin.valInt;

        if(mMax.exists && value > mMax.valInt)
            value = mMax.valInt;

        // Protect against trying to write negative values to an unsigned int
        if(mType == REST_P_UINT && value < 0)
            value = 0;

        if(mType == REST_P_BOOL)
            status = basePut(toString((bool)value), timeout);
        else
            status = basePut(toString(value), timeout);

        if(status)
        {
            ERR_ARGS("[param=%s] underlying basePut failed",
                    mAsynName.c_str());
            return EXIT_FAILURE;
        }
    }

    if(mAsynType == asynParamInt32)
        status = setParam(value);
    else
        status = setParam(mEnumValues[value]);

    if(status)
    {
        ERR_ARGS("[param=%s] failed to set asyn parameter",
                mAsynName.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::put (double value, int timeout)
{
    const char *functionName = "put<double>";
    FLOW_ARGS("%lf", value);
    if(mEpsilon)
    {
        double currentValue;
        getParam(currentValue);
        if(fabs(currentValue - value) < mEpsilon)
            return EXIT_SUCCESS;
    }

    if(mRemote)
    {
        if (!mInitialised && fetch()) {
            return EXIT_FAILURE;
        }

        if(mType != REST_P_DOUBLE)
            return EXIT_FAILURE;

        // Clamp value
        if(mMin.exists && (value < mMin.valDouble))
        {
            value = mMin.valDouble;
            ERR_ARGS("clamped to min %lf", value);
        }

        if(mMax.exists && (value > mMax.valDouble))
        {
            value = mMax.valDouble;
            ERR_ARGS("clamped to max %lf", value);
        }

        if(basePut(toString(value), timeout))
            return EXIT_FAILURE;
    }

    if(setParam(value))
    {
        ERR_ARGS("[param=%s] failed to set asyn parameter",
                mAsynName.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::put (string const & value, int timeout)
{
    const char *functionName = "put<string>";
    FLOW_ARGS("%s", value.c_str());
    if(!mRemote)
    {
        setParam(value);
        return EXIT_SUCCESS;
    }

    if (!mInitialised && fetch()) {
        return EXIT_FAILURE;
    }

    if(mType != REST_P_STRING && mType != REST_P_ENUM)
        return EXIT_FAILURE;

    size_t index;
    if(mType == REST_P_ENUM && getEnumIndex(value, index))
        return EXIT_FAILURE;

    if(basePut(toString(value), timeout))
        return EXIT_FAILURE;

    int status;
    if(mAsynType == asynParamInt32)
        status = setParam((int)index);
    else
        status = setParam(value);

    if(status)
    {
        ERR_ARGS("[param=%s] failed to set asyn parameter",
                mAsynName.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RestParam::put (const char * value, int timeout)
{
    return put(string(value), timeout);
}

RestParamSet::RestParamSet (asynPortDriver *portDriver, RestAPI *api,
        asynUser *user)
: mPortDriver(portDriver), mApi(api), mUser(user), mConfigMap(), mAsynMap()
{}

RestParam *RestParamSet::create(std::string const & asynName, asynParamType asynType,
                                std::string subSystem, std::string const & name)
{
    RestParam *p = new RestParam(this, asynName, asynType, subSystem, name);

    mAsynMap.insert(std::make_pair(p->getIndex(), p));
    return p;
}

RestParam * RestParamSet::create(const std::string& asynName, rest_param_type_t restType,
                                 const std::string& subSystem, std::string const & name,
                                 size_t arraySize, bool strict)
{
  RestParam * p = new RestParam(this, asynName, restType, subSystem, name, arraySize, strict);

  mAsynMap.insert(std::make_pair(p->getIndex(), p));
  return p;
}

void RestParamSet::addToConfigMap(std::string const & name, RestParam *p)
{
    if(!name.empty()) {
        mConfigMap.insert(std::make_pair(name, p));
    }
}

asynPortDriver *RestParamSet::getPortDriver (void)
{
    return mPortDriver;
}

RestAPI *RestParamSet::getApi (void)
{
    return mApi;
}

RestParam *RestParamSet::getByName (string const & name)
{
    rest_param_map_t::iterator item(mConfigMap.find(name));

    if(item != mConfigMap.end())
        return item->second;
    return NULL;
}

RestParam *RestParamSet::getByIndex (int index)
{
    rest_asyn_map_t::iterator item(mAsynMap.find(index));

    if(item != mAsynMap.end())
        return item->second;
    return NULL;
}

asynUser *RestParamSet::getUser (void)
{
    return mUser;
}

int RestParamSet::fetchAll (void)
{
    int status = EXIT_SUCCESS;

    rest_asyn_map_t::iterator it;
    for(it = mAsynMap.begin(); it != mAsynMap.end(); ++it)
        status |= it->second->fetch();

    return status;
}

int RestParamSet::fetchParams (vector<string> const & params)
{
    int status = EXIT_SUCCESS;
    vector<string>::const_iterator param;

    for(param = params.begin(); param != params.end(); ++param)
    {
        RestParam *p = getByName(*param);
        if(p)
            status |= p->fetch();
    }

    return status;
}
