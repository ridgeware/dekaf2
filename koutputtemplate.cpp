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

#include "koutputtemplate.h"
#include "kstringstream.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KOutputTemplate::AddStream(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	return kAppendAll(InStream, m_sBuffer, false);

} // AddStream

//-----------------------------------------------------------------------------
bool KOutputTemplate::AddFile(KStringView sInFilename)
//-----------------------------------------------------------------------------
{
	KInFile InFile(sInFilename);
	return kAppendAll(InFile, m_sBuffer, false);

} // AddFile

//-----------------------------------------------------------------------------
bool KOutputTemplate::Write(KOutStream& OutStream,
		   KStringView sFrom,
		   KStringView sTo,
		   const KReplacer& Replacer)
//-----------------------------------------------------------------------------
{
	KStringView sRange(m_sBuffer);

	if (!sFrom.empty())
	{
		auto pos = sRange.find(sFrom);
		if (pos == KStringView::npos)
		{
			// start not found, do not output anything
			kDebug(2, "sFrom ({}) not found, no output", sFrom);
			return true;
		}
		sRange.remove_prefix(pos + sFrom.size());

		// go forward to start of next line
		pos = sRange.find('\n');
		if (pos != KStringView::npos)
		{
			sRange.remove_prefix(pos + 1);
		}
		// else if next line not found, output same line from after sFrom
	}

	if (!sTo.empty())
	{
		auto pos = sRange.find(sTo);
		if (pos == KStringView::npos)
		{
			// end not found, do not output anything
			kDebug(2, "sTo ({}) not found, no output", sTo);
			return true;
		}

		// go back to start of line
		pos = sRange.rfind('\n', pos);
		if (pos != KStringView::npos)
		{
			sRange.remove_suffix(sRange.size() - pos - 1);
		}
	}

	if (Replacer.empty() && !Replacer.GetRemoveAllVariables())
	{
		OutStream.Write(sRange);
	}
	else
	{
		OutStream.Write(Replacer.Replace(sRange));
	}
	
	return OutStream.Good();

} // Write

//-----------------------------------------------------------------------------
bool KOutputTemplate::Write(KStringView sOutFilename,
		   KStringView sFrom,
		   KStringView sTo,
		   const KReplacer& Replacer)
//-----------------------------------------------------------------------------
{
	KOutFile OutFile(sOutFilename);
	return Write(OutFile, sFrom, sTo, Replacer);

} // Write

//-----------------------------------------------------------------------------
KString KOutputTemplate::Write(KStringView sFrom,
			  KStringView sTo,
			  const KReplacer& Replacer)
//-----------------------------------------------------------------------------
{
	KString sOut;
	{
		KOutStringStream oss(sOut);
		Write(oss, sFrom, sTo, Replacer);
	}
	return sOut;

} // Write

static_assert(std::is_nothrow_move_constructible<KOutputTemplate>::value,
			  "KOutputTemplate is intended to be nothrow move constructible, but is not!");

} // of namespace dekaf2

