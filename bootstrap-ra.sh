#!/bin/sh

# exit on any error
set -e

# make sure we're in the source directory
cd `dirname $`

# don't do redundant work
if [ ! -d ra ]; then
	# checkout dg directory
	git clone https://github.com/xvitovs1/ra
fi

cd ra
if [ ! -d CMakeFiles ]; then
	cmake .
fi

make

echo "range analysis library successfully installed"

