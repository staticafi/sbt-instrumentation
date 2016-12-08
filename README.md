### Building

Before configuring the project, the json libraries must be bootstrapped:
```
./bootstrap-json.sh
```

This will download and generate jsoncpp libraries. Then cofigure the project
using `cmake` and build it in the usual way using `make`.

### Running

Run script `instr`. This script takes a C file, generates .bc file using clang
and then instruments the file according to given config.json. You may use --bc
switch to indicate that the input file is .bc file

Json config files should look like this:
```
    {
		"file": string,
		"analyses": list of strings,
		"globalVariablesRule": 	optional, a rule to instrument global variables
		{
			"findGlobals": {
					 	"globalVariable": string,
						"getSizeTo": string
					},
			"newInstruction": {
						"returnValue": string,
						"instruction": string(call, alloca),
						"operands": list of strings
					},
			"in": string (name of function, where new instruction should be inserted to)
		},
		"instructionRules":
		[
			{
				"findInstructions": sequence of instructions we are looking for, e.g.
						   [
							   {
							      "returnValue": string,
							      "instruction": string(call,alloca,...),
							      "operands": list of strings
							      "getSizeTo": string (optional, for alloca, load or store)
							   },
							   {
							      "returnValue": string,
							      "instruction": string(call,alloca,...),
							      "operands": list of strings
							   },
							   ...
						   ],
				"newInstruction": {
						      "returnValue": string,
						      "instruction": call,
						      "operands": list of strings
						  },
				"where": "before"/"after",
				"condition": list of strings (optional, can be used only together with analyses)
				"in": string (name of function, can be "*" for any function)
			}
		]
    }
```

`<x>` is variable, `*` matches any string. The new instruction can only be a `call` for now. 

`getSizeTo` can be used to get allocated type size when instrumenting `alloca`, `load` or `store`  instruction. It cannot be used when looking for a sequence of instructions.

In the .c file given in `config.json` all function names should start with `__INSTR_` or else they will be instrumented too. For now, if a function from this file has an argument that will not be passed from the program that is being instrumented, it has to be an integer.

Instrumentation can be used together with static analyses to make the instrumentation conditional. You can plug them in by adding the paths to .so files to `analyses` list. Plugins must be derived from `InstrPlugin` class. You can specify the conditions by adding `condition` to elements of `instructionRules`. Currently, conditions `isNull`, `isConstant`, `isValidPointer` or `!isValidPointer` are supported.

Example:
```json

    {
		"file": "example.c",
		"analyses": ["libPoints_to_plugin.so"],
		"globalVariablesRule": 	
		{
			"findGlobals": {
						"globalVariable": "<t1>",
						"getSizeTo": "<t2>"
					},
			"newInstruction": {
						"returnValue": "*",
						"instruction": "call",
						"operands": ["<t1>","<t2>", "__INSTR_remember"]
					  },
			"in": "main"
		},
		"instructionRules":
		[
			{
				"findInstructions": [
							   {
								  "returnValue": "*",
								  "instruction": "load",
								  "operands": ["<t1>"],
								  "getSizeTo": "<t2>"
							   }
						   ],
				"newInstruction": {
						      "returnValue": "*",
						      "instruction": "call",
						      "operands": ["<t1>","<t2>", "__INSTR_check_load_store"]
						  },
				"where": "before",
				"condition": ["!isValidPointer", "<t1>", "<t2>"],
				"in": "*"
			}
		]
    }
```


