
all:
	g++ `llvm-config --cppflags` --std=c++11 -Wall -Wextra json/jsoncpp.cpp src/instr.cpp -o instr `llvm-config --libs all`  `llvm-config --ldflags`
