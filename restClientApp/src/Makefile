TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_INCLUDES += -I$(TOP)/restClientApp/frozenSrc

LIBRARY_IOC = restClient

LIB_SRCS += restApi.cpp
LIB_SRCS += restParam.cpp
LIB_SRCS += dictItem.cpp

INC += restDefinitions.h
INC += restApi.h
INC += restParam.h
INC += dictItem.h

LIB_LIBS += asyn

PROD = dictItemTest
dictItemTest_SRCS = dictItemTest.cpp
dictItemTest_LIBS += restClient
dictItemTest_LIBS += frozen

USR_INCLUDES += $(BOOST_INCLUDE)
boost_unit_test_framework_DIR=$(BOOST_LIB)
dictItemTest_LIBS += boost_unit_test_framework

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
