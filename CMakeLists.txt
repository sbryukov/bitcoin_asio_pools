cmake_minimum_required(VERSION 2.6)
PROJECT(bitcooin-asio-pools)

SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost 1.53.0 REQUIRED COMPONENTS system thread regex )
IF(Boost_FOUND)
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
  LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

SET(USED_LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY} jsoncpp)

ADD_EXECUTABLE(bitcoin_asio_pools pools.cpp pools-server.cpp pools-conn.cpp)
TARGET_LINK_LIBRARIES(bitcoin_asio_pools ${USED_LIBS})

