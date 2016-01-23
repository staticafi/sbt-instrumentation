
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
						      "instruction": string(callinstr,alloca,...),
						      "operands": list of strings
						   },
				"newInstruction": {
						      "returnValue": string,
						      "instruction": call,
						      "operands": list of strings
						  },
				"where": "before"/"after"/"replace"
			}
		]
    }
```

\<x\> is variable, !s matches any string, !n is none. The new instruction can be only a call for now.

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
						      "operands": ["!s", "malloc"]
						   },
				"newInstruction": {
						      "returnValue": "!n",
						      "instruction": "call",
						      "operands": ["nameOfFunction","<t1>"]
						  },
				"where": "before"
			}
		]
    }
```

In `.c` file given in `.json` file all function names should start with `__INSTR_` or else they will be instrumented too.

