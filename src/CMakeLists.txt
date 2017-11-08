# generate a git-version.h with a HEAD commit hash tag
# (if it changed)
add_custom_target(
	gitversion ALL sh ${CMAKE_CURRENT_SOURCE_DIR}/../git-version.sh
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(LLVMinstr
	rewriter.cpp
	instr_log.cpp
	instr_analyzer.cpp
	instr.cpp
	${JSON_FILES}
)
target_link_libraries(LLVMinstr ${llvm_libs} ${JSON_LIBS})
add_dependencies(LLVMinstr gitversion)

install(TARGETS LLVMinstr RUNTIME DESTINATION bin)

