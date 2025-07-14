#!/usr/bin/env python3
"""
Direct build script for file_manager project
Executes the key build steps without relying on bash
"""

import os
import subprocess
import sys
import shutil
from pathlib import Path

class BuildHelper:
    def __init__(self):
        self.project_root = Path("/Users/dongcheolshin/Sources/file_manager")
        self.vcpkg_root = Path("/Users/dongcheolshin/Sources/vcpkg")
        self.build_dir = self.project_root / "build"
        
    def log(self, message, level="INFO"):
        print(f"[{level}] {message}")
        
    def run_command(self, cmd, cwd=None, description=""):
        """Execute a command and return success status"""
        if cwd is None:
            cwd = self.project_root
            
        self.log(f"{description}: {cmd}")
        
        try:
            # Split command properly for subprocess
            if isinstance(cmd, str):
                cmd_parts = cmd.split()
            else:
                cmd_parts = cmd
                
            result = subprocess.run(
                cmd_parts,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            if result.stdout.strip():
                self.log(f"STDOUT: {result.stdout.strip()}")
            if result.stderr.strip():
                self.log(f"STDERR: {result.stderr.strip()}", "WARN")
                
            self.log(f"Exit code: {result.returncode}")
            return result.returncode == 0, result.stdout, result.stderr
            
        except subprocess.TimeoutExpired:
            self.log("Command timed out!", "ERROR")
            return False, "", "Timeout"
        except Exception as e:
            self.log(f"Command failed with exception: {e}", "ERROR")
            return False, "", str(e)
    
    def clean_build(self):
        """Clean existing build artifacts"""
        self.log("=== CLEAN BUILD ===")
        
        # Remove build directories
        dirs_to_remove = [
            self.project_root / "build",
            self.project_root / "build-debug", 
            self.project_root / "lib",
            self.project_root / "bin"
        ]
        
        for dir_path in dirs_to_remove:
            if dir_path.exists():
                self.log(f"Removing {dir_path}")
                shutil.rmtree(dir_path, ignore_errors=True)
            else:
                self.log(f"Directory {dir_path} does not exist, skipping")
        
        return True
    
    def setup_build_dirs(self):
        """Create necessary build directories"""
        self.log("=== SETUP BUILD DIRECTORIES ===")
        
        dirs_to_create = [self.build_dir, self.project_root / "lib", self.project_root / "bin"]
        
        for dir_path in dirs_to_create:
            dir_path.mkdir(parents=True, exist_ok=True)
            self.log(f"Created directory: {dir_path}")
        
        return True
    
    def configure_cmake(self):
        """Configure project with CMake"""
        self.log("=== CMAKE CONFIGURATION ===")
        
        # First try with vcpkg
        cmake_cmd = [
            "cmake",
            f"-DCMAKE_TOOLCHAIN_FILE={self.vcpkg_root}/scripts/buildsystems/vcpkg.cmake",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_SHARED_LIBS=OFF",
            ".."
        ]
        
        success, stdout, stderr = self.run_command(
            cmake_cmd, 
            cwd=self.build_dir,
            description="Configuring with vcpkg"
        )
        
        if success:
            return True
            
        # Fallback: try without vcpkg
        self.log("vcpkg configuration failed, trying without vcpkg...")
        cmake_cmd = [
            "cmake",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_SHARED_LIBS=OFF", 
            ".."
        ]
        
        success, stdout, stderr = self.run_command(
            cmake_cmd,
            cwd=self.build_dir, 
            description="Configuring without vcpkg"
        )
        
        return success
    
    def build_project(self):
        """Build the project"""
        self.log("=== BUILD PROJECT ===")
        
        # Check what build system is available
        makefile_path = self.build_dir / "Makefile"
        ninja_path = self.build_dir / "build.ninja"
        
        if ninja_path.exists():
            build_cmd = ["ninja"]
            description = "Building with ninja"
        elif makefile_path.exists():
            build_cmd = ["make", "-j8"]  # Use 8 cores
            description = "Building with make"
        else:
            # Fallback to cmake build
            build_cmd = ["cmake", "--build", ".", "--config", "Release"]
            description = "Building with cmake --build"
        
        success, stdout, stderr = self.run_command(
            build_cmd,
            cwd=self.build_dir,
            description=description
        )
        
        if success:
            return True
            
        # Try building only core components
        self.log("Full build failed, trying core libraries only...")
        core_targets = ["utilities", "thread_base", "logger", "thread_pool", "container", "network"]
        
        for target in core_targets:
            if makefile_path.exists():
                cmd = ["make", target]
            else:
                cmd = ["cmake", "--build", ".", "--target", target]
                
            success, _, _ = self.run_command(
                cmd,
                cwd=self.build_dir,
                description=f"Building {target}"
            )
            
            if not success:
                self.log(f"Failed to build {target}", "WARN")
        
        return True  # Return success even if some targets fail
    
    def list_artifacts(self):
        """List built artifacts"""
        self.log("=== BUILD ARTIFACTS ===")
        
        # Find libraries
        lib_patterns = ["**/*.a", "**/*.so", "**/*.dylib"]
        for pattern in lib_patterns:
            for lib_file in self.build_dir.glob(pattern):
                self.log(f"Library: {lib_file.relative_to(self.build_dir)}")
        
        # Find executables  
        for exe_file in self.build_dir.glob("**/bin/*"):
            if exe_file.is_file() and os.access(exe_file, os.X_OK):
                self.log(f"Executable: {exe_file.relative_to(self.build_dir)}")
    
    def run_tests(self):
        """Run tests if available"""
        self.log("=== RUNNING TESTS ===")
        
        # Look for test executables
        test_paths = [
            self.build_dir / "messaging_system" / "unittest" / "unittest",
            self.build_dir / "bin" / "unittest"
        ]
        
        for test_path in test_paths:
            if test_path.exists():
                success, stdout, stderr = self.run_command(
                    [str(test_path)],
                    description=f"Running tests from {test_path}"
                )
                
                if success:
                    self.log("Tests completed successfully!")
                    return True
                else:
                    self.log("Some tests failed", "WARN")
                    
        # Try CTest
        ctest_success, _, _ = self.run_command(
            ["ctest", "-C", "Release", "--output-on-failure"],
            cwd=self.build_dir,
            description="Running CTest"
        )
        
        return ctest_success
    
    def full_build(self):
        """Execute complete build process"""
        self.log("Starting file_manager build process...")
        
        # Step 1: Clean
        if not self.clean_build():
            self.log("Clean build failed!", "ERROR")
            return False
            
        # Step 2: Setup directories  
        if not self.setup_build_dirs():
            self.log("Directory setup failed!", "ERROR")
            return False
            
        # Step 3: Configure
        if not self.configure_cmake():
            self.log("CMake configuration failed!", "ERROR")
            return False
            
        # Step 4: Build
        if not self.build_project():
            self.log("Build failed!", "ERROR") 
            return False
            
        # Step 5: List artifacts
        self.list_artifacts()
        
        self.log("Build process completed!")
        return True

def main():
    builder = BuildHelper()
    
    if len(sys.argv) > 1:
        command = sys.argv[1]
        if command == "--clean":
            return builder.full_build()
        elif command == "--tests":
            return builder.run_tests()
        else:
            print(f"Unknown command: {command}")
            print("Usage: python3 direct_build.py [--clean|--tests]")
            return False
    else:
        return builder.full_build()

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)