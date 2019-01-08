
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

set(TOOLS  ".")
#set(CMAKE_BUILD_TYPE debug)


# dir tools
INCLUDE_DIRECTORIES("${TOOLS}")
FILE(GLOB tools "${TOOLS}/*.cc")

# dir base
INCLUDE_DIRECTORIES("${TOOLS}/base/")
FILE(GLOB base "${TOOLS}/base/*.cc")

# dir node
INCLUDE_DIRECTORIES("${TOOLS}/node/")
FILE(GLOB node "${TOOLS}/node/*.cc")


# dir protobuf
INCLUDE_DIRECTORIES("${TOOLS}/protobuf/")
FILE(GLOB protobuf "${TOOLS}/protobuf/*.cc")


# dir threadpool
INCLUDE_DIRECTORIES("${TOOLS}/threadpool/")


# dir xml
INCLUDE_DIRECTORIES("${TOOLS}/third/tinyxml/")
FILE(GLOB xml "${TOOLS}/third/tinyxml/*.cpp")

# dir redis
INCLUDE_DIRECTORIES("${TOOLS}/third/redis/")
FILE(GLOB redis "${TOOLS}/third/redis/hiredis/*.c")

# dir json
INCLUDE_DIRECTORIES("${TOOLS}/third/json/")
FILE(GLOB json "${TOOLS}/third/json/*.cpp")

LINK_DIRECTORIES("/usr/local/lib/")

ADD_LIBRARY(im SHARED  ${tools} ${threadpool}  ${xml}  ${redis}  ${base} ${protobuf} ${node} ${json} )
INSTALL(FILES libim.so   DESTINATION lib)
