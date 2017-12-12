#ifndef RESTCLIENT_JSONDICT_H
#define RESTCLIENT_JSONDICT_H

#include <string>
#include <vector>

class JsonDict
{
 public:
  // Simple key-value-pair
  // {"key": "value"}
  JsonDict(const std::string& key, const std::string& value);
  // A single key with an arbitrary dictionary for the value
  // {"key": {"subKey": "value"}}
  JsonDict(const std::string& key, JsonDict& value);
  // Multiple key-value pairs
  // {"key1": "value1", "key2": "value2", "key3": "value3"}
  JsonDict(std::vector<JsonDict>& values);

  std::string str();

 private:
  std::string mKey;
  std::string mValue;

  std::string rawQuote(const std::string& value);

};

#endif //RESTCLIENT_JSONDICT_H
