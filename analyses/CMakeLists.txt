add_library(PointsToPlugin MODULE
	points_to_plugin.cpp
	call_graph.cpp
)
target_link_libraries(PointsToPlugin PRIVATE ${dg_libs})
install(TARGETS PointsToPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(RangeAnalysisPlugin MODULE
	range_analysis_plugin.cpp
)
target_link_libraries(RangeAnalysisPlugin PRIVATE ${ra_libs})
install(TARGETS RangeAnalysisPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# We need all the symbols with dynamic libs. With static libs, we get errors.
if (BUILD_SHARED_LIBS)
	target_link_libraries(PointsToPlugin PRIVATE ${llvm_libs})
	target_link_libraries(RangeAnalysisPlugin PRIVATE ${llvm_libs})
endif()
