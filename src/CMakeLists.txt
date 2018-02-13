# generate a git-version.h with a HEAD commit hash tag
# (if it changed)
add_custom_target(
	gitversion ALL sh ${CMAKE_CURRENT_SOURCE_DIR}/../git-version.sh
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(sbt-instr
	rewriter.cpp
	instr_log.cpp
	instr_analyzer.cpp
	instr.cpp
	${JSON_FILES}
)
target_link_libraries(sbt-instr ${llvm_libs} ${JSON_LIBS})
add_dependencies(sbt-instr gitversion)

install(TARGETS sbt-instr RUNTIME DESTINATION bin)

