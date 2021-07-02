cmake_minimum_required(VERSION 3.1.0)
project(sbt-instrumentation CXX)

# --------------------------------------------------
# Build type
# --------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type not set. Setting default.")
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "RelWithDebInfo"
                                                     "MinSizeRel" "Release")

# --------------------------------------------------
# Compilation flags
# --------------------------------------------------
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -fno-rtti")

# --------------------------------------------------
# LLVM
# --------------------------------------------------
find_package(LLVM 3.9.1 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}")
include(AddLLVM)

# TODO: Move this to the section above when we switch to C++14
# LLVM 10 and newer require at least C++14 standard
if(${LLVM_PACKAGE_VERSION} VERSION_GREATER "9.0")
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  # otherwise we need C++11 standard
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")
message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")

option(LLVM_LINK_DYLIB "Link with LLVM dynamically" ON)
if(LLVM_LINK_DYLIB)
  message(STATUS "LLVM linking: dynamic")
  set(LLVM_LIBS LLVM)
else()
  message(STATUS "LLVM linking: static")
  # Find the libraries that correspond to the LLVM components
  # that we wish to use
  llvm_map_components_to_libnames(LLVM_LIBS bitwriter code irreader
                                            linker support)
endif()

# --------------------------------------------------
# DG
# --------------------------------------------------
add_definitions(-DHAVE_LLVM)

if(DG_PATH)
  include_directories(${DG_PATH}/include/)
else()
  include_directories(${CMAKE_SOURCE_DIR}/dg/include)
  set(DG_PATH ${CMAKE_SOURCE_DIR}/dg)
endif()

# Those libraries we'll use, since we want to use the LLVM points-to analysis
find_library(libdganalysis dganalysis PATHS ${DG_PATH}/lib/)
find_library(libdgllvmpta dgllvmpta PATHS ${DG_PATH}/lib/)
find_library(libdgpta dgpta PATHS ${DG_PATH}/lib/)

if(libdganalysis AND libdgllvmpta AND libdgpta)
  message(STATUS "DG libraries found: ${libdganalysis}, ${libdgllvmpta}, ${libdgpta}")
  set(DG_LIBS ${libdganalysis} ${libdgllvmpta} ${libdgpta})
else()
  message(FATAL_ERROR "DG not found! Try specifying -DDG_PATH=<path>.")
endif()

option(LLVM_LINK_DYLIB "Link with LLVM dynamically" ON)
if (LLVM_LINK_DYLIB)
	message(STATUS "LLVM linking: dynamic")
	if (${LLVM_PACKAGE_VERSION} VERSION_LESS "3.8")
		set(llvm_libs LLVM-${LLVM_PACKAGE_VERSION})
	else()
		# only LLVM 3.8+ provide unversioned library
		set(llvm_libs LLVM)
	endif()
else()
	# Find the libraries that correspond to the LLVM components
	# that we wish to use
	if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
		llvm_map_components_to_libnames(llvm_libs support core irreader bitwriter linker)
	else()
		llvm_map_components_to_libraries(llvm_libs support core irreader bitwriter linker)
	endif()
endif()

# --------------------------------------------------
# Global includes
# --------------------------------------------------
include_directories(include)
include_directories(analyses)

# --------------------------------------------------
# Subdirectories
# --------------------------------------------------
add_subdirectory(analyses)
add_subdirectory(instrumentations)
add_subdirectory(src)
add_subdirectory(tests)
