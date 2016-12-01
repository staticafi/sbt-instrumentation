add_executable(LLVMinstr
	rewriter.cpp
	instr_log.cpp
	instr_analyzer.cpp
	instr.cpp
	${JSON_FILES}
)
target_link_libraries(LLVMinstr ${llvm_libs} ${JSON_LIBS})

install(TARGETS LLVMinstr RUNTIME DESTINATION bin)

