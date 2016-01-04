
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
					      "match-parameters": string
					   },
			"newInstruction": {
					      "returnValue": string,
					      "instruction": string(callinstr,alloca,...),
					      "parameters": [[type, value]]
					  },
			"where": "before"/"after"/"replace"
		}
	]
```

\<x\> is variable, %s matches any string, %n is none.

Example:
```json
	[
		{
			"findInstruction": {
					      "returnValue": "<t1>",
					      "instruction": "callinst",
					      "match-parameters": "%s@malloc(%s <t1>)%s"
					   },
			"newInstruction": {
					      "returnValue": "%n",
					      "instruction": "callinst",
					      "parameters": [["i8*","<t1>"]]
					  },
			"where": "before"
		}
	]
```

