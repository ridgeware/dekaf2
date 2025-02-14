
#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/ksql.h>
#include <dekaf2/kerror.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// minimal command line SQL client
class KSql : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KSql ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "ksql"  };
	static constexpr KStringViewZ s_sProjectVersion { "1.0.0" };

}; // KSql
