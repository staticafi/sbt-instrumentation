install(FILES
	memsafety/config.json
	memsafety/config-noconst.json
	memsafety/config-basic.json
	memsafety/config-enhanced.json
	memsafety/config-enhanced-noconst.json
	memsafety/config-marker.json
	memsafety/config-i2-enhanced.json
	memsafety/config-i2-enhanced-lifetimes.json
	memsafety/config-lifetimes.json
	memsafety/config-minmax.json
	memsafety/memsafety.c
	memsafety/marker.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/memsafety/
)
install(FILES
	memsafety/sttt-experiments/config-basic.json
	memsafety/sttt-experiments/config-rc-rt.json
	memsafety/sttt-experiments/config-rc.json
	memsafety/sttt-experiments/config-i1-rt.json
	DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/memsafety/sttt-experiments
)

install(FILES
	null_deref/config.json
	null_deref/null_deref.c
	DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/null_deref/
)

install(FILES
    int_overflows/config.json
    int_overflows/config-no-ra.json
    int_overflows/overflows.c
    DESTINATION ${CMAKE_INSTALL_DATADIR}/sbt-instrumentation/int_overflows/
)
