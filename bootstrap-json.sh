#!/bin/sh

# exit on any error
set -e

# make sure we're in the source directory
cd `dirname $`

# don't do redundant work
if [ ! -d jsoncpp ]; then
	# checkout jsoncpp directory
	git clone https://github.com/open-source-parsers/jsoncpp
fi

cd jsoncpp
if [ ! -d CMakeFiles ]; then
	cmake .
fi

make
python amalgamate.py

# copy the jsoncpp.cpp file into parent's src/ folder
cp dist/jsoncpp.cpp ../src/
rsync -r dist/json ../include/

echo "json files successfully copied"

