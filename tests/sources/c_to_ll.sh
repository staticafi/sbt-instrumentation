#!/bin/sh

# exit on any error
set -e

# convert .c files

find . -name "*.c" | while read file; do
	clang -S -emit-llvm "$file"	
done
