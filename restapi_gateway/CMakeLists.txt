CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

SET(PROGRAM_NAME restapi_gateway)
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)

PROJECT(${PROGRAM_NAME})

# Find required packages
FIND_PACKAGE(httplib CONFIG)
FIND_PACKAGE(nlohmann_json CONFIG REQUIRED)

# Check if we have cpp-httplib
if(httplib_FOUND)
    SET(SOURCES restapi_gateway.cpp)
    
    ADD_EXECUTABLE(${PROGRAM_NAME} ${SOURCES})
    
    INCLUDE_DIRECTORIES(../messaging_system/thread_system/sources/utilities)
    INCLUDE_DIRECTORIES(../messaging_system/thread_system/sources/utilities/parsing)
    INCLUDE_DIRECTORIES(../messaging_system/container)
    INCLUDE_DIRECTORIES(../messaging_system/network)

    # We don't have a real network library to link, it's a dummy target
    # ADD_DEPENDENCIES(${PROGRAM_NAME} network)
    TARGET_LINK_LIBRARIES(${PROGRAM_NAME} PUBLIC 
        httplib::httplib
        nlohmann_json::nlohmann_json
    )
else()
    message(STATUS "cpp-httplib not found, skipping ${PROGRAM_NAME}")
endif()