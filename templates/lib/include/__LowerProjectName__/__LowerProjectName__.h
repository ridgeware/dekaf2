
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the public interface of the __ProjectName__ library - a public header does not
/// pull in a using namespace, it names the dekaf2 types with their namespace
class __ProjectName__
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// returns a greeting for sName
	dekaf2::KString Greet (dekaf2::KStringView sName) const;

	/// returns the version of the library
	static dekaf2::KStringViewZ Version ();

	static constexpr dekaf2::KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr dekaf2::KStringViewZ s_sProjectVersion { "__ProjectVersion__" };

}; // __ProjectName__
