#!/bin/sh

# exit on any error
set -e

# make sure we're in the source directory
cd `dirname $`

# don't do redundant work
if [ ! -d range_analysis_plugin ]; then
	# checkout dg directory
	git clone https://github.com/xvitovs1/range_analysis_plugin
fi

cd range_analysis_plugin
if [ ! -d CMakeFiles ]; then
	cmake .
fi

make

echo "range analysis library successfully installed"

