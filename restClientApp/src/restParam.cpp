#include <stdlib.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <limits>

#include <frozen.h>
#include <math.h>
#include "restParam.h"

#define ERROR(message) \
        { \
            std::stringstream ss; \
            ss << message; \
            setError(functionName, ss.str()); \
        }

#define ERROR_IDX(message, index) \
        { \
            std::stringstream ss; \
            ss << message; \
            setError(functionName, ss.str(), index); \
        }

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
            for(int i = 1; i <= t->num_desc; ++i){
                arrayValues.push_back(string((t+i)->ptr, (t+i)->len));
            }
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
        ERROR("Failed to find '" << key.c_str() << "' json field");
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
        ERROR("Unrecognized value type '" << typeStr.c_str() << "'");
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
        ERROR("Invalid access mode '" << accessModeStr.c_str() << "'");
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
            ERROR("Failed to find '" << mSet->getApi()->PARAM_TYPE.c_str() << "' json field");
            return EXIT_FAILURE;
        }

        string value(t->ptr, t->len);
        if(type->ptr[0] == 'i' || type->ptr[0] == 'u')
        {
            if(sscanf(value.c_str(), "%d", &minMax.valInt) != 1)
            {
                ERROR("Failed to parse '" << value.c_str() << "' as integer");
                return EXIT_FAILURE;
            }
        }
        else if(type->ptr[0] == 'f')
        {
            if(sscanf(value.c_str(), "%lf", &minMax.valDouble) != 1)
            {
                ERROR("Failed to parse '" << value.c_str() << "' as double");
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
    ERROR("Failed to parse parameter type");
    return EXIT_FAILURE;
  }

  if (mType == REST_P_ENUM) {
    if (!mCustomEnum) {
      mEnumValues = parseArray(tokens, mSet->getApi()->PARAM_ENUM_VALUES);
      // Confirm that the number of enum elements is non zero (non empty array), else fail
      if (mEnumValues.empty()) {
        ERROR("Failed to parse enum values");
        return EXIT_FAILURE;
      }
    }
  }

  if (mStrictInitialisation) {

    mCriticalValues = parseArray(tokens, mSet->getApi()->PARAM_CRITICAL_VALUES);

    if(mType == REST_P_INT || mType == REST_P_UINT || mType == REST_P_DOUBLE) {
      if(parseMinMax(tokens, mSet->getApi()->PARAM_MIN, mMin)) {
        ERROR("Failed to parse min limit");
        return EXIT_FAILURE;
      }

      if(parseMinMax(tokens, mSet->getApi()->PARAM_MAX, mMax)) {
        ERROR("Failed to parse max limit");
        return EXIT_FAILURE;
      }
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
        ERROR("Failed to find '" << key.c_str() << "' json field");
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
        ERROR("Failed to parse value '" << rawValue.c_str() << "' as boolean");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::parseValue (string const & rawValue, int & value)
{
    const char *functionName = "parseValue";

    if(sscanf(rawValue.c_str(), "%d", &value) != 1)
    {
        ERROR("Failed to parse value '" << rawValue.c_str() << "' as integer");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::parseValue (string const & rawValue, double & value)
{
    const char *functionName = "parseValue";

    if(sscanf(rawValue.c_str(), "%lf", &value) != 1)
    {
        ERROR("Failed to parse value '" << rawValue.c_str() << "' as double");
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
    const char *functionName = "toString";
    if(mType == REST_P_ENUM){
        if ((size_t)value >= mEnumValues.size()){
            ERROR("Value '" << value << "' is out of range; no enum value at this index");
            return "";
        }
        return toString(mEnumValues[value]);
    }
    std::ostringstream os;
    os << value;
    return os.str();
}

std::string RestParam::toString (double value)
{
    std::ostringstream os;
    os << std::setprecision(std::numeric_limits<long double>::digits10 + 2) << value;
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

    ERROR("Failed to find index of value " << value.c_str());
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
    if (address < 0) address = 0;
    return (int) mSet->getPortDriver()->setIntegerParam(address, mAsynIndex, value);
}

int RestParam::setParam(double value, int address)
{
    if (address < 0) address = 0;
    return (int) mSet->getPortDriver()->setDoubleParam(address, mAsynIndex, value);
}

int RestParam::setParam(const std::string& value, int address)
{
    if (address < 0) address = 0;
    return (int) mSet->getPortDriver()->setStringParam(address, mAsynIndex, value);
}

int RestParam::setParamStatus(int status, int address)
{
    asynStatus state = status ? asynDisconnected : asynSuccess;
    return (int) mSet->getPortDriver()->setParamStatus(address, getIndex(), state);

}

int RestParam::setConnectedStatus(int status)
{
    mConnected[0] = (bool) status == 0;
    return setParamStatus(status);
}

int RestParam::setConnectedStatus(std::vector<int> status)
{
    int _status = 0;
    for (int index = 0; (size_t) index < mArraySize; ++index) {
        mConnected[index] = (bool) status[index] == 0;
        _status |= setParamStatus(status[index], index);
    }
    return _status;
}

RestParam::RestParam(RestParamSet *set, std::string const & asynName, asynParamType asynType,
                     std::string subSystem, std::string const & name)
    : mErrorFilter(new ErrorFilter()), mSet(set),
      mAsynName(asynName), mAsynType(asynType), mAsynIndex(-1),
      mSubSystem(subSystem), mName(name), mRemote(!mName.empty()), mPushAll(true),
      mAccessMode(REST_ACC_RW), mMin(), mMax(), mEnumValues(), mCriticalValues(), mEpsilon(0.0),
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
            ERROR("Invalid asyn type " << (int) asynType);
            throw std::runtime_error(mAsynName);
        }
    }

  bindAsynParam();
  setTimeout(DEFAULT_TIMEOUT);
}

RestParam::RestParam(RestParamSet * set, const std::string& asynName, rest_param_type_t restType,
                     const std::string& subSystem, const std::string& name, size_t arraySize,
                     bool strict)
    : mErrorFilter(new ErrorFilter()), mSet(set),
      mAsynName(asynName), mAsynType(asynParamNotDefined), mAsynIndex(-1),
      mSubSystem(subSystem), mName(name), mRemote(!mName.empty()), mPushAll(true), mType(restType),
      mAccessMode(REST_ACC_RW), mMin(), mMax(), mEnumValues(), mCriticalValues(), mEpsilon(0.0),
      mCustomEnum(false), mArraySize(arraySize), mInitialised(false), mStrictInitialisation(strict),
      mConnected(std::vector<bool>(mArraySize, false))
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
            ERROR("Invalid REST type " << (int) restType);
            throw std::runtime_error(mAsynName);
    }

    bindAsynParam();
    setTimeout(DEFAULT_TIMEOUT);
}

asynStatus RestParam::bindAsynParam()
{
    const char *functionName = "bindAsynParam";
    asynStatus status = asynSuccess;
    // Check if asyn parameter already exists, create if it doesn't
    if(mSet->getPortDriver()->findParam(mAsynName.c_str(), &mAsynIndex)) {
        FLOW_ARGS("[param=%s] creating param", mAsynName.c_str());
        status = mSet->getPortDriver()->createParam(mAsynName.c_str(), mAsynType, &mAsynIndex);
        if (mArraySize){
          for (size_t index = 0; index < mArraySize; index++){
            switch (mAsynType){
            case asynParamInt32:
              mSet->getPortDriver()->setIntegerParam(index, mAsynIndex, 0);
              break;
            case asynParamFloat64:
              mSet->getPortDriver()->setDoubleParam(index, mAsynIndex, 0.0);
              break;
            case asynParamOctet:
              mSet->getPortDriver()->setStringParam(index, mAsynIndex, "");
              break;
            default:
              // Do nothing if we are not using the parameter type
              break;
            }
          }
        }
        if(status) {
            ERROR("Failed to create param");
            throw std::runtime_error(mAsynName);
        }
    }
    else {
        RestParam* param = mSet->getByIndex(mAsynIndex);
        if (param != NULL) {
            ERROR("Asyn param " << mAsynName << "is already bound to " << param->mName);
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

void RestParam::setTimeout(int timeout)
{
  mTimeout = timeout;
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

int RestParam::baseFetch(string & rawValue)
{
    const char *functionName = "baseFetch";
    if(!mRemote)
    {
        ERROR("Can't fetch local parameter");
        return EXIT_FAILURE;
    }

    if(mAccessMode == REST_ACC_WO)
        return EXIT_SUCCESS;

    string buffer;
    mSet->getApi()->get(mSubSystem, mName, buffer, mTimeout);

    // Parse JSON
    struct json_token tokens[MAX_JSON_TOKENS];
    int err = parse_json(buffer.c_str(), buffer.size(), tokens, MAX_JSON_TOKENS);
    if(err < 0)
    {
        ERROR("Failed to parse json response:\n'" << buffer << "'");
        return EXIT_FAILURE;
    }

    if (!mInitialised) {
        if (initialise(tokens)) {
            ERROR("Failed to initialise param from response:\n'" << buffer << "'");
            return EXIT_FAILURE;
        }
    }

    if(parseValue(tokens, rawValue))
    {
        ERROR("Failed to parse raw value from response:\n'" << buffer << "'");
        return EXIT_FAILURE;
    }

    FLOW_ARGS("%s", rawValue.c_str());
    return EXIT_SUCCESS;
}

int RestParam::baseFetch(std::vector<std::string>& rawValue)
{
    const char *functionName = "baseFetch<vector>";
    if(!mRemote)
    {
        ERROR("Can't fetch local parameter");
        return EXIT_FAILURE;
    }

    if(mAccessMode == REST_ACC_WO)
        return EXIT_SUCCESS;

    std::string buffer;
    mSet->getApi()->get(mSubSystem, mName, buffer, mTimeout);

    // Parse JSON
    struct json_token tokens[MAX_JSON_TOKENS];
    int err = parse_json(buffer.c_str(), buffer.size(), tokens, MAX_JSON_TOKENS);
    if(err < 0)
    {
        ERROR("Unable to parse json response\n'" << buffer << "'");
        return EXIT_FAILURE;
    }

    if (!mInitialised) {
        if (initialise(tokens)) {
            ERROR("Failed to initialise param from response:\n'" << buffer << "'");
            return EXIT_FAILURE;
        }
    }

    std::vector<std::string> valueArray = parseArray(tokens, mSet->getApi()->PARAM_VALUE);
    if(valueArray.empty())
    {
        ERROR("Failed to parse raw value array from response:\n'" << buffer << "'");
        return EXIT_FAILURE;
    }
    for (int index = 0; (size_t) index != valueArray.size(); ++index) {
        if (valueArray[index] == "null") {
            ERROR("Failed to parse raw value from array:\n'" << buffer << "'");
            return EXIT_FAILURE;
        }
    }

    rawValue.resize(valueArray.size(), "");
    for (int index = 0; (size_t) index != valueArray.size(); ++index) {
        rawValue[index] = valueArray[index];
    }

    return EXIT_SUCCESS;
}

int RestParam::fetch(bool & value)
{
    const char *functionName = "fetch<bool>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        string rawValue;
        if(baseFetch(rawValue))
        {
            ERROR("Underlying baseFetch failed");
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
                ERROR("Can't fetch non-binary enum as bool");
                return EXIT_FAILURE;
            }

            size_t index;
            if(getEnumIndex(rawValue, index))
                return EXIT_FAILURE;

            value = (bool) index;
            if(setParam((int) index))
            {
                ERROR("Failed to set asyn parameter");
                return EXIT_FAILURE;
            }
        }
        else
        {
            ERROR("Unexpected type for boolean param: " << mType);
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%d", value);
    return EXIT_SUCCESS;
}

std::vector<int> RestParam::fetch(std::vector<bool>& value)
{
    const char *functionName = "fetch<vector<bool>>";

    std::vector<int> status(mArraySize, 1);
    if(mRemote && mType != REST_P_COMMAND) {
        std::vector<std::string> rawValue;
        if (baseFetch(rawValue)) {
            ERROR("Underlying baseFetch failed");
            return status;
        }

        value.resize(mArraySize);
        for (int index = 0; (size_t) index != rawValue.size(); ++index) {
            if (mType == REST_P_BOOL) {
                bool temp;
                if (parseValue(rawValue[index], temp)) {
                    return status;
                }
                value[index] = temp;
            }
            else if(mType == REST_P_ENUM) {
                if(mEnumValues.size() != 2) {
                    ERROR("Can't fetch non-binary enum as bool");
                    return status;
                }

                size_t eIndex;
                if (getEnumIndex(rawValue[index], eIndex))
                    return status;

                value[index] = (bool) eIndex;
                status[index] = setParam((int) eIndex, index);
                if (status[index]) {
                    ERROR("Failed to set asyn parameter");
                }
            }
            else {
                ERROR("Unexpected type for boolean param: " << mType);
                return status;
            }
            FLOW_ARGS("%d", (bool) value[index]);
        }
    }
    return status;
}

int RestParam::fetch(int & value)
{
    const char *functionName = "fetch<int>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        string rawValue;
        if(baseFetch(rawValue))
        {
            ERROR("Underlying baseFetch failed");
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
            ERROR("Unexpected type for int param: " << mType);
            return EXIT_FAILURE;
        }

        if(setParam(value))
        {
            ERROR("Failed to set asyn parameter");
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%d", value);
    return EXIT_SUCCESS;
}

std::vector<int> RestParam::fetch(std::vector<int>& value)
{
    const char *functionName = "fetch<vector<int>>";

    std::vector<int> status(mArraySize, 1);
    if (mRemote && mType != REST_P_COMMAND) {
        std::vector<std::string> rawValue;
        if (baseFetch(rawValue)) {
            ERROR("Underlying baseFetch failed");
            return status;
        }

        value.resize(mArraySize);
        for (int index = 0; (size_t) index != rawValue.size(); ++index) {
            if (mType == REST_P_ENUM) {
                size_t eIndex;
                if (getEnumIndex(rawValue[index], eIndex)) {
                    return status;
                }

                value[index] = (int) eIndex;
            }
            else if (mType == REST_P_BOOL) {
                bool tempValue;
                if (parseValue(rawValue[index], tempValue)) {
                    return status;
                }
                value[index] = (int) tempValue;
            }
            else if (mType == REST_P_INT || mType == REST_P_UINT) {
                if (parseValue(rawValue[index], value[index]))
                    return status;
            }
            else {
                ERROR_IDX("Unexpected type for int param " << mType, index);
                return status;
            }

            status[index] = setParam(value[index], index);
            if (status[index]) {
                ERROR_IDX("Failed to set asyn parameter", index);
            }
            FLOW_ARGS("%d", value[index]);
        }
    }
    return status;
}

int RestParam::fetch(double & value)
{
    const char *functionName = "fetch<double>";
    if(mRemote && mType != REST_P_COMMAND)
    {
        if(mType != REST_P_DOUBLE && mType != REST_P_UNINIT)
        {
            ERROR("Unexpected type for double param: " << mType);
            return EXIT_FAILURE;
        }

        string rawValue;
        if(baseFetch(rawValue))
        {
            ERROR("Underlying baseFetch failed");
            return EXIT_FAILURE;
        }

        if(parseValue(rawValue, value))
            return EXIT_FAILURE;

        if(setParam(value))
        {
            ERROR("Failed to set asyn parameter");
            return EXIT_FAILURE;
        }
    }

    FLOW_ARGS("%lf", value);
    return EXIT_SUCCESS;
}

std::vector<int> RestParam::fetch(std::vector<double>& value)
{
    const char *functionName = "fetch<double>";

    std::vector<int> status(mArraySize, 1);
    value.resize(mArraySize);
    if(mRemote && mType != REST_P_COMMAND) {
        if(mType != REST_P_DOUBLE && mType != REST_P_UNINIT) {
            ERROR("Unexpected type for double param: " << mType);
            return status;
        }

        std::vector<std::string> rawValue;
        if(baseFetch(rawValue)) {
            ERROR("Underlying baseFetch failed");
            return status;
        }

        for (size_t index = 0; index != rawValue.size(); ++index) {
            status[index] = parseValue(rawValue[index], value[index]);
            if (status[index] == 0){
              status[index] = setParam(value[index], (int) index);
              if (status[index]) {
                ERROR_IDX("Failed to set asyn parameter", (int) index);
              }
              FLOW_ARGS("%lf", value[index]);
            }
        }
    }
    return status;
}

int RestParam::fetch(string & value)
{
    const char *functionName = "fetch<string>";

    if(mRemote && mType != REST_P_COMMAND)
    {
        if(mType != REST_P_STRING && mType != REST_P_ENUM &&
           mType != REST_P_UNINIT)
            return EXIT_FAILURE;

        if(baseFetch(value))
        {
            ERROR("Underlying baseFetch failed");
            return EXIT_FAILURE;

        }

        // TODO: check if it is critical

        if(setParam(value))
        {
            ERROR("Failed to set asyn parameter");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    FLOW_ARGS("%s", value.c_str());
    return EXIT_SUCCESS;
}

std::vector<int> RestParam::fetch(std::vector<std::string>& value)
{
    const char *functionName = "fetch<vector<string>>";

    std::vector<int> status(mArraySize, 1);
    if(mRemote && mType != REST_P_COMMAND) {
        if(mType != REST_P_STRING && mType != REST_P_ENUM && mType != REST_P_UNINIT) {
            return status;
        }

        if(baseFetch(value)) {
            ERROR("Underlying baseFetch failed");
            return status;
        }

        for (size_t index = 0; index != value.size(); ++index) {
            status[index] = setParam(value[index], (int) index);
            if (status[index]) {
                ERROR_IDX("Failed to set asyn parameter", (int) index);
            }
            FLOW_ARGS("%s", value[index].c_str());
        }
    }
    return status;
}

int RestParam::fetch()
{
  int status = 0;
  if (mAccessMode != REST_ACC_WO) {
    if (mArraySize) {
      std::vector<int> fetch_status;
      switch (mAsynType) {
        case asynParamInt32: {
          std::vector<int> dummy;
          fetch_status = fetch(dummy);
          break;
        }
        case asynParamFloat64: {
          std::vector<double> dummy;
          fetch_status = fetch(dummy);
          break;
        }
        case asynParamOctet: {
          std::vector<std::string> dummy;
          fetch_status = fetch(dummy);
          break;
        }
        default:break;
      }
      status |= std::accumulate(fetch_status.begin(), fetch_status.end(), 0);
      status |= setConnectedStatus(fetch_status);
    } else {
      switch (mAsynType) {
        case asynParamInt32: {
          int dummy;
          status = fetch(dummy);
          break;
        }
        case asynParamFloat64: {
          double dummy;
          status = fetch(dummy);
          break;
        }
        case asynParamOctet: {
          std::string dummy;
          status = fetch(dummy);
          break;
        }
        default:break;
      }
      status |= setConnectedStatus(status);
    }
    if (status == 0) {
      mErrorFilter->clearErrors();
    }
  }
  return status;
}

int RestParam::push()
{
  int status = 0;
  bool boolValue;
  int intValue;
  double doubleValue;
  std::string stringValue;

  switch (mType)
  {
    case REST_P_BOOL:
      if (mArraySize){
        for (size_t index = 0; index < mArraySize; index++){
          status |= getParam(intValue, index);
          boolValue = (bool)intValue;
          status |= this->put(boolValue, index);
        }
      } else {
        status |= getParam(intValue);
        boolValue = (bool)intValue;
        status |= this->put(boolValue);
      }
      break;
    case REST_P_UINT: case REST_P_INT: case REST_P_ENUM:
      if (mArraySize){
        for (size_t index = 0; index < mArraySize; index++){
          status |= getParam(intValue, index);
          status |= this->put(intValue, index);
        }
      } else {
        status |= getParam(intValue);
        status |= this->put(intValue);
      }
      break;
    case REST_P_DOUBLE:
      if (mArraySize){
        for (size_t index = 0; index < mArraySize; index++){
          status |= getParam(doubleValue, index);
          status |= this->put(doubleValue, index);
        }
      } else {
        status |= getParam(doubleValue);
        status |= this->put(doubleValue);
      }
      break;
    case REST_P_STRING:
      if (mArraySize){
        for (size_t index = 0; index < mArraySize; index++){
          status |= getParam(stringValue, index);
          status |= this->put(stringValue, index);
        }
      } else {
        status |= getParam(stringValue);
        status |= this->put(stringValue);
      }
      break;
    default:
      // Do not push anything
      break;
  }
  return status;
}

int RestParam::basePut(const std::string & rawValue, int index)
{
    const char *functionName = "basePut";
   FLOW_ARGS("'%s'", rawValue.c_str());
    if(mAccessMode == REST_ACC_RO)
    {
        ERROR_IDX("Can't write to read-only parameter", index);
        return EXIT_FAILURE;
    }

    std::stringstream endpoint;
    if (index < 0) {
        endpoint << mName;
    }
    else {
        endpoint << mName << "/" << index;
    }

    std::string reply;
    if(mSet->getApi()->put(mSubSystem, endpoint.str(), rawValue, &reply, mTimeout))
    {
        ERROR_IDX("Underlying RestAPI put failed", index);
        return EXIT_FAILURE;
    }

    // Parse JSON
    if(!reply.empty())
    {
        struct json_token tokens[MAX_JSON_TOKENS];
        int err = parse_json(reply.c_str(), reply.size(), tokens, MAX_JSON_TOKENS);
        if(err < 0)
        {
            ERROR("Unable to parse json response\n'" << reply << "'");
            return EXIT_FAILURE;
        }

        mSet->fetchParams(parseArray(tokens));
    }
    return EXIT_SUCCESS;
}

void RestParam::disablePushAll()
{
  mPushAll = false;
}

bool RestParam::canPushAll()
{
  return mPushAll;
}

int RestParam::put(bool value, int index)
{
    const char *functionName = "put<bool>";
    FLOW_ARGS("%d", value);
    if(!mRemote)
    {
        setParam((int)value, index);
        return EXIT_SUCCESS;
    }

    if (!mInitialised && fetch()) {
        return EXIT_FAILURE;
    }

    if(mType != REST_P_BOOL && mType != REST_P_ENUM && mType != REST_P_COMMAND){
        return EXIT_FAILURE;
    }
    int status;
    if(mType == REST_P_ENUM)
    {
        if(mEnumValues.size() != 2){
          return EXIT_FAILURE;

        }

        // XOR with mReversedEnum
        status = basePut(toString(value), index);
    }
    else
        status = basePut(toString(value), index);

    if(status){
        return EXIT_FAILURE;
    }

    if(setParam(value, index))
    {
        ERROR_IDX("Failed to set asyn parameter", index);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RestParam::put(int value, int index)
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
            ERROR("Unexpected type for int param " << mType);
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
            status = basePut(toString((bool)value), index);
        else
            status = basePut(toString(value), index);

        if(status)
        {
            ERROR_IDX("Underlying basePut failed", index);
            return EXIT_FAILURE;
        }
    }

    if(mAsynType == asynParamInt32)
        status = setParam(value, index);
    else
        status = setParam(mEnumValues[value], index);

    if(status)
    {
        ERROR_IDX("Failed to set asyn parameter", index);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::put(double value, int index)
{
    const char *functionName = "put<double>";
    FLOW_ARGS("%lf", value);
    if(mEpsilon)
    {
        double currentValue;
        getParam(currentValue, index);
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
            ERROR_IDX("PUT value clamped to minimum: " << value, index);
        }

        if(mMax.exists && (value > mMax.valDouble))
        {
            value = mMax.valDouble;
            ERROR_IDX("PUT value clamped to maximum: " << value, index);
        }

        if(basePut(toString(value), index)){
            return EXIT_FAILURE;
        }
    }

    if(setParam(value, index))
    {
        ERROR_IDX("Failed to set asyn parameter", index);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int RestParam::put(const std::string& value, int index)
{
    const char *functionName = "put<string>";
    FLOW_ARGS("%s", value.c_str());
    if(!mRemote)
    {
        setParam(value, index);
        return EXIT_SUCCESS;
    }

    if (!mInitialised && fetch()) {
        return EXIT_FAILURE;
    }

    if(mType != REST_P_STRING && mType != REST_P_ENUM) {
        return EXIT_FAILURE;
    }

    size_t eIndex;
    if(mType == REST_P_ENUM && getEnumIndex(value, eIndex)) {
        return EXIT_FAILURE;
    }

    if(basePut(toString(value), index)) {
        return EXIT_FAILURE;
    }

    int status;
    if(mAsynType == asynParamInt32) {
        status = setParam((int)eIndex, index);
    } else {
        status = setParam(value, index);
    }

    if(status)
    {
        ERROR_IDX("Failed to set asyn parameter", index);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RestParam::put(const char * value, int index)
{
    return put(string(value), index);
}

void RestParam::setError(const char* functionName, std::string error, int index)
{
    std::stringstream indexSS;
    if (index >= 0) {
        indexSS << "[" << index << "]";
    }
    std::stringstream errorMessage;
    errorMessage << "RestParam[" << mAsynName << " -> " << mName << indexSS.str() << "]::"
                 << functionName << ": " << error << "\n";

    if (mErrorFilter->newError(errorMessage.str())) {
        asynPrint(mSet->getUser(), ASYN_TRACE_ERROR, errorMessage.str().c_str());
    }
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

int RestParamSet::pushAll (void)
{
    int status = EXIT_SUCCESS;

    rest_asyn_map_t::iterator it;
    for(it = mAsynMap.begin(); it != mAsynMap.end(); ++it){
      if (it->second->canPushAll()){
        status |= it->second->push();
      }
    }
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
