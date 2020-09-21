
#include "dbrestserver.h"
#include "db.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void DBRestServer::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	auto pdb = DB::Get();

	auto iSchemaVersion = pdb->GetSchema ("__UpperProjectName___SCHEMA");

	KJSON json
	{
		{ "Project", "DBRestServer" },
		{ "Version", "0.0.1" },
		{ "Schema" , iSchemaVersion }
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion
