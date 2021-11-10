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
# Tests
# --------------------------------------------------

add_executable(${PROJECT_NAME} tests-main.cpp
                               tests.cpp
                               ../src/jsoncpp.cpp
                               ${CMAKE_SOURCE_DIR}/analyses/value_relations_plugin.cpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${libdgvra}
                                             ${libdgllvmvra}
                                      PRIVATE ${LLVM_LIBS} ${JSON_LIBS}
)
if(Catch2_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC Catch2::Catch2)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tested-files.json ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
