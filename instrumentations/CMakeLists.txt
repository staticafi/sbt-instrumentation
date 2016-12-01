install(FILES
	double_free/double_free.c
	double_free/config.json
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/double_free/
)
install(FILES
	int_overflow/int_overflow.c
	int_overflow/config.json
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/int_overflow/
)
install(FILES
	valid_deref/valid_deref.c
	valid_deref/config.json
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/valid_deref/
)
install(FILES
	null_deref/config.json
	null_deref/null_deref.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/null_deref/
)
