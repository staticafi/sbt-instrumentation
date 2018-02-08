install(FILES
	memsafety/config.json
	memsafety/config-noconst.json
	memsafety/config-simple.json
	memsafety/config-onephase.json
	memsafety/config-onephase-noconst.json
	memsafety/memsafety.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/memsafety/
)
install(FILES
	null_deref/config.json
	null_deref/null_deref.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/llvm-instrumentation/null_deref/
)

