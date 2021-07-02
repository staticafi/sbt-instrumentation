# --------------------------------------------------
# Generate a git-version.h with a HEAD commit hash
# --------------------------------------------------
find_package(Git)
if(NOT Git_FOUND OR NOT EXISTS ${CMAKE_SOURCE_DIR}/.git AND NOT GIT_VERSION)
  set(GIT_VERSION "unknown")
else()
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  OUTPUT_VARIABLE NEW_GIT_VERSION)
  if(NOT "${GIT_VERSION}" STREQUAL "${NEW_GIT_VERSION}")
      set(GIT_VERSION ${NEW_GIT_VERSION})
  endif()
endif()

set(GIT_VERSION ${GIT_VERSION} CACHE STRING "Git version hash" FORCE)
configure_file(git-version.h.in git-version.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(sbt-instr
	rewriter.cpp
	instr_log.cpp
	instr_analyzer.cpp
	instr.cpp
	${JSON_FILES}
)
target_link_libraries(sbt-instr ${llvm_libs} ${JSON_LIBS})

install(TARGETS sbt-instr RUNTIME DESTINATION bin)

