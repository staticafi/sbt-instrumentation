cmake_minimum_required(VERSION 2.8.8)
project(instr)

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -std=c++11 -Wall -Wextra")

# Define add_llvm_* macro's.
find_package(LLVM REQUIRED CONFIG)
include(AddLLVM)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
#include(LLVMConfig)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_path(LLVM_SRC_PATH include/llvm/Module.h)
find_path(LLVM_BUILD_PATH include/llvm/Intrinsics.gen)

if (LLVM_SRC_PATH)
  include_directories(${LLVM_SRC_PATH}/include)
  SET(LLVM_INCLUDE_DIRS ${LLVM_SRC_PATH}/include)
  message(STATUS "Got LLVM_SRC_PATH ${LLVM_SRC_PATH}")
endif(LLVM_SRC_PATH)
if (LLVM_BUILD_PATH)
  include_directories(${LLVM_BUILD_PATH}/include)
  link_directories(${LLVM_BUILD_PATH}/lib)
  SET(LLVM_LIBRARY_DIRS ${LLVM_BUILD_PATH}/lib)
  message(STATUS "Got LLVM_BUILD_PATH ${LLVM_BUILD_PATH}")
endif(LLVM_BUILD_PATH)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

add_subdirectory(src)
add_subdirectory(include)
include_directories(${CMAKE_SOURCE_DIR}/include)

SET(JSON_FILES 
    src/jsoncpp.cpp
    include/json.h
    include/json-forwards.h
)

SET(SOURCE_FILES
    lib/instr_log.hpp
    lib/rewriter.hpp
    src/rewriter.cpp
    src/instr_log.cpp
)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libraries(llvm_libs support core irreader bitwriter)

#ADD_LIBRARY(instr_model ${JSON_FILES} ${SOURCE_FILES})
set(LLVM_LINK_COMPONENTS core engine support)
add_executable(LLVMinstr src/instr.cpp ${JSON_FILES} ${SOURCE_FILES})
TARGET_LINK_LIBRARIES(LLVMinstr ${llvm_libs})

