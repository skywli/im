PROJECT(im_dispatch)

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

set(COMMON  "../../common")
#set(CMAKE_BUILD_TYPE debug)


AUX_SOURCE_DIRECTORY(. DIR_SRCS)

# dir root
INCLUDE_DIRECTORIES("./")
FILE(GLOB root "./*.cc")


# dir COMMON
INCLUDE_DIRECTORIES("${COMMON}")

# dir base
INCLUDE_DIRECTORIES("${COMMON}/base/")

# dir node
INCLUDE_DIRECTORIES("${COMMON}/node/")


# dir protobuf
INCLUDE_DIRECTORIES("${COMMON}/protobuf/")

# dir xml
INCLUDE_DIRECTORIES("${COMMON}/third/tinyxml/")

# dir redis
INCLUDE_DIRECTORIES("${COMMON}/third/redis/")
LINK_DIRECTORIES("/usr/local/lib/")
LINK_DIRECTORIES("${COMMON}")


#ADD_EXECUTABLE(dispatch ${DIR_SRCS} ${root} ${COMMON} ${redis} ${xml} ${base} ${protobuf} ${node} )
add_executable(im_dispatch  ${DIR_SRCS} ${root})
 TARGET_LINK_LIBRARIES(im_dispatch protobuf pthread log4cpp event_pthreads event jemalloc im)
 INSTALL(TARGETS im_dispatch  RUNTIME DESTINATION bin)
 INSTALL(FILES dispatch.xml  DESTINATION conf)
