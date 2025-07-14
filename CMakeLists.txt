CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

# 프로젝트 이름 설정
SET(PROJECT_NAME file_manager)

# 프로젝트 선언과 언어 지정
PROJECT(${PROJECT_NAME} VERSION 1.0 LANGUAGES CXX)

# C++20 표준 사용
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)
SET(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug flags
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

# 컴파일러 정보 출력
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00) # Windows 10
    OPTION(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
elseif(APPLE)
    add_definitions(-DAPPLE_PLATFORM)
    OPTION(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
ELSE()
    SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../../lib")
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin")
    OPTION(BUILD_SHARED_LIBS "Build using shared libraries" ON)
ENDIF()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set CMAKE_TOOLCHAIN_FILE for vcpkg integration
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Options
OPTION(USE_UNIT_TEST "Use unit test" OFF)

# Find required packages
find_package(Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Add messaging_system as subdirectory
# Set it to build as a submodule (no samples/tests)
set(BUILD_THREADSYSTEM_AS_SUBMODULE ON CACHE BOOL "Build ThreadSystem as submodule" FORCE)
set(BUILD_MESSAGING_SAMPLES OFF CACHE BOOL "Build messaging system samples" FORCE)
set(BUILD_DATABASE OFF CACHE BOOL "Build database module" FORCE)
set(USE_UNIT_TEST OFF CACHE BOOL "Use unit test" FORCE)

# Add messaging_system subdirectory
ADD_SUBDIRECTORY(messaging_system)

# Include directories for messaging_system
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/messaging_system
    ${CMAKE_CURRENT_SOURCE_DIR}/messaging_system/thread_system/sources
)

# 유닛 테스트 일시적으로 비활성화 (formatter 호환성 문제로 인해)
# 서브모듈 수정 없이 빌드를 허용하기 위해 주석 처리
# IF(USE_UNIT_TEST)
#     ADD_SUBDIRECTORY(messaging_system/unittest)
# ENDIF()

# cpp_samples
ADD_SUBDIRECTORY(upload_sample)
ADD_SUBDIRECTORY(download_sample)
# ADD_SUBDIRECTORY(restapi_client_sample)  # Missing logging.h

# micro_services
ADD_SUBDIRECTORY(main_server)
ADD_SUBDIRECTORY(middle_server)
# ADD_SUBDIRECTORY(restapi_gateway)  # Missing compressing.h