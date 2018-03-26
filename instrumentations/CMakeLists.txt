install(FILES
	memsafety/config.json
	memsafety/config-noconst.json
	memsafety/config-basic.json
	memsafety/config-enhanced.json
	memsafety/config-enhanced-noconst.json
	memsafety/config-marker.json
	memsafety/memsafety.c
	memsafety/marker.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/memsafety/
)
install(FILES
	null_deref/config.json
	null_deref/null_deref.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/null_deref/
)

