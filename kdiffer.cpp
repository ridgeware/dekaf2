/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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

#include "kdiffer.h"
#include "diff_match_patch.h"
#include "klog.h"

namespace dekaf2 {

using KDiffer = diff_match_patch<KString>;

//-----------------------------------------------------------------------------
KString KDiffToHTML (KStringView sText1, KStringView sText2, KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="del"*/)
//-----------------------------------------------------------------------------
{
	KString sResult;
	KDiffer differ;
	auto    diffs = differ.diff_main (sText1, sText2, /*check-lines*/false);

	differ.diff_cleanupSemantic (diffs);

	kDebug (2, "text1: {}", sText1);
	kDebug (2, "text2: {}", sText2);

	// iterate over diffs:
	for (const auto& diff : diffs)
	{
		const auto& sText = diff.text;
	
		switch (diff.operation)
		{
		case KDiffer::INSERT:
			sResult += kFormat ("<{}>{}</{}>", sInsertTag, sText, sInsertTag);
			break;
		case KDiffer::DELETE:
			sResult += kFormat ("<{}>{}</{}>", sDeleteTag, sText, sDeleteTag);
			break;
		case KDiffer::EQUAL:
		default:
			sResult += sText;
			break;
		}
	}

	kDebug (2, "diffs: {}", sResult);

	return sResult;

} // KDiffToHTML

//-----------------------------------------------------------------------------
KString KDiffToASCII (KStringView sText1, KStringView sText2)
//-----------------------------------------------------------------------------
{
	KString sResult;
	KDiffer differ;
	auto    diffs = differ.diff_main (sText1, sText2, /*check-lines*/false);

	differ.diff_cleanupSemantic (diffs);

	kDebug (2, "text1: {}", sText1);
	kDebug (2, "text2: {}", sText2);

	// iterate over diffs:
	for (const auto& diff : diffs)
	{
		const auto& sText = diff.text;
	
		switch (diff.operation)
		{
		case KDiffer::INSERT:
			sResult += kFormat ("[+{}]", sText);
			break;
		case KDiffer::DELETE:
			sResult += kFormat ("[-{}]", sText);
			break;
		case KDiffer::EQUAL:
		default:
			sResult += kFormat ("{}", sText);
			break;
		}
	}

	kDebug (2, "diffs: {}", sResult);
	return sResult;

} // KDiffToASCII

} // end of namespace dekaf2
