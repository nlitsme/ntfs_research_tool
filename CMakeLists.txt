cmake_minimum_required(VERSION 3.10)
project(ntfstool)
set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_find")
include(cmake_find/boilerplate.cmake)



if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing")
endif()
find_package(itslib REQUIRED)
find_package(Boost REQUIRED date_time)

add_executable(ntfsrd ntfsrd.cpp)
target_link_libraries(ntfsrd itslib)
target_link_libraries(ntfsrd Boost::headers Boost::date_time)
target_link_directories(ntfsrd PUBLIC ${Boost_LIBRARY_DIR_RELEASE})
