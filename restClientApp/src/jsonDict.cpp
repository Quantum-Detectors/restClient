#include "jsonDict.h"

#include <sstream>

// Simple key-value pairs
JsonDict::JsonDict(const std::string& key, const char * value)
    : mKey(toJson(key)), mValue(toJson(std::string(value))) {}

JsonDict::JsonDict(const std::string& key, bool value)
    : mKey(toJson(key)), mValue(toJson(value)) {}

JsonDict::JsonDict(const std::string& key, int value)
    : mKey(toJson(key)), mValue(toJson(value)) {}

JsonDict::JsonDict(const std::string& key, double value)
    : mKey(toJson(key)), mValue(toJson(value)) {}

// Nest dictionary
JsonDict::JsonDict(const std::string& key, JsonDict& dictValue)
    : mKey(toJson(key)), mValue(dictValue.str()) {}

// Multiple key-value pairs
JsonDict::JsonDict(std::vector<JsonDict>& values)
    : mKey(), mValue()
{
  std::stringstream dict;
  std::vector<JsonDict>::iterator it;
  for (it = values.begin(); it != values.end(); it++) {
    if (it->mKey.empty()) {
      dict << it->mValue;
    }
    else {
      dict << it->mKey << ": " << it->mValue;
    }
    if (it < values.end() - 1) {
      dict << ", ";
    }
  }

  mValue = dict.str();
}

std::string JsonDict::str()
{
  std::stringstream dict;
  if (mKey.empty()) {
    dict << "{" << mValue << "}";
  }
  else {
    dict << "{" << mKey << ": " << mValue << "}";
  }

  return dict.str();
}

std::string JsonDict::toJson(const std::string& value)
{
  std::stringstream v;
  v << "\"" << value << "\"";
  return v.str();
}

std::string JsonDict::toJson(bool value)
{
  return value ? "true" : "false";
}

std::string JsonDict::toJson(int value)
{
  std::stringstream v;
  v << value;
  return v.str();
}

std::string JsonDict::toJson(double value)
{
  std::stringstream v;
  v << value;
  return v.str();
}
