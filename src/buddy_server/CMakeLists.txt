PROJECT(im_buddy_server)

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
INCLUDE_DIRECTORIES("./")
FILE(GLOB root "./*.cc")

# dir tools
INCLUDE_DIRECTORIES("${TOOLS}")

# dir base
INCLUDE_DIRECTORIES("${TOOLS}/base/")

# dir node
INCLUDE_DIRECTORIES("${TOOLS}/node/")


# dir protobuf
INCLUDE_DIRECTORIES("${TOOLS}/protobuf/")


# dir threadpool
INCLUDE_DIRECTORIES("${TOOLS}/threadpool/")


# dir xml
INCLUDE_DIRECTORIES("${TOOLS}/third/tinyxml/")

# dir redis
INCLUDE_DIRECTORIES("${TOOLS}/third/redis/")

LINK_DIRECTORIES("/usr/local/lib/")
LINK_DIRECTORIES("../tools")

#ADD_EXECUTABLE(buddy_server ${DIR_SRCS} ${root} ${tools} ${threadpool}  ${xml}  ${redis}  ${base} ${protobuf} ${node}  )
ADD_EXECUTABLE(im_buddy ${DIR_SRCS} ${root})
 TARGET_LINK_LIBRARIES(im_buddy protobuf pthread log4cpp event_pthreads event jemalloc im)
 INSTALL(TARGETS im_buddy RUNTIME DESTINATION bin)
 INSTALL(FILES buddy.xml  DESTINATION conf)
