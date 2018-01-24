TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_INCLUDES += -I$(TOP)/restClientApp/frozenSrc

LIBRARY_IOC = restClient

CXXFLAGS += -O0

LIB_SRCS += restApi.cpp
LIB_SRCS += restParam.cpp
LIB_SRCS += jsonDict.cpp

INC += restDefinitions.h
INC += restApi.h
INC += restParam.h
INC += jsonDict.h

LIB_LIBS += asyn

PROD = jsonDictTest
jsonDictTest_SRCS = jsonDictTest.cpp
jsonDictTest_LIBS += restClient
jsonDictTest_LIBS += frozen

USR_INCLUDES += $(BOOST_INCLUDE)
boost_unit_test_framework_DIR=$(BOOST_LIB)
jsonDictTest_LIBS += boost_unit_test_framework

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
