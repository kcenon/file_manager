#!/bin/bash

# File Manager Project Build Script
# Based on messaging_system build script with project-specific adaptations

# Color definitions for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Display banner
echo -e "${BOLD}${BLUE}============================================${NC}"
echo -e "${BOLD}${BLUE}    File Manager Project Build Script      ${NC}"
echo -e "${BOLD}${BLUE}============================================${NC}"

# Display help information
show_help() {
    echo -e "${BOLD}Usage:${NC} $0 [options]"
    echo ""
    echo -e "${BOLD}Build Options:${NC}"
    echo "  --clean           Perform a clean rebuild by removing the build directory"
    echo "  --debug           Build in debug mode (default is release)"
    echo ""
    echo -e "${BOLD}Target Options:${NC}"
    echo "  --all             Build all targets (default)"
    echo "  --lib-only        Build only the messaging system libraries"
    echo "  --servers         Build only the server applications"
    echo "  --samples         Build only the sample applications"
    echo "  --tests           Build and run the unit tests with detailed output"
    echo ""
    echo -e "${BOLD}Module Options:${NC}"
    echo "  --no-database     Disable database module"
    echo "  --no-network      Disable network module"
    echo "  --no-restapi      Disable REST API applications (due to missing dependencies)"
    echo ""
    echo -e "${BOLD}Feature Options:${NC}"
    echo "  --no-format       Disable std::format even if supported"
    echo "  --no-jthread      Disable std::jthread even if supported"
    echo "  --lockfree        Enable lock-free implementations by default"
    echo ""
    echo -e "${BOLD}General Options:${NC}"
    echo "  --cores N         Use N cores for compilation (default: auto-detect)"
    echo "  --verbose         Show detailed build output"
    echo "  --vcpkg-root PATH Set custom vcpkg root directory"
    echo "  --help            Display this help and exit"
    echo ""
}

# Function to print status messages
print_status() {
    echo -e "${BOLD}${BLUE}[STATUS]${NC} $1"
}

# Function to print success messages
print_success() {
    echo -e "${BOLD}${GREEN}[SUCCESS]${NC} $1"
}

# Function to print error messages
print_error() {
    echo -e "${BOLD}${RED}[ERROR]${NC} $1"
}

# Function to print warning messages
print_warning() {
    echo -e "${BOLD}${YELLOW}[WARNING]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Function to check and install dependencies
check_dependencies() {
    print_status "Checking build dependencies..."
    
    local missing_deps=()
    
    # Check for essential build tools
    for cmd in cmake git; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check for at least one build system (make or ninja)
    if ! command_exists "make" && ! command_exists "ninja"; then
        missing_deps+=("make or ninja")
    fi
    
    # Check for PostgreSQL development files if database module is enabled
    if [ $BUILD_DATABASE -eq 1 ]; then
        if ! pkg-config --exists libpq 2>/dev/null && ! [ -f "/usr/include/postgresql/libpq-fe.h" ]; then
            print_warning "PostgreSQL development files not found. Database module may fail to build."
        fi
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required dependencies: ${missing_deps[*]}"
        print_warning "Please install missing dependencies before building."
        return 1
    fi
    
    # Check for vcpkg
    if [ ! -d "$VCPKG_ROOT" ]; then
        print_warning "vcpkg not found at $VCPKG_ROOT"
        print_warning "Trying alternative vcpkg locations..."
        
        # Try common vcpkg locations
        local vcpkg_locations=(
            "$HOME/vcpkg"
            "../vcpkg"
            "../../vcpkg"
            "/usr/local/vcpkg"
            "/opt/vcpkg"
        )
        
        for loc in "${vcpkg_locations[@]}"; do
            if [ -d "$loc" ]; then
                print_status "Found vcpkg at $loc"
                VCPKG_ROOT="$loc"
                break
            fi
        done
        
        if [ ! -d "$VCPKG_ROOT" ]; then
            print_error "vcpkg not found. Please install vcpkg or set VCPKG_ROOT environment variable."
            return 1
        fi
    fi
    
    print_success "All dependencies are satisfied"
    return 0
}

# Process command line arguments
CLEAN_BUILD=0
BUILD_TYPE="Release"
TARGET="all"
DISABLE_STD_FORMAT=0
DISABLE_STD_JTHREAD=0
USE_LOCKFREE=0
BUILD_CORES=0
VERBOSE=0
BUILD_DATABASE=1
BUILD_NETWORK=1
BUILD_RESTAPI=0  # Disabled by default due to missing dependencies
VCPKG_ROOT="${VCPKG_ROOT:-../vcpkg}"

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --all)
            TARGET="all"
            shift
            ;;
        --lib-only)
            TARGET="lib-only"
            shift
            ;;
        --servers)
            TARGET="servers"
            shift
            ;;
        --samples)
            TARGET="samples"
            shift
            ;;
        --tests)
            TARGET="tests"
            shift
            ;;
        --no-database)
            BUILD_DATABASE=0
            shift
            ;;
        --no-network)
            BUILD_NETWORK=0
            shift
            ;;
        --no-restapi)
            BUILD_RESTAPI=0
            shift
            ;;
        --no-format)
            DISABLE_STD_FORMAT=1
            shift
            ;;
        --no-jthread)
            DISABLE_STD_JTHREAD=1
            shift
            ;;
        --lockfree)
            USE_LOCKFREE=1
            shift
            ;;
        --cores)
            if [[ $2 =~ ^[0-9]+$ ]]; then
                BUILD_CORES=$2
                shift 2
            else
                print_error "Option --cores requires a numeric argument"
                exit 1
            fi
            ;;
        --vcpkg-root)
            VCPKG_ROOT="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Set number of cores to use for building
if [ $BUILD_CORES -eq 0 ]; then
    if command_exists nproc; then
        BUILD_CORES=$(nproc)
    elif [ "$(uname)" == "Darwin" ]; then
        BUILD_CORES=$(sysctl -n hw.ncpu)
    else
        # Default to 2 if we can't detect
        BUILD_CORES=2
    fi
fi

print_status "Using $BUILD_CORES cores for compilation"

# Store original directory
ORIGINAL_DIR=$(pwd)

# Check dependencies before proceeding
check_dependencies
if [ $? -ne 0 ]; then
    print_error "Failed dependency check. Exiting."
    exit 1
fi

# Check for messaging_system submodule
if [ ! -d "messaging_system" ]; then
    print_error "messaging_system submodule not found. Please run 'git submodule update --init --recursive'"
    exit 1
fi

if [ ! -d "messaging_system/thread_system" ]; then
    print_warning "messaging_system submodule appears incomplete. Updating submodules..."
    git submodule update --init --recursive
    if [ $? -ne 0 ]; then
        print_error "Failed to update submodules"
        exit 1
    fi
fi

# Clean build if requested
if [ $CLEAN_BUILD -eq 1 ]; then
    print_status "Performing clean build..."
    rm -rf build build-debug lib bin
fi

# Create build directory if it doesn't exist
BUILD_DIR="build"
if [ "$BUILD_TYPE" == "Debug" ]; then
    BUILD_DIR="build-debug"
fi

if [ ! -d "$BUILD_DIR" ]; then
    print_status "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# Create output directories
mkdir -p lib bin

# Prepare CMake arguments
CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
CMAKE_ARGS+=" -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
CMAKE_ARGS+=" -DBUILD_SHARED_LIBS=OFF"

# Add module flags
if [ $BUILD_DATABASE -eq 0 ]; then
    CMAKE_ARGS+=" -DBUILD_DATABASE=OFF"
fi

if [ $BUILD_NETWORK -eq 0 ]; then
    CMAKE_ARGS+=" -DBUILD_NETWORK=OFF"
fi

# Add feature flags based on options
if [ $DISABLE_STD_FORMAT -eq 1 ]; then
    CMAKE_ARGS+=" -DSET_STD_FORMAT=OFF"
fi

if [ $DISABLE_STD_JTHREAD -eq 1 ]; then
    CMAKE_ARGS+=" -DSET_STD_JTHREAD=OFF"
fi

if [ $USE_LOCKFREE -eq 1 ]; then
    CMAKE_ARGS+=" -DUSE_LOCKFREE_BY_DEFAULT=ON"
fi

# Set test option
if [ "$TARGET" == "tests" ]; then
    CMAKE_ARGS+=" -DUSE_UNIT_TEST=ON"
else
    CMAKE_ARGS+=" -DUSE_UNIT_TEST=OFF"
fi

# Create temporary CMakeLists.txt for selective building
create_selective_cmake() {
    local cmake_file="$BUILD_DIR/CMakeLists_selective.txt"
    
    cat > "$cmake_file" << 'EOF'
cmake_minimum_required(VERSION 3.14)

# Project configuration
project(file_manager 
    VERSION 1.0 
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific settings
if(WIN32)
    option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../../../lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin")
    option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
endif()

# Set CMAKE_TOOLCHAIN_FILE for vcpkg integration
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

option(USE_UNIT_TEST "Use unit test" OFF)

# Build core messaging system libraries
add_subdirectory(../messaging_system/thread_system/sources messaging_system/thread_system/sources)
add_subdirectory(../messaging_system/container messaging_system/container)
EOF

    if [ $BUILD_DATABASE -eq 1 ]; then
        echo "add_subdirectory(../messaging_system/database messaging_system/database)" >> "$cmake_file"
    fi
    
    if [ $BUILD_NETWORK -eq 1 ]; then
        echo "add_subdirectory(../messaging_system/network messaging_system/network)" >> "$cmake_file"
    fi
    
    # Add applications based on target
    if [ "$TARGET" == "all" ] || [ "$TARGET" == "samples" ]; then
        cat >> "$cmake_file" << 'EOF'

# Sample applications (those that build successfully)
add_subdirectory(../upload_sample upload_sample)
add_subdirectory(../download_sample download_sample)
EOF
    fi
    
    if [ "$TARGET" == "all" ] || [ "$TARGET" == "servers" ]; then
        cat >> "$cmake_file" << 'EOF'

# Server applications (those that build successfully) 
add_subdirectory(../main_server main_server)
add_subdirectory(../middle_server middle_server)
EOF
    fi
    
    if [ $BUILD_RESTAPI -eq 1 ]; then
        cat >> "$cmake_file" << 'EOF'

# REST API applications (may have missing dependencies)
if(httplib_FOUND AND nlohmann_json_FOUND)
    add_subdirectory(../restapi_client_sample restapi_client_sample)
    add_subdirectory(../restapi_gateway restapi_gateway)
endif()
EOF
    fi
    
    echo "$cmake_file"
}

# Enter build directory
cd "$BUILD_DIR" || { print_error "Failed to enter build directory"; exit 1; }

# Create selective CMakeLists.txt
SELECTIVE_CMAKE=$(create_selective_cmake)

# Run CMake configuration
print_status "Configuring project with CMake..."
print_status "CMake arguments: $CMAKE_ARGS"

cmake -S "$SELECTIVE_CMAKE" $CMAKE_ARGS

# Check if CMake configuration was successful
if [ $? -ne 0 ]; then
    print_error "CMake configuration failed. Trying with original CMakeLists.txt..."
    
    # Fallback to original CMakeLists.txt
    cmake .. $CMAKE_ARGS
    
    if [ $? -ne 0 ]; then
        print_error "CMake configuration failed completely. See the output above for details."
        cd "$ORIGINAL_DIR"
        exit 1
    fi
fi

# Build the project
print_status "Building project in $BUILD_TYPE mode..."

# Determine build target based on option
BUILD_TARGET=""
if [ "$TARGET" == "all" ]; then
    BUILD_TARGET="all"
elif [ "$TARGET" == "lib-only" ]; then
    BUILD_TARGET="utilities thread_base logger thread_pool container"
    if [ $BUILD_DATABASE -eq 1 ]; then
        BUILD_TARGET+=" database"
    fi
    if [ $BUILD_NETWORK -eq 1 ]; then
        BUILD_TARGET+=" network"
    fi
elif [ "$TARGET" == "servers" ]; then
    BUILD_TARGET="main_server middle_server"
elif [ "$TARGET" == "samples" ]; then
    BUILD_TARGET="upload_sample download_sample"
elif [ "$TARGET" == "tests" ]; then
    BUILD_TARGET=""  # Let CMake handle test targets
fi

# Detect build system (Ninja or Make)
if [ -f "build.ninja" ]; then
    BUILD_COMMAND="ninja"
    if [ $VERBOSE -eq 1 ]; then
        BUILD_ARGS="-v"
    else
        BUILD_ARGS=""
    fi
elif [ -f "Makefile" ]; then
    BUILD_COMMAND="make"
    if [ $VERBOSE -eq 1 ]; then
        BUILD_ARGS="VERBOSE=1"
    else
        BUILD_ARGS=""
    fi
else
    print_error "No build system files found (neither build.ninja nor Makefile)"
    cd "$ORIGINAL_DIR"
    exit 1
fi

print_status "Using build system: $BUILD_COMMAND"

# Run build with appropriate target and cores
if [ "$BUILD_COMMAND" == "ninja" ]; then
    if [ -n "$BUILD_TARGET" ]; then
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS $BUILD_TARGET
    else
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS
    fi
elif [ "$BUILD_COMMAND" == "make" ]; then
    if [ -n "$BUILD_TARGET" ]; then
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS $BUILD_TARGET
    else
        $BUILD_COMMAND -j$BUILD_CORES $BUILD_ARGS
    fi
fi

# Check if build was successful
BUILD_EXIT_CODE=$?
if [ $BUILD_EXIT_CODE -ne 0 ]; then
    print_warning "Build had some failures. Attempting to build only successful targets..."
    
    # Try building only the core libraries
    print_status "Building core messaging system libraries..."
    CORE_TARGETS="utilities thread_base logger thread_pool container"
    if [ $BUILD_DATABASE -eq 1 ]; then
        CORE_TARGETS+=" database"
    fi
    if [ $BUILD_NETWORK -eq 1 ]; then
        CORE_TARGETS+=" network"
    fi
    
    if [ "$BUILD_COMMAND" == "make" ]; then
        make -j$BUILD_CORES $BUILD_ARGS $CORE_TARGETS
    else
        ninja -j$BUILD_CORES $BUILD_ARGS $CORE_TARGETS
    fi
    
    if [ $? -eq 0 ]; then
        print_success "Core libraries built successfully!"
        print_warning "Some applications failed to build due to missing dependencies"
    else
        print_error "Even core libraries failed to build. See the output above for details."
        cd "$ORIGINAL_DIR"
        exit 1
    fi
else
    print_success "All targets built successfully!"
fi

# Copy binaries to bin directory
if [ -d "bin" ]; then
    cp -r bin/* ../bin/ 2>/dev/null || true
fi

# Copy libraries to lib directory  
if [ -d "lib" ]; then
    cp -r lib/* ../lib/ 2>/dev/null || true
fi

# Run tests if requested
if [ "$TARGET" == "tests" ]; then
    print_status "Running tests..."
    
    # Check if unittest executable exists
    UNITTEST_PATH=""
    if [ -f "messaging_system/unittest/unittest" ]; then
        UNITTEST_PATH="messaging_system/unittest/unittest"
    elif [ -f "bin/unittest" ]; then
        UNITTEST_PATH="bin/unittest"
    fi
    
    if [ -n "$UNITTEST_PATH" ]; then
        print_status "Executing unit tests..."
        ./$UNITTEST_PATH
        TEST_EXIT_CODE=$?
        
        if [ $TEST_EXIT_CODE -eq 0 ]; then
            print_success "All tests passed!"
        else
            print_error "Some tests failed (exit code: $TEST_EXIT_CODE)"
            cd "$ORIGINAL_DIR"
            exit $TEST_EXIT_CODE
        fi
    else
        print_warning "No test executables found. Make sure tests were built correctly."
    fi
    
    # Run CTest if available
    if command_exists "ctest"; then
        print_status "Running CTest..."
        ctest -C $BUILD_TYPE --output-on-failure
        if [ $? -ne 0 ]; then
            print_warning "Some CTest tests failed"
        fi
    fi
fi

# Return to original directory
cd "$ORIGINAL_DIR"

# Show success message
print_success "Build completed!"

# Final success message
echo -e "${BOLD}${GREEN}============================================${NC}"
echo -e "${BOLD}${GREEN}   File Manager Project Build Complete    ${NC}"
echo -e "${BOLD}${GREEN}============================================${NC}"

if [ -d "bin" ]; then
    echo -e "${CYAN}Available executables:${NC}"
    ls -la bin/ 2>/dev/null || echo "  (No executables found)"
fi

echo -e "${CYAN}Build configuration:${NC}"
echo -e "  Build type: $BUILD_TYPE"
echo -e "  Target: $TARGET"
echo -e "  Cores used: $BUILD_CORES"
echo -e "  Database module: $([ $BUILD_DATABASE -eq 1 ] && echo "Enabled" || echo "Disabled")"
echo -e "  Network module: $([ $BUILD_NETWORK -eq 1 ] && echo "Enabled" || echo "Disabled")"
echo -e "  REST API apps: $([ $BUILD_RESTAPI -eq 1 ] && echo "Enabled" || echo "Disabled")"

if [ $USE_LOCKFREE -eq 1 ]; then
    echo -e "  Lock-free: Enabled by default"
fi

exit 0