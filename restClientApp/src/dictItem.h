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
  DictItem(const std::string& key, DictItem value);
  // A single key with a dictionary with multiple key-value pairs as the value
  // {"key": {"subKey1": "value1", "subKey2": "value2", "subKey3": "value3"}}
  DictItem(const std::string& key, std::vector<DictItem>& values);

  std::string str();

 private:
  std::string mKey;
  std::string mValue;

  std::string rawQuote(const std::string& value);

};

#endif //RESTCLIENT_DICTITEM_H
