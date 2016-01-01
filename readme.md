
Download 
https://github.com/open-source-parsers/jsoncpp
and install it into `include/json` folder, move `jsoncpp.cpp` into `src` and modify '#include "json/json.h"' to '#include "../include/json/json.h"'.  

Json config files should look like this:

[
	{
		"from": list of strings
		"to": list of strings
		"where": "before"/"after"/"replace"
	}
]

<x> is variable.

Example:
```json
[
	{
			"from": ["<t1>", "=malloc(", "<t2>", ")"],
			"to": ["<t1>", "=call_malloc(", "<t1>", "<t2>", ")"],
			"where": "before"
        }
]
```

