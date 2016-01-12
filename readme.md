
Download 
https://github.com/open-source-parsers/jsoncpp
and install it into `include/` folder, move `jsoncpp.cpp` into `src` and modify `#include "json.h"` to `#include "../include/json.h"`.  

Json config files should look like this:
```
	[
		{
			"findInstruction": {
					      "returnValue": string,
					      "instruction": string(callinstr,alloca,...),
					      "operands": list of strings
					   },
			"newInstruction": {
					      "returnValue": string,
					      "instruction": string(callinstr,alloca,...),
					      "operands": list of strings
					  },
			"where": "before"/"after"/"replace"
		}
	]
```

\<x\> is variable, !s matches any string, !n is none.

Example:
```json
	

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
```

