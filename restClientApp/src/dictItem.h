#ifndef RESTCLIENT_DICTITEM_H
#define RESTCLIENT_DICTITEM_H

#include <string>
#include <vector>

class DictItem
{
 public:
  // Simple key-value-pair
  // {"key": "value"}
  DictItem(const std::string& key, const std::string& value);
  // A single key with an arbitrary dictionary for the value
  // {"key": {"subKey": "value"}}
  DictItem(const std::string& key, DictItem& value);
  // Multiple key-value pairs
  // {"key1": "value1", "key2": "value2", "key3": "value3"}
  DictItem(std::vector<DictItem>& values);

  std::string str();

 private:
  std::string mKey;
  std::string mValue;

  std::string rawQuote(const std::string& value);

};

#endif //RESTCLIENT_DICTITEM_H
