
all:
	g++ `llvm-config --cppflags` --std=c++11 -Wall -Wextra src/jsoncpp.cpp src/instr.cpp -o instr `llvm-config --libs all`  `llvm-config --ldflags`
