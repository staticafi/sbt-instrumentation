
Download
https://github.com/open-source-parsers/jsoncpp and amalgamate the `jsonccp.cpp` source:
```
cd jsoncpp
python amalgamate.py
```
Move newly created `jsoncpp.cpp` into `LLVMInstrument/src` folder.
Then cofigure the project using `cmake` and build it in the usual way
using `make`.

Run script `instr`.

Json config files should look like this:
```
    {
		"file": string,
		"rules":	
		[
			{
				"findInstruction": {
						      "returnValue": string,
						      "instruction": string(call,alloca,...),
						      "operands": list of strings
						   },
				"newInstruction": {
						      "returnValue": string,
						      "instruction": call,
						      "operands": list of strings
						  },
				"where": "before"/"after"/"replace",
				"in": string (name of function, can be "*" for any function)
			}
		]
    }
```

`<x>` is variable, `*` matches any string. The new instruction can be only a call for now.

Example:
```json
	
    {
		"file": "example.c",
		"rules":
		[
			{
				"findInstruction": {
						      "returnValue": "<t1>",
						      "instruction": "call",
						      "operands": ["*", "malloc"],
						   },
				"newInstruction": {
						      "returnValue": "*",
						      "instruction": "call",
						      "operands": ["<t1>","nameOfFunction"],
						  },
				"where": "before",
				"in": "main"
			}
		]
    }
```

In `.c` file given in `.json` file all function names should start with `__INSTR_` or else they will be instrumented too. For now, if a function from this file has an argument that will not be passed from the program that is being instrumented, it has to be an integer.

