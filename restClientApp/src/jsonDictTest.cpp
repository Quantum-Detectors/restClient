#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "JsonDictUnitTests"
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include "jsonDict.h"


BOOST_AUTO_TEST_SUITE(JsonDictUnitTests);

BOOST_AUTO_TEST_CASE(StringTest)
{
  JsonDict testDict = JsonDict("key", "value");

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": \"value\"}");
};

BOOST_AUTO_TEST_CASE(BoolTest)
{
  JsonDict testDict = JsonDict("key", false);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": false}");
};

BOOST_AUTO_TEST_CASE(IntTest)
{
  JsonDict testDict = JsonDict("key", 10);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": 10}");
};

BOOST_AUTO_TEST_CASE(DoubleTest)
{
  JsonDict testDict = JsonDict("key", 2.5);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": 2.5}");
};

BOOST_AUTO_TEST_CASE(ListStringTest)
{
  std::vector<std::string> stringVector;
  stringVector.push_back("value1");
  stringVector.push_back("value2");
  stringVector.push_back("value3");
  JsonDict testDict = JsonDict("key", stringVector);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": [\"value1\", \"value2\", \"value3\"]}");
};

BOOST_AUTO_TEST_CASE(ListBoolTest)
{
  std::vector<bool> boolVector;
  boolVector.push_back(false);
  boolVector.push_back(true);
  boolVector.push_back(false);
  JsonDict testDict = JsonDict("key", boolVector);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": [false, true, false]}");
};

BOOST_AUTO_TEST_CASE(ListIntTest)
{
  std::vector<int> intVector;
  intVector.push_back(1);
  intVector.push_back(2);
  intVector.push_back(3);
  JsonDict testDict = JsonDict("key", intVector);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": [1, 2, 3]}");
};

BOOST_AUTO_TEST_CASE(ListDoubleTest)
{
  std::vector<double> doubleVector;
  doubleVector.push_back(1.2);
  doubleVector.push_back(2.3);
  doubleVector.push_back(3.4);
  JsonDict testDict = JsonDict("key", doubleVector);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": [1.2, 2.3, 3.4]}");
};

BOOST_AUTO_TEST_CASE(DictTest)
{
  JsonDict subDict = JsonDict("subKey", "value");
  BOOST_TEST_MESSAGE(subDict.str().c_str());
  JsonDict testDict = JsonDict("key", subDict);

  BOOST_TEST_MESSAGE(testDict.str());
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

  BOOST_TEST_MESSAGE(testDict.str());
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

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{\"key\": "
                                        "{\"subKey1\": \"value1\", "
                                        "\"subKey2\": \"value2\", "
                                        "\"subKey3\": \"value3\""
                                        "}"
                                    "}");
};

BOOST_AUTO_TEST_CASE(ComplicatedTest)
{
  JsonDict pi = JsonDict("pi", 3.14);
  JsonDict happy = JsonDict("happy", true);
  JsonDict name = JsonDict("name", "Niels");
  JsonDict everything = JsonDict("everything", 42);
  JsonDict answer = JsonDict("answer", everything);
  std::vector<int> listVector;
  listVector.push_back(1);
  listVector.push_back(0);
  listVector.push_back(2);
  JsonDict list = JsonDict("list", listVector);
  std::vector<JsonDict> objectVector;
  objectVector.push_back(JsonDict("currency", "GBP"));
  objectVector.push_back(JsonDict("value", 42.99));
  JsonDict objectDict = JsonDict(objectVector);
  JsonDict object = JsonDict("object", objectDict);

  std::vector<JsonDict> testVector;
  testVector.push_back(pi);
  testVector.push_back(happy);
  testVector.push_back(name);
  testVector.push_back(answer);
  testVector.push_back(list);
  testVector.push_back(object);
  JsonDict testDict(testVector);

  BOOST_TEST_MESSAGE(testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{"
    "\"pi\": 3.14, "
    "\"happy\": true, "
    "\"name\": \"Niels\", "
    "\"answer\": {"
      "\"everything\": 42"
    "}, "
    "\"list\": [1, 0, 2], "
    "\"object\": {"
      "\"currency\": \"GBP\", "
      "\"value\": 42.99"
    "}"
  "}");
}

BOOST_AUTO_TEST_CASE(MergeTest)
{
  std::vector<JsonDict> firstVector;
  firstVector.push_back(JsonDict("answer", 42));
  firstVector.push_back(JsonDict("happy", true));
  JsonDict firstDict = JsonDict(firstVector);
  JsonDict first = JsonDict(firstDict);
  BOOST_TEST_MESSAGE("First: " + first.str());

  std::vector<JsonDict> secondVector;
  secondVector.push_back(JsonDict("currency", "GBP"));
  secondVector.push_back(JsonDict("value", 42.99));
  JsonDict secondDict = JsonDict(secondVector);
  JsonDict second = JsonDict(secondDict);
  BOOST_TEST_MESSAGE("Second: " + second.str());

  std::vector<JsonDict> testVector;
  testVector.push_back(first);
  testVector.push_back(second);
  JsonDict testDict(testVector);

  BOOST_TEST_MESSAGE("Merged: " + testDict.str());
  BOOST_CHECK_EQUAL(testDict.str(), "{"
      "\"answer\": 42, "
      "\"happy\": true, "
      "\"currency\": \"GBP\", "
      "\"value\": 42.99"
      "}");
}

BOOST_AUTO_TEST_CASE(EmptyRaisesTest)
{
  std::vector<JsonDict> emptyVector;
  BOOST_REQUIRE_THROW(JsonDict test = JsonDict(emptyVector), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END();
