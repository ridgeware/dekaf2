
#include "kurlencode.h"


namespace dekaf2 {

namespace detail {

/*
	Schema         = 0,
	User           = 1,
	Password       = 2,
	Domain         = 3,
	Port           = 4,
	Path           = 5,
	Query          = 6,
	Fragment       = 7
*/

KUrlEncodingTables KUrlEncodingTables::MyInstance {};

const char* KUrlEncodingTables::s_sExcludes[] =
{
    "-._~",     // used by Schema .. Port
    "-._~;,=/", // used by Path
    ""          // used by Query and Fragment ("/?" would be better for conformity (see RFC3986 3.4)
};

bool* KUrlEncodingTables::EncodingTable[TABLECOUNT];
bool KUrlEncodingTables::Tables[INT_TABLECOUNT][256];

//-----------------------------------------------------------------------------
KUrlEncodingTables::KUrlEncodingTables()
//-----------------------------------------------------------------------------
{
	// set up the encoding tables
	for (auto table = 0; table < INT_TABLECOUNT; ++table)
	{
		std::memset(&Tables[table], false, 256);
		const unsigned char* p = reinterpret_cast<const unsigned char*>(s_sExcludes[table]);
		while (auto ch = *p++)
		{
			Tables[table][ch] = true;
		}
	}
	// now set up the pointers into these tables, as some are shared..
	for (auto table = static_cast<int>(URIPart::Protocol); table < static_cast<int>(URIPart::Path); ++table)
	{
		EncodingTable[table] = Tables[0];
	}
	for (auto table = static_cast<int>(URIPart::Path); table < static_cast<int>(URIPart::Fragment); ++table)
	{
		EncodingTable[table] = Tables[(table - static_cast<int>(URIPart::Path)) + 1];
	}
	EncodingTable[static_cast<int>(URIPart::Fragment)] = EncodingTable[static_cast<int>(URIPart::Query)];
}

} // end of namespace detail


}// end of namespace dekaf2
