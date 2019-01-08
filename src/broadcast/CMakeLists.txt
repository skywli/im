PROJECT(im_broadcast)

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

set(TOOLS  "../tools")
#set(CMAKE_BUILD_TYPE debug)


AUX_SOURCE_DIRECTORY(. DIR_SRCS)

# dir root
INCLUDE_DIRECTORIES("./core")
FILE(GLOB root "./core/*.cc")

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
LINK_DIRECTORIES("../../libs/")


ADD_EXECUTABLE(broadcast  ${DIR_SRCS} ${root} ${tools} ${base} ${protobuf} ${node} ${xml}  ${redis} ${json}  )
TARGET_LINK_LIBRARIES(broadcast  protobuf -lpthread log4cpp event_pthreads event jemalloc)
 INSTALL(TARGETS broadcast RUNTIME DESTINATION bin)
 INSTALL(FILES broadcast.xml  DESTINATION conf)
