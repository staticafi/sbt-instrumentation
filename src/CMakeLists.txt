install(FILES rewriter.hpp
	DESTINATION ${INSTALL_INC_DIR})
	
install(FILES jsoncpp.cpp
	DESTINATION ${INSTALL_INC_DIR})

set(LLVM_LINK_COMPONENTS core engine)

#add_llvm_loadable_module(Instr instr.cpp)

add_library(Instr MODULE instr.cpp)





