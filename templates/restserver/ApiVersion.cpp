
#include "__LowerProjectName__.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void __ProjectName__::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KJSON json
	{
		{ "Project", "__ProjectName__" },
		{ "Version", "__ProjectVersion__" },
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion
