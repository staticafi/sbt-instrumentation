cmake_minimum_required(VERSION 3.12..3.18)
project(vr_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_library(VRPlugin STATIC	${CMAKE_SOURCE_DIR}/analyses/value_relations_plugin.cpp)
target_link_libraries(VRPlugin PRIVATE ${dg_libs})
target_compile_options(VRPlugin PRIVATE -Wall -Wextra -pedantic -g ${COLORS})

add_executable(${PROJECT_NAME} tests-main.cpp tests.cpp ../analyses/value_relations_plugin.cpp ../src/jsoncpp.cpp)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/../dg/include/dg/llvm/ValueRelations/)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2 ${llvm_libs} VRPlugin ${JSON_LIBS})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -g ${COLORS})

add_custom_target(test_all ./${PROJECT_NAME} --use-colour yes
                      DEPENDS ${PROJECT_NAME}
                      COMMENT "Running tests"
                      WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
                      VERBATIM)
add_custom_target(test_correct ./${PROJECT_NAME} -c all_correct --use-colour yes
                      DEPENDS ${PROJECT_NAME}
                      COMMENT "Running tests"
                      WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
                      VERBATIM)
add_custom_target(test_problematic ./${PROJECT_NAME} -c problematic --use-colour yes
                      DEPENDS ${PROJECT_NAME}
                      COMMENT "Running tests"
                      WORKING_DIRECTORY ${CMAKE_BUILD_DIR}
                      VERBATIM)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tested-files.json ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)