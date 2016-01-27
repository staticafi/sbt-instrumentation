
Download 
https://github.com/open-source-parsers/jsoncpp
and install it into `include/` folder, move `jsoncpp.cpp` into `src` and modify `#include "json.h"` to `#include "../include/json.h"`. 

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
						      "operands": list of strings,
						      "arguments": list of strings(empty if instruction is not call)
						   },
				"newInstruction": {
						      "returnValue": string,
						      "instruction": call,
						      "operands": ["nameOfFunction"],
						      "arguments": list of strings
						  },
				"where": "before"/"after"/"replace"
			}
		]
    }
```

`\<x\>` is variable, `*` matches any string. The new instruction can be only a call for now (with one operand with function name).

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
						      "arguments": ["*"]
						   },
				"newInstruction": {
						      "returnValue": "*",
						      "instruction": "call",
						      "operands": ["nameOfFunction"],
						      "arguments": ["<t1>"]
						  },
				"where": "before"
			}
		]
    }
```

In `.c` file given in `.json` file all function names should start with `__INSTR_` or else they will be instrumented too. For now, if a function from this file has an argument that will not be passed from the program that is being instrumented, it has to be an integer.

