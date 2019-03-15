PROJECT(im_im)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
set(CMAKE_CXX_STANDARD 11)
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -std=c++11 -pthread -O2")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

set(COMMON  ".")
#set(CMAKE_BUILD_TYPE debug)


INCLUDE_DIRECTORIES("../../include")
# dir COMMON
INCLUDE_DIRECTORIES("${COMMON}")
FILE(GLOB root "${COMMON}/*.cc")

INCLUDE_DIRECTORIES("${COMMON}/core/")
INCLUDE_DIRECTORIES("${COMMON}/event/")
INCLUDE_DIRECTORIES("${COMMON}/network/")
INCLUDE_DIRECTORIES("${COMMON}/proto/")
# dir base
INCLUDE_DIRECTORIES("${COMMON}/base/")
FILE(GLOB base "${COMMON}/base/*.cc")

# dir node
INCLUDE_DIRECTORIES("${COMMON}/node/")
FILE(GLOB node "${COMMON}/node/*.cc")


# dir protobuf
INCLUDE_DIRECTORIES("${COMMON}/protobuf/")
FILE(GLOB protobuf "${COMMON}/protobuf/*.cc")


# dir threadpool
INCLUDE_DIRECTORIES("${COMMON}/threadpool/")


# dir xml
INCLUDE_DIRECTORIES("${COMMON}/third/tinyxml/")
FILE(GLOB xml "${COMMON}/third/tinyxml/*.cpp")

# dir redis
INCLUDE_DIRECTORIES("${COMMON}/third/redis/")
FILE(GLOB redis "${COMMON}/third/redis/hiredis/*.c")

# dir json
INCLUDE_DIRECTORIES("${COMMON}/third/json/")
FILE(GLOB json "${COMMON}/third/json/*.cpp")

LINK_DIRECTORIES("/usr/local/lib/")

ADD_LIBRARY(im SHARED  ${root} ${threadpool}  ${xml}  ${redis}  ${base} ${protobuf} ${node} ${json} )
INSTALL(FILES libim.so   DESTINATION lib)
