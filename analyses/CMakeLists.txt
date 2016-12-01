add_library(Points_to_plugin MODULE
	points_to_plugin.cpp
)
target_link_libraries(Points_to_plugin PRIVATE ${dg_libs} ${llvm_libs})
install(TARGETS Points_to_plugin
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
