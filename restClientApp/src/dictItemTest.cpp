#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "DictItemUnitTests"
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "dictItem.h"

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(DictItemUnitTests);

BOOST_AUTO_TEST_CASE(StringTest)
{
  DictItem testDict = DictItem("key", "value");

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": \"value\"}");
};

BOOST_AUTO_TEST_CASE(DictTest)
{
  DictItem subDict = DictItem("subKey", "value");
  BOOST_TEST_MESSAGE(subDict.str().c_str());
  DictItem testDict = DictItem("key", subDict);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": "
                                        "{\"subKey\": \"value\"}"
                                    "}");
};

BOOST_AUTO_TEST_CASE(DictVectorTest)
{
  std::vector<DictItem> dictVector;
  dictVector.push_back(DictItem("subKey1", "value1"));
  dictVector.push_back(DictItem("subKey2", "value2"));
  dictVector.push_back(DictItem("subKey3", "value3"));
  DictItem testDict = DictItem("key", dictVector);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": "
                                        "{\"subKey1\": \"value1\","
                                        " \"subKey2\": \"value2\","
                                        " \"subKey3\": \"value3\""
                                        "}"
                                    "}");
};

BOOST_AUTO_TEST_SUITE_END();
