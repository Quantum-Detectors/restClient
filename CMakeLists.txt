cmake_minimum_required(VERSION 3.5)
project(restClient)

set(BOOST "/dls_sw/prod/tools/RHEL6-x86_64/boost/1-48-0/prefix")
set(SUPPORT "/dls_sw/prod/R3.14.12.3/support")
set(EPICS_BASE "/dls_sw/epics/R3.14.12.3/base")
set(ASYN "${SUPPORT}/asyn/4-31")
set(TEMPLATE_TOP "${EPICS_BASE}/templates/makeBaseApp/top")

include_directories(
    ${ASYN}/include
    ${TEMPLATE_TOP}/include
    ${BOOST}/include
    ${SUPPORT}/include
    ${EPICS_BASE}/include
    ${EPICS_BASE}/include/os/Linux
    restClientApp/frozenSrc)

set(RESTCLIENT_SOURCE_FILES
    restClientApp/frozenSrc/frozen.c
    restClientApp/frozenSrc/frozen.h
    restClientApp/src/restParam.h
    restClientApp/src/restParam.cpp
    restClientApp/src/restApi.cpp
    restClientApp/src/jsonDict.h
    restClientApp/src/jsonDict.cpp
    restClientApp/src/restApi.h
    restClientApp/src/jsonDictTest.cpp
    restClientApp/src/restDefinitions.h
    include/restParam.h
    include/jsonDict.h
    include/restApi.h
    include/restDefinitions.h
    cmake-build-debug/CMakeFiles/feature_tests.c
    cmake-build-debug/CMakeFiles/3.8.2/CompilerIdC/CMakeCCompilerId.c
    cmake-build-debug/CMakeFiles/3.8.2/CompilerIdCXX/CMakeCXXCompilerId.cpp
)

add_library(restClient_source ${RESTCLIENT_SOURCE_FILES})
add_custom_target(restClient_build
    COMMAND $(MAKE) -C /scratch/work/R3.14.12.3/support/restClient
    SOURCES ${RESTCLIENT_SOURCE_FILES})
