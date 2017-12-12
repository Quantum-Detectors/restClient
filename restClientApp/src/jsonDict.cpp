#include "jsonDict.h"

#include <sstream>

JsonDict::JsonDict(const std::string& key, const std::string& value)
    : mKey(rawQuote(key)), mValue(rawQuote(value)) {}

JsonDict::JsonDict(const std::string& key, JsonDict& dictValue)
    : mKey(rawQuote(key)), mValue(dictValue.str()) {}

JsonDict::JsonDict(std::vector<JsonDict>& values)
    : mKey(), mValue()
{
  std::stringstream dict;
  std::vector<JsonDict>::iterator it;
  for (it = values.begin(); it != values.end(); it++) {
    dict << it->mKey << ": " << it->mValue;
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

std::string JsonDict::rawQuote(const std::string& value)
{
  std::stringstream quotedValue;
  quotedValue << "\"" << value << "\"";

  return quotedValue.str();
}
