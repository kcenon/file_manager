CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

SET(PROGRAM_NAME upload_sample)
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)

SET(SOURCES upload_sample.cpp)

PROJECT(${PROGRAM_NAME})

ADD_EXECUTABLE(${PROGRAM_NAME} ${SOURCES})

# Find required packages
find_package(fmt CONFIG REQUIRED)
find_package(Threads REQUIRED)

TARGET_INCLUDE_DIRECTORIES(${PROGRAM_NAME} PUBLIC 
    ../messaging_system/thread_system/sources/utilities
    ../messaging_system/thread_system/sources/utilities/parsing
    ../messaging_system
    ../messaging_system/container
    ../messaging_system/network
)

ADD_DEPENDENCIES(${PROGRAM_NAME} utilities container network)
TARGET_LINK_LIBRARIES(${PROGRAM_NAME} PUBLIC utilities container network fmt::fmt)