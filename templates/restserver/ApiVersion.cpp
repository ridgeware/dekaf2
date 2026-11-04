
#include "__LowerProjectName__.h"
//@if db
#include "db.h"
//@endif

using namespace dekaf2;

//-----------------------------------------------------------------------------
void __ProjectName__::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
//@if db
	auto pdb = DB::Get();

	auto iSchemaVersion = pdb->GetSchema ("__UpperProjectName___SCHEMA");

//@endif
	KJSON json
	{
		{ "Project", "__ProjectName__" },
		{ "Version", "__ProjectVersion__" },
//@if db
		{ "Schema" , iSchemaVersion }
//@endif
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion
