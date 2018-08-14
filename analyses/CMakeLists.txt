add_library(Points_to_plugin MODULE
	points_to_plugin.cpp
	call_graph.cpp
)
target_link_libraries(Points_to_plugin PRIVATE ${dg_libs})
install(TARGETS Points_to_plugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(Range_analysis_plugin MODULE
	range_analysis_plugin.cpp
)
target_link_libraries(Range_analysis_plugin PRIVATE ${ra_libs})
install(TARGETS Range_analysis_plugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# We need all the symbols with dynamic libs. With static libs, we get errors.
if (BUILD_SHARED_LIBS)
	target_link_libraries(Points_to_plugin PRIVATE ${llvm_libs})
	target_link_libraries(Range_analysis_plugin PRIVATE ${llvm_libs})
endif()
