PROJECT(im_imtest)

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

#set(CMAKE_BUILD_TYPE debug)

set(COMMON  "../../src/common")

AUX_SOURCE_DIRECTORY(. DIR_SRCS)

# dir root
INCLUDE_DIRECTORIES("./src")
FILE(GLOB root "./src/*.cc")

# dir protobuf
INCLUDE_DIRECTORIES("${COMMON}/protobuf/")
FILE(GLOB protobuf "${COMMON}/protobuf/*.cc")

# dir redis
INCLUDE_DIRECTORIES("${COMMON}/third/redis")
FILE(GLOB redis "${COMMON}/third/redis/hiredis/*.c")

#dir log4cpp
INCLUDE_DIRECTORIES("../../include/")

LINK_DIRECTORIES("/usr/local/lib/")

ADD_EXECUTABLE(imtest  ${DIR_SRCS} ${root} ${protobuf}  ${redis}  )
TARGET_LINK_LIBRARIES(imtest  libprotobuf.a  -lpthread log4cpp  libevent_pthreads.a libevent.a)
