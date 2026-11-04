
#include <__LowerProjectName__/__LowerProjectName__.h>
#include <dekaf2/core/format/kformat.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
KString __ProjectName__::Greet (KStringView sName) const
//-----------------------------------------------------------------------------
{
	return kFormat("hello {}, this is {}", sName, s_sProjectName);

} // Greet

//-----------------------------------------------------------------------------
KStringViewZ __ProjectName__::Version ()
//-----------------------------------------------------------------------------
{
	return s_sProjectVersion;

} // Version

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
