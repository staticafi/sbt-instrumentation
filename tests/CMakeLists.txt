project(vr_tests)

set(CMAKE_BUILD_TYPE Debug)

# --------------------------------------------------
# Catch2
# --------------------------------------------------

# version of bundled Catch2
set(CATCH_VERSION 2.13.7)

# If we can find the same version or newer, use it as it may contain
# additional fixes relevant to the given OS
find_package(Catch2 ${CATCH_VERSION} QUIET CONFIG)
if(Catch2_FOUND)
    message(STATUS "Found Catch2 (found version \"${Catch2_VERSION}\")")
    # Issue a warning if there is a newer version, so that we will be notified
    # to update the bundled one.
    if(${Catch2_VERSION} VERSION_GREATER ${CATCH_VERSION})
        message(WARNING
            "The bundled Catch2 seems to be outdated: \"${CATCH_VERSION}\"\n"
            "Please report this to our issue tracker. Thank you!")
    endif()
else()
    message(STATUS "Using bundled Catch2")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()

# --------------------------------------------------
# find compatible clang
# --------------------------------------------------

# clang binary uses only major version as suffix since 7.0
if(${LLVM_PACKAGE_VERSION} VERSION_LESS "7.0")
    set(CLANG_BINARY clang-${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR})
else()
    set(CLANG_BINARY clang-${LLVM_VERSION_MAJOR})
endif()

# REQUIRED available with CMake 3.18+
find_program(LLVMCC ${CLANG_BINARY} HINTS ${LLVM_TOOLS_BINARY_DIR}
                                    # do not use ccache
                                    NO_SYSTEM_ENVIRONMENT_PATH)
if(NOT LLVMCC)
    message(FATAL_ERROR "Bitcode compiler: version compatible with \
                         LLVM ${LLVM_PACKAGE_VERSION} not found")
endif()
message(STATUS "Bitcode compiler: ${LLVMCC}")

# --------------------------------------------------
# Tests
# --------------------------------------------------

add_executable(${PROJECT_NAME} tests-main.cpp
                               tests.cpp
                               ${JSON_FILES}
                               ${CMAKE_SOURCE_DIR}/analyses/value_relations_plugin.cpp
)
target_compile_options(${PROJECT_NAME} PUBLIC -DLLVMCC="${LLVMCC}")
target_link_libraries(${PROJECT_NAME} PUBLIC ${libdgvra}
                                             ${libdgllvmvra}
                                      PRIVATE ${LLVM_LIBS} ${JSON_LIBS}
)
if(Catch2_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC Catch2::Catch2)
endif()

if(SV_BENCHMARKS_PATH)
    message(STATUS "sv-benchmarks path: " ${SV_BENCHMARKS_PATH})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tested-files.json.in
                   ${CMAKE_CURRENT_BINARY_DIR}/tested-files.json @ONLY)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ll-files)
else()
    message(WARNING "tests configuration needs SV_BENCHMARKS_PATH"
                    "set to path to the sv-benchmarks folder")
endif()
