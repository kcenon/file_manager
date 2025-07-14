#!/usr/bin/env python3
import os
import subprocess
import sys

def run_command(cmd, description):
    """Run a shell command and capture output"""
    print(f"[INFO] {description}")
    print(f"[CMD] {cmd}")
    
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd='/Users/dongcheolshin/Sources/file_manager')
        
        if result.stdout:
            print(f"[STDOUT] {result.stdout}")
        if result.stderr:
            print(f"[STDERR] {result.stderr}")
        
        print(f"[EXIT CODE] {result.returncode}")
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        print(f"[ERROR] Exception occurred: {e}")
        return False, "", str(e)

def main():
    """Main build process"""
    print("=== File Manager Build Helper ===")
    
    # Change to file_manager directory
    os.chdir('/Users/dongcheolshin/Sources/file_manager')
    print(f"Working directory: {os.getcwd()}")
    
    # Step 1: Clean build directories
    print("\n--- Step 1: Cleaning build directories ---")
    success, _, _ = run_command("rm -rf build build-debug lib bin", "Removing old build directories")
    
    # Step 2: Create new build directory
    print("\n--- Step 2: Creating build directory ---")
    success, _, _ = run_command("mkdir -p build", "Creating build directory")
    
    # Step 3: Change to build directory
    os.chdir('build')
    print(f"Changed to build directory: {os.getcwd()}")
    
    # Step 4: Run CMake configuration
    print("\n--- Step 3: Running CMake configuration ---")
    cmake_cmd = "cmake -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .."
    success, stdout, stderr = run_command(cmake_cmd, "Configuring project with CMake")
    
    if not success:
        print("\n[ERROR] CMake configuration failed!")
        print("Trying without vcpkg...")
        success, stdout, stderr = run_command("cmake -DCMAKE_BUILD_TYPE=Release ..", "Configuring project without vcpkg")
        
        if not success:
            print("\n[FATAL] CMake configuration completely failed!")
            return False
    
    # Step 5: Build the project
    print("\n--- Step 4: Building the project ---")
    # Try different build systems
    if os.path.exists('build.ninja'):
        build_cmd = "ninja"
    elif os.path.exists('Makefile'):
        build_cmd = "make -j$(nproc)"
    else:
        build_cmd = "cmake --build . --config Release"
    
    success, stdout, stderr = run_command(build_cmd, "Building the project")
    
    if not success:
        print("\n[WARNING] Full build failed, trying core libraries only...")
        core_cmd = "make utilities thread_base logger thread_pool container network"
        success, stdout, stderr = run_command(core_cmd, "Building core libraries")
    
    # Step 6: List built artifacts
    print("\n--- Step 5: Listing built artifacts ---")
    run_command("find . -name '*.a' -o -name '*.so' -o -name '*.dylib' | head -20", "Finding built libraries")
    run_command("find . -type f -executable | grep -v CMake | head -20", "Finding built executables")
    
    return success

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)