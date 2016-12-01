add_library(Points_to_plugin MODULE
	points_to_plugin.cpp
)
target_link_libraries(Points_to_plugin PRIVATE ${dg_libs})
install(TARGETS Points_to_plugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# We need all the symbols with dynamic libs. With static libs, we get errors.
if (BUILD_SHARED_LIBS)
	target_link_libraries(Points_to_plugin PRIVATE ${llvm_libs})
endif()
