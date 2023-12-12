/*
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
#include "kstring.h"
#include "kstringview.h"
#include <ostream>

/// @file khtmlentities.h
/// provides support for html entity encoding

DEKAF2_NAMESPACE_BEGIN

class DEKAF2_PUBLIC KHTMLEntity
{

public:

	/// Returns the numerical entity for the input character
	static void ToHex(uint32_t ch, KStringRef& sOut);

	/// Returns the codepoints for the named entity in cp1, cp2, false if not found
	static bool FromNamedEntity(KStringView sEntity, uint32_t& cp1, uint32_t& cp2);

	/// Adds only mandatory entities for the input character (<>&"'),
	/// otherwise appends input char to output (and converts into UTF8)
	static void ToMandatoryEntity(uint32_t ch, KStringRef& sOut);

	/// Escapes utf8 input with mandatory HTML entities (<>'"&) and appends to sAppendTo
	static void AppendMandatory(KStringRef& sAppendTo, KStringView sIn);

	/// Escapes utf8 input with mandatory HTML entities (<>'"&)
	static KString EncodeMandatory(KStringView sIn);

	/// Escapes utf8 input with mandatory HTML entities (<>'"&)
	static void EncodeMandatory(std::ostream& Out, KStringView sIn);

	/// Converts utf8 input into HTML entities for non-alnum/space/punct characters
	static KString Encode(KStringView sIn);

	/// Converts string containing HTML entities into pure utf8
	static KString Decode(KStringView sIn, bool bAlsoNumeric = true);

	/// Converts string containing HTML entities into pure utf8, returns count of converted entities
	static std::size_t DecodeInPlace(KStringRef& sContent, bool bAlsoNumeric = true);

	/// Converts isolated HTML entity into utf8
	static KString DecodeOne(KStringView sIn);

}; // KHTMLEntity

DEKAF2_NAMESPACE_END
