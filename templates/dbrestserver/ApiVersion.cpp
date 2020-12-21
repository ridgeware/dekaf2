
#include "__LowerProjectName__.h"
#include "db.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void __ProjectName__::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	auto pdb = DB::Get();

	auto iSchemaVersion = pdb->GetSchema ("__UpperProjectName___SCHEMA");

	KJSON json
	{
		{ "Project", "__ProjectName__" },
		{ "Version", "__ProjectVersion__" },
		{ "Schema" , iSchemaVersion }
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion
