# sbt-instrumentation

*sbt-instrumentation* is a tool for configurable instrumentation of programs in LLVM. The instrumentation can be staged (divided into phases) and can utilize various static analyses that can be plugged into the instrumentation. 

The user needs to supply the tool with instrumentation rules in JSON format and a file with definitions of instrumentation functions whose calls will be inserted into the analyzed code. As the instrumentation works with LLVM, the instrumentation functions need to be defined in a language that can be subsequently translated into LLVM.
 
 We currently use *sbt-instrumentation* in our verification tool Symbiotic (https://github.com/staticafi/symbiotic) for memory safety instrumentation. Configurations for memory safety instrumentation used in Symbiotic can be found in `instrumentations/memsafety`.
 
### Building

To compile and run *sbt-instrumentation*, it is necessary to have CMake (minimal version 3.1.0) and the LLVM minimum 3.9.1 together with Clang installed.

Before configuring the project, the json libraries and [dg library](https://github.com/mchalupa/dg) must be bootstrapped:
```
./bootstrap-json.sh
./bootstrap-dg.sh
```

This will download and generate jsoncpp libraries and a dg library. Then cofigure the project
using `cmake` and build it in the usual way using `make`:

```
cmake -DCMAKE_INSTALL_PREFIX=. -DCMAKE_INSTALL_LIBDIR=bin .
make install
```

### Running

As we use sbt-instrumentation mainly to instrument programs in C, we provide a script `sbt-instr-c` that takes 
a C file, generates .bc file using clang and then instruments the file according to the given config.json. 
You may use --bc switch to indicate that the input file is .bc file.

Usage: `./sbt-instr-c OPTS config.json source.c` where `OPTS` can be following:
* `--output=FILE` 	- specify output file
* `--help	`	- show help message
* `--bc`		- given file is a bytecode
* `--ll`		- generate .ll file from output .bc file.

If you want to instrument a code in LLVM, you can run directly the compiled tool `sbt-instr` from the `bin` directory.

Usage: `./sbt-instr config.json <IR to be instrumented> <IR with definitions> <outputFileName> <options>`

Options are following:
* `--version` - shows git version
* `--no-linking` - disables linking of definitions of instrumentation functions

### Running tests

Before running the tests, you need to execute the `c_to_ll.sh` script in `tests/sources`.

### Json config file

Json config files should look like this:
```
{
  "file": path to a file with function definitions,
  "analyses": list of paths to analyses,
  "flags": list of strings,
  "phases":
     [{
          "instructionRules":
             [{
                 "findInstructions": sequence of instructions we are looking for, e.g.
                   [
                      {
                         "returnValue": string,
                         "instruction": string(call,alloca,...),
                         "operands": list of strings
                         "getTypeSize": string (optional, for alloca, load or store),
                         "getPointerInfo": pair of strings (optional, for load or store)
                      },
                      {
                         "returnValue": string,
                         "instruction": string(call,alloca,...),
                         "operands": list of strings
                      },
                      ...
                   ],
                 "newInstruction": {
                                      "instruction": call,
                                      "operands": list of strings, last is the name of the called function
                                   },
                 "where": "before"/"after",
                 "conditions": list of conditions (optional) 
			[
                          {
                              "query": list of string
                              "expectedResult": list of strings
                          }
                        ]
                  "in": string (name of function, can be "*" for any function),
                  "setFlags": list of string pairs
              },
              ...
            ]
          "globalVariablesRules": optional, list of rules to instrument global variables,
             [{
                 "findGlobals": {
                    "globalVariable": string,
                    "getSizeTo": string
                 },
                 "newInstruction": {
                    "instruction": string(call, alloca),
                    "operands": list of strings
                 },
                 "in": string (name of function, where new instruction should be inserted to)
             }]
     },     
     ... ]
}
```

`<x>` is variable, `*` matches any string. The new instruction can only be a `call` for now. 

`getTypeSize` can be used to get allocated type size when instrumenting `alloca`, `load` or `store`  instruction. It cannot be used when looking for a sequence of instructions.

`getPointerInfo` can be used to get allocated size and location of allocation when instrumenting `load` or `store`  instruction. It can only be used if a pointer analysis plugin is available.

For now, if a function from this file has an argument that will not be passed from the program that is being instrumented, it has to be an integer.

If the list of phases contains more than one phase, the rules will be applied in phases in given order.

It is possible to define flags in `flags` field and to set them when a rule is applied via `setFlags` (e.g. `"setFlags": [["exampleFlag", "true"]]` sets flag `exampleFlag` to `true`).

Instrumentation can be used together with static analyses to make the instrumentation conditional. You can plug them in by adding the paths to .so files to `analyses` list. Plugins must be derived from `InstrPlugin` class. You can specify the conditions by adding `condition` to elements of `instructionRules`.

For more detailed description of configuration in JSON see https://is.muni.cz/th/409920/fi_m/thesis.pdf. Example of a real config file can be found [here](https://github.com/staticafi/llvm-instrumentation/blob/master/instrumentations/memsafety/config.json).

___

For more information about sbt-instrumentation please read https://is.muni.cz/th/409920/fi_m/thesis.pdf or contact us at statica@fi.muni.cz or vitovskamartina@gmail.com
