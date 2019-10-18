add_library(PointsToPlugin MODULE
	points_to_plugin.cpp
	call_graph.cpp
)
target_link_libraries(PointsToPlugin PRIVATE ${dg_libs})
install(TARGETS PointsToPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(RangeAnalysisPlugin MODULE
	range_analysis_plugin.cpp
	ra/RangeAnalysis.cpp
)
target_link_libraries(RangeAnalysisPlugin PRIVATE ${ra_libs})
install(TARGETS RangeAnalysisPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(CheckNSWPlugin MODULE
	check_nsw_plugin.cpp
)
install(TARGETS CheckNSWPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(InfiniteLoopsPlugin MODULE
	infinite_loops_plugin.cpp
)
install(TARGETS InfiniteLoopsPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(ValueRelationsPlugin MODULE
	value_relations_plugin.cpp
)
install(TARGETS ValueRelationsPlugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(PredatorPlugin MODULE
    predator_plugin.cpp
)

# We need all the symbols with dynamic libs. With static libs, we get errors.
if (BUILD_SHARED_LIBS)
	target_link_libraries(PointsToPlugin PRIVATE ${llvm_libs})
	target_link_libraries(RangeAnalysisPlugin PRIVATE ${llvm_libs})
	target_link_libraries(CheckNSWPlugin PRIVATE ${llvm_libs})
	target_link_libraries(InfiniteLoopsPlugin PRIVATE ${llvm_libs})
endif()
