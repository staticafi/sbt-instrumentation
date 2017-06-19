enable_testing()

# --------------------------------------------------
# callgraph-test
# --------------------------------------------------
add_executable(test-callgraph test-callgraph.cpp ../analyses/call_graph.cpp)
target_link_libraries(test-callgraph ${llvm_libs} ${dg_libs})
add_test(test-callgraph test-callgraph)

