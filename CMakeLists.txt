CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

SET(PROJECT_NAME messaging_system)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    OPTION(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
ELSE()
    SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../../lib")
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin")
    OPTION(BUILD_SHARED_LIBS "Build using shared libraries" ON)
ENDIF()

OPTION(USE_UNIT_TEST "Use unit test" ON)

# set the project name
PROJECT(${PROJECT_NAME} VERSION 1.0)

# cpp_libraries
ADD_SUBDIRECTORY(messaging_system/utilities)
ADD_SUBDIRECTORY(messaging_system/container)
ADD_SUBDIRECTORY(messaging_system/threads)
ADD_SUBDIRECTORY(messaging_system/database)
ADD_SUBDIRECTORY(messaging_system/network)
IF(USE_UNIT_TEST)
    ADD_SUBDIRECTORY(messaging_system/unittest)
ENDIF()

# cpp_samples
ADD_SUBDIRECTORY(upload_sample)
ADD_SUBDIRECTORY(download_sample)
ADD_SUBDIRECTORY(restapi_client_sample)

# micro_services
ADD_SUBDIRECTORY(main_server)
ADD_SUBDIRECTORY(middle_server)
ADD_SUBDIRECTORY(restapi_gateway)