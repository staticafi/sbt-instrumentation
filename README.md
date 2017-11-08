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

Usage: `./instr OPTS config.json source.c` where `OPTS` can be following:
* `--output=FILE` 	- specify output file
* `--help	`	- show help message
* `--bc`		- given file is a bytecode
* `--ll`		- generate .ll file from output .bc file.

Json config files should look like this:
```
{
	"file": path to a file with function definitions,
	"analyses": list of paths to analyses,
	"flags": list of strings,
	"globalVariablesRule": 	optional, a rule to instrument global variables,
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
	"phases":
	[   {
		"instructionRules":
		[
			{
				"findInstructions": sequence of instructions we are looking for, e.g.
						   [
							{
							    "returnValue": string,
						      	    "instruction": string(call,alloca,...),
						      	    "operands": list of strings
						      	    "getSizeTo": string (optional, for alloca, load or store),
							    "getPointerInfoTo": pair of strings (optional, for load or store)
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
				"conditions": list of condition (of strings) (optional, can be used only together with analyses)
				"in": string (name of function, can be "*" for any function),
				"setFlags": list of string pairs
			},
			...
	     ]
	}, ... ]
}
```

`<x>` is variable, `*` matches any string. The new instruction can only be a `call` for now. 

`getSizeTo` can be used to get allocated type size when instrumenting `alloca`, `load` or `store`  instruction. It cannot be used when looking for a sequence of instructions.

`getPointerInfoTo` can be used to get allocated size and location of allocation when instrumenting `load` or `store`  instruction. It can only be used if a pointer analysis plugin is available.

In the .c file given in `config.json` all function names should start with `__INSTR_` or else they will be instrumented too. For now, if a function from this file has an argument that will not be passed from the program that is being instrumented, it has to be an integer.

If the list of stages phases contains more than one phase, the rules will be applied in phases in given order.

It is possible to define flags in `flags` field and to set them when a rule is applied via `setFlags` (e.g. `"setFlags": [["exampleFlag", "true"]]` sets flag `exampleFlag` to `true`).

Instrumentation can be used together with static analyses to make the instrumentation conditional. You can plug them in by adding the paths to .so files to `analyses` list. Plugins must be derived from `InstrPlugin` class. You can specify the conditions by adding `condition` to elements of `instructionRules`. Currently, conditions `isNull`, `isConstant`, `isValidPointer`,`knownSize`, `!knownSize` or `!isValidPointer` are supported.

Example of a config file can be found [here](https://github.com/staticafi/llvm-instrumentation/blob/master/share/llvm-instrumentation/memsafety/config.json).


