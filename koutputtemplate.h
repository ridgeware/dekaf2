/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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
*/

#pragma once

/// @file koutputtemplate.h
/// adds a class that outputs and replaces text sections

#include "kstringview.h"
#include "kstring.h"
#include "kstream.h"
#include "kreplacer.h"
#include "klog.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values
class DEKAF2_PUBLIC KOutputTemplate
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KOutputTemplate() = default;
	KOutputTemplate(KInStream& InStream)
	{
		AddStream(InStream);
	}
	KOutputTemplate(KStringView sInFilename)
	{
		AddFile(sInFilename);
	}

	bool AddStream(KInStream& InStream);
	bool AddFile(KStringView sInFilename);
	bool AddString(KStringView sContent)
	{
		m_sBuffer += sContent;
		return true;
	}

	KOutputTemplate& operator+=(KStringView sContent)
	{
		AddString(sContent);
		return *this;
	}

	void clear()
	{
		m_sBuffer.clear();
	}

	bool Write(KOutStream& OutStream,
			   KStringView sFrom,
			   KStringView sTo,
			   const KReplacer& Replacer = KReplacer{});

	bool Write(KStringView sOutFilename,
			   KStringView sFrom,
			   KStringView sTo,
			   const KReplacer& Replacer = KReplacer{});

	KString Write(KStringView sFrom,
				  KStringView sTo,
				  const KReplacer& Replacer = KReplacer{});

//----------
private:
//----------

	KString m_sBuffer;

}; // KOutputtemplate

} // of namespace dekaf2

