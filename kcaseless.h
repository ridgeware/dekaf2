/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

#include "kdefinitions.h"
#include "kstringview.h"
#include "bits/khash.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// compares case insensitive
DEKAF2_PUBLIC
int kCaselessCompare(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality
DEKAF2_PUBLIC
bool kCaselessEqual(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at begin of left string
DEKAF2_PUBLIC
bool kCaselessBeginsWith(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at end of left string
DEKAF2_PUBLIC
bool kCaselessEndsWith(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// finds case insensitively
DEKAF2_PUBLIC
KStringView::size_type kCaselessFind(KStringView sHaystack, KStringView sNeedle, KStringView::size_type iPos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// finds case insensitively, assuming right string in lowercase
DEKAF2_PUBLIC
KStringView::size_type kCaselessFindLeft(KStringView sHaystack, KStringView sNeedle, KStringView::size_type iPos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare from whitespace
DEKAF2_PUBLIC
int kCaselessCompareTrim(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality from whitespace
DEKAF2_PUBLIC
bool kCaselessEqualTrim(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare
DEKAF2_PUBLIC
int kCaselessCompareTrim(KStringView left, KStringView right, KStringView svTrim);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
DEKAF2_PUBLIC
bool kCaselessEqualTrim(KStringView left, KStringView right, KStringView svTrim);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, assuming right string in lowercase
DEKAF2_PUBLIC
int kCaselessCompareLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality, assuming right string in lowercase
DEKAF2_PUBLIC
bool kCaselessEqualLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at begin of left string, assuming right string in lowercase
/// (despite the name this tests for left beginning with right)
DEKAF2_PUBLIC
bool kCaselessBeginsWithLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at end of left string, assuming right string in lowercase
/// (despite the name this tests for left ending with right)
DEKAF2_PUBLIC
bool kCaselessEndsWithLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare from whitespace,
/// assuming right string trimmed and in lowercase
DEKAF2_PUBLIC
int kCaselessCompareTrimLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests whitespace trimmed strings for case insensitive equality
/// assuming right string trimmed and in lowercase
DEKAF2_PUBLIC
bool kCaselessEqualTrimLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare,
/// assuming right string trimmed and in lowercase
DEKAF2_PUBLIC
int kCaselessCompareTrimLeft(KStringView left, KStringView right, KStringView svTrim);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
/// assuming right string trimmed and in lowercase
DEKAF2_PUBLIC
bool kCaselessEqualTrimLeft(KStringView left, KStringView right, KStringView svTrim);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive string
DEKAF2_PUBLIC
DEKAF2_CONSTEXPR_14
std::size_t kCalcCaselessHash(KStringView sv)
//-----------------------------------------------------------------------------
{
	return kCaseHash(sv.data(), sv.size());
}

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive whitespace trimmed string
DEKAF2_PUBLIC
std::size_t kCalcCaselessHashTrim(KStringView);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive trimmed string
DEKAF2_PUBLIC
std::size_t kCalcCaselessHashTrim(KStringView, KStringView svTrim);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END
