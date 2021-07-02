# --------------------------------------------------
# CheckNSWPlugin
# --------------------------------------------------
add_library(CheckNSWPlugin MODULE
    check_nsw_plugin.cpp
)

# --------------------------------------------------
# dgPointsToPlugin
# --------------------------------------------------
add_library(dgPointsToPlugin MODULE
    dg_points_to_plugin.cpp
)
target_link_libraries(dgPointsToPlugin PRIVATE ${DG_LIBS})

# --------------------------------------------------
# InfiniteLoopsPlugin
# --------------------------------------------------
add_library(InfiniteLoopsPlugin MODULE
    infinite_loops_plugin.cpp
)

# --------------------------------------------------
# LLVMPointsToPlugin
# --------------------------------------------------
add_library(LLVMPointsToPlugin MODULE
    llvm_points_to_plugin.cpp
)

# --------------------------------------------------
# PredatorPlugin
# --------------------------------------------------
add_library(PredatorPlugin MODULE
    predator_plugin.cpp
)

# --------------------------------------------------
# RangeAnalysisPlugin
# --------------------------------------------------
add_library(RangeAnalysisPlugin MODULE
    range_analysis_plugin.cpp
    ra/RangeAnalysis.cpp
)

# --------------------------------------------------
# ValueRelationsPlugin
# --------------------------------------------------
add_library(ValueRelationsPlugin MODULE
    value_relations_plugin.cpp
)
target_link_libraries(ValueRelationsPlugin PRIVATE ${DG_LIBS})

# --------------------------------------------------
# Install
# --------------------------------------------------
install(TARGETS CheckNSWPlugin dgPointsToPlugin InfiniteLoopsPlugin
                LLVMPointsToPlugin PredatorPlugin RangeAnalysisPlugin
                ValueRelationsPlugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(PROGRAMS predator_wrapper.py
        DESTINATION ${CMAKE_INSTALL_BINDIR})
