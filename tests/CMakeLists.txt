cmake_minimum_required(VERSION 3.12..3.18)
project(vr_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Debug)

include(FetchContent)
FetchContent_Declare(
    fetch_catch
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.1)
FetchContent_GetProperties(fetch_catch)
if(NOT fetch_catch_POPULATED)
    FetchContent_Populate(fetch_catch)
    add_subdirectory(${fetch_catch_SOURCE_DIR} ${fetch_catch_BINARY_DIR})
endif()

add_library(VRPlugin SHARED
	${CMAKE_SOURCE_DIR}/analyses/value_relations_plugin.cpp
)

target_link_directories(VRPlugin PRIVATE ${CMAKE_INSTALL_LIBDIR})
target_link_libraries(VRPlugin PRIVATE
    dgllvmvra dgvra dganalysis
)

add_executable(${PROJECT_NAME} tests-main.cpp tests.cpp ../src/jsoncpp.cpp)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/../dg/include/dg/llvm/ValueRelations/)
target_link_libraries(${PROJECT_NAME} PUBLIC Catch2::Catch2 ${llvm_libs} VRPlugin)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -g ${COLORS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tested-files.json ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)