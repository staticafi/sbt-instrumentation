cmake_minimum_required(VERSION 2.8.8)
project(instr)

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -std=c++11 -Wall -Wextra")

# Define add_llvm_* macro's.
find_package(LLVM REQUIRED CONFIG)
#list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_path(LLVM_SRC_PATH include/llvm/Module.h)
find_path(LLVM_BUILD_PATH include/llvm/Intrinsics.gen)

if (LLVM_SRC_PATH)
  include_directories(${LLVM_SRC_PATH}/include)
endif(LLVM_SRC_PATH)
if (LLVM_BUILD_PATH)
  include_directories(${LLVM_BUILD_PATH}/include)
  link_directories(${LLVM_BUILD_PATH}/lib)
endif(LLVM_BUILD_PATH)

add_subdirectory(src)
add_subdirectory(include)

SET(    JSON_FILES 
        src/jsoncpp.cpp
        include/json.h
        include/json-forwards.h
)

SET(    SOURCE_FILES
        src/rewriter.hpp
)

ADD_LIBRARY( instr_model ${JSON_FILES} ${SOURCE_FILES})

add_executable(myexec src/instr.cpp ${JSON_FILES} ${SOURCE_FILES})

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader bitwriter)

TARGET_LINK_LIBRARIES(myexec  ${llvm_libs} instr_model )


