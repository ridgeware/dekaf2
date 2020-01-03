
#include "{{LowerProjectName}}.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void {{ProjectName}}::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KJSON json
	{
		{ "Project", "{{ProjectName}}" },
		{ "Version", "{{ProjectVersion}}" },
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion
