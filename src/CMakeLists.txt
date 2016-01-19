# install(FILES rewriter.hpp
# 	DESTINATION ${INSTALL_INC_DIR})
# 	
# install(FILES jsoncpp.cpp
# 	DESTINATION ${INSTALL_INC_DIR})
# 

#set(LLVM_LINK_COMPONENTS core engine)
#add_llvm_loadable_module(Instr instr.cpp)

set(LLVM_LINK_COMPONENTS core engine)

add_llvm_loadable_module(Instr
	instr.cpp
	instr_log.cpp
	jsoncpp.cpp
)





