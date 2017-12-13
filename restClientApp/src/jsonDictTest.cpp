#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "JsonDictUnitTests"
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "jsonDict.h"


BOOST_AUTO_TEST_SUITE(JsonDictUnitTests);

BOOST_AUTO_TEST_CASE(StringTest)
{
  JsonDict testDict = JsonDict("key", "value");

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": \"value\"}");
};

BOOST_AUTO_TEST_CASE(BoolTest)
{
  JsonDict testDict = JsonDict("key", false);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": false}");
};

BOOST_AUTO_TEST_CASE(IntTest)
{
  JsonDict testDict = JsonDict("key", 10);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": 10}");
};

BOOST_AUTO_TEST_CASE(DoubleTest)
{
  JsonDict testDict = JsonDict("key", 2.5);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": 2.5}");
};

BOOST_AUTO_TEST_CASE(DictTest)
{
  JsonDict subDict = JsonDict("subKey", "value");
  BOOST_TEST_MESSAGE(subDict.str().c_str());
  JsonDict testDict = JsonDict("key", subDict);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": "
                                        "{\"subKey\": \"value\"}"
                                    "}");
};

BOOST_AUTO_TEST_CASE(DictVectorTest)
{
  std::vector<JsonDict> dictVector;
  dictVector.push_back(JsonDict("key1", "value1"));
  dictVector.push_back(JsonDict("key2", "value2"));
  dictVector.push_back(JsonDict("key3", "value3"));
  JsonDict testDict = JsonDict(dictVector);

  BOOST_CHECK_EQUAL(testDict.str(), "{"
                                        "\"key1\": \"value1\", "
                                        "\"key2\": \"value2\", "
                                        "\"key3\": \"value3\""
                                    "}");
};

BOOST_AUTO_TEST_CASE(DictKeyVectorTest)
{
  std::vector<JsonDict> dictVector;
  dictVector.push_back(JsonDict("subKey1", "value1"));
  dictVector.push_back(JsonDict("subKey2", "value2"));
  dictVector.push_back(JsonDict("subKey3", "value3"));
  JsonDict vectorDict = JsonDict(dictVector);
  BOOST_TEST_MESSAGE(vectorDict.str().c_str());
  JsonDict testDict = JsonDict("key", vectorDict);

  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": "
                                        "{\"subKey1\": \"value1\", "
                                        "\"subKey2\": \"value2\", "
                                        "\"subKey3\": \"value3\""
                                        "}"
                                    "}");
};

BOOST_AUTO_TEST_SUITE_END();
