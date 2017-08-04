/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include <clocale>
#include "dekaf2.h"
#include "klog.h"
#include "kcppcompat.h"


namespace dekaf2
{

const char DefaultLocale[] = "en_US.UTF-8";

//---------------------------------------------------------------------------
Dekaf::Dekaf()
//---------------------------------------------------------------------------
{
	SetUnicodeLocale();
}

//---------------------------------------------------------------------------
bool Dekaf::SetUnicodeLocale(KString sName)
//---------------------------------------------------------------------------
{
	try
	{
#ifdef DEKAF2_IS_OSX
		// no way to get the user's locale in OSX with C++. So simply set to en_US if not given as a parameter.
		if (sName.empty() || sName == "C" || sName == "C.UTF-8")
		{
			sName = DefaultLocale;
		}
		std::setlocale(LC_ALL, sName.c_str());
		// OSX does not use the .UTF-8 suffix (as all is UTF8)
		if (sName.EndsWith(".UTF-8"))
		{
			sName.erase(sName.end() - 6, sName.end());
		}
		std::locale::global(std::locale(sName.c_str()));
		// make the name compatible to the rest of the world
		sName += ".UTF-8";
#else
		// on other platforms, query the user's locale
		if (sName.empty())
		{
			sName = std::locale("").name();
		}
		// set to a fully defined locale if only the C locale is setup. This is also needed for C.UTF-8, as
		// that one does not permit character conversions outside the ASCII range.
		if (sName.empty() || sName == "C" || sName == "C.UTF-8")
		{
			sName = DefaultLocale;
		}
		std::setlocale(LC_ALL, sName.c_str());
		std::locale::global(std::locale(sName.c_str()));
#endif
	}
	catch (std::exception& e) {
		KLog().Exception(e, DEKAF2_FUNCTION_NAME);
		sName.erase();
	}
	catch (...) {
		KLog().Exception(DEKAF2_FUNCTION_NAME);
		sName.erase();
	}
	m_sLocale = sName;
	return !m_sLocale.empty();
}


//---------------------------------------------------------------------------
class Dekaf& Dekaf()
//---------------------------------------------------------------------------
{
	static class Dekaf myDekaf;
	return myDekaf;
}

} // end of namespace dekaf2

