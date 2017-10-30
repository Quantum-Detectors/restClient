TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_INCLUDES += -I$(TOP)/restClientApp/frozenSrc

LIBRARY_IOC = restClient

LIB_SRCS += restApi.cpp
LIB_SRCS += restParam.cpp

INC += restApi.h
INC += restParam.h

LIB_LIBS += asyn

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
