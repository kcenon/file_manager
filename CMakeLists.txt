CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

SET(PROJECT_NAME messaging_system)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

OPTION(USE_UNIT_TEST "Use unit test" ON)
OPTION(USE_TYPE_CONTAINER "Contains type container library" ON)

IF(USE_UNIT_TEST)
    ENABLE_TESTING()
    ADD_DEFINITIONS(-D__USE_UNIT_TEST__)
ENDIF()

IF(USE_TYPE_CONTAINER)
    ADD_DEFINITIONS(-D__USE_TYPE_CONTAINER__)
ENDIF()

# set the project name
PROJECT(${PROJECT_NAME} VERSION 1.0)

# cpp_libraries
ADD_SUBDIRECTORY(messaging_system/utilities)
IF(USE_TYPE_CONTAINER)
    ADD_SUBDIRECTORY(messaging_system/container)
ENDIF()
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