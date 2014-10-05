cmake_minimum_required(VERSION 2.8.4)
project(MerlionServerCore)

find_package(Boost 1.55.0 REQUIRED COMPONENTS system regex thread log filesystem)
include_directories(${Boost_INCLUDE_DIRS})
list(APPEND LIB_LIST ${Boost_LIBRARIES})

find_package(OpenSSL 1.0.1 REQUIRED)
include_directories(${OpenSSL_INCLUDE_DIRS})
list(APPEND LIB_LIST ${OpenSSL_LIBRARIES})

set(SOURCE_FILES
Library.cpp
Master.cpp
Exceptions.cpp
MasterNodeConnection.cpp
MasterNodeClientStream.cpp
MasterVersionProvider.cpp
MasterNode.cpp
MasterClient.cpp
Node.cpp
NodeDomain.cpp
NodeClient.cpp
NodeVersionLoader.cpp
Utils.cpp
Packet.cpp
Logging.cpp
LikeMatcher.cpp
)
add_library(MerlionServerCore SHARED ${SOURCE_FILES})
target_link_libraries(MerlionServerCore ${LIB_LIST})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
