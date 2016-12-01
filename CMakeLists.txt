cmake_minimum_required(VERSION 2.8.8)
project(instr)

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -std=c++11 -Wall -Wextra")

find_package(LLVM REQUIRED CONFIG)
if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
  include(LLVMConfig)
endif()
include(AddLLVM)

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
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib)
add_definitions(-DHAVE_LLVM)

if (DG_PATH)
  add_definitions("-DHAVE_DG")
  include_directories(${DG_PATH}/src/)
  link_directories(${DG_PATH}/src/)
  # those libraries we'll use. We need to use whole LLVMdg,
  # since we'll use the LLVM points-to analysis
  set(dg_libs "LLVMpta")
  message(STATUS "Got DG: ${DG_PATH}")
endif(DG_PATH)

include(GNUInstallDirs)
message(STATUS "CMAKE_INSTALL_LIBDIR: \"${CMAKE_INSTALL_LIBDIR}\"")

find_package(jsoncpp CONFIG)
if (jsoncpp_FOUND)
	set(JSON_LIBS jsoncpp_lib)
	message(STATUS "Using system jsoncpp library")
else()
	set(JSON_FILES
		src/jsoncpp.cpp
		include/json/json.h
		include/json/json-forwards.h
	)
endif()

SET(SOURCE_FILES
    lib/rewriter.hpp
    src/rewriter.cpp
    lib/instr_log.hpp
    src/instr_log.cpp
    lib/instr_plugin.hpp
    lib/instr_analyzer.hpp
    src/instr_analyzer.cpp
)

# Find the libraries that correspond to the LLVM components
# that we wish to use
if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
  llvm_map_components_to_libnames(llvm_libs support core irreader bitwriter)
else()
  llvm_map_components_to_libraries(llvm_libs support core irreader bitwriter)
endif()

#ADD_LIBRARY(instr_model ${JSON_FILES} ${SOURCE_FILES})
set(LLVM_LINK_COMPONENTS core engine support)
#set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
add_executable(LLVMinstr src/instr.cpp ${JSON_FILES} ${SOURCE_FILES})
TARGET_LINK_LIBRARIES(LLVMinstr ${llvm_libs} ${JSON_LIBS})
INSTALL(TARGETS LLVMinstr RUNTIME DESTINATION bin)

add_library(Points_to_plugin MODULE analyses/points_to_plugin.cpp lib/instr_plugin.hpp)
target_link_libraries(Points_to_plugin PRIVATE ${dg_libs} ${llvm_libs})
install(TARGETS Points_to_plugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

