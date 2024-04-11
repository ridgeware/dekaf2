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

/// @file kencode.h
/// provides support for various encoding schemes

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"
#include "khex.h"
#include "kbase64.h"
#include "kurlencode.h"
#include "khtmlentities.h"
#include "kquotedprintable.h"

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Collection of string encoders
class DEKAF2_PUBLIC KEncode
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------


	/// Append a char in hexadecimal to sOut
	static void HexAppend(KStringRef& sOut, char chIn)
	{
		kHexAppend(sOut, chIn);

	} // KEnc::HexAppend

	/// Convert an input string to hexadecimal
	DEKAF2_NODISCARD
	static KString Hex(KStringView sIn)
	{
		return kHex(sIn);
	}

	/// Convert an input string to hexadecimal
	static void HexInPlace(KStringRef& sBuffer)
	{
		sBuffer = Hex(sBuffer).str();
	}

	/// Wrapper around KBase64::Encode. Does the same.
	DEKAF2_NODISCARD
	static KString Base64(KStringView sIn)
	{
		return KBase64::Encode(sIn);
	}

	/// Wrapper around KBase64::Encode. Does the same with a different interface.
	static void Base64InPlace(KStringRef& sBuffer)
	{
		sBuffer = Base64(sBuffer).str();
	}

	/// Wrapper around KBase64Url::Encode. Does the same.
	DEKAF2_NODISCARD
	static KString Base64Url(KStringView sIn)
	{
		return KBase64Url::Encode(sIn);
	}

	/// Wrapper around KBase64Url::Encode. Does the same with a different interface.
	static void Base64UrlInPlace(KStringRef& sBuffer)
	{
		sBuffer = Base64Url(sBuffer).str();
	}

	/// Wrapper around KQuotedPrintable::Encode. Does the same.
	DEKAF2_NODISCARD
	static KString QuotedPrintable(KStringView sIn, bool bForMailHeaders = false)
	{
		return KQuotedPrintable::Encode(sIn, bForMailHeaders);
	}

	/// Wrapper around KQuotedPrintable::Encode. Does the same with a different interface.
	static void QuotedPrintableInPlace(KStringRef& sBuffer, bool bForMailHeaders = false)
	{
		sBuffer = QuotedPrintable(sBuffer, bForMailHeaders).str();
	}

	/// Wrapper around kURLEncode. Does the same with a different interface.
	DEKAF2_NODISCARD
	static KString URL(KStringView sIn, URIPart URIpart = URIPart::Query)
	{
		KString sRet;
		kUrlEncode(sIn, sRet, URIpart);
		return sRet;
	}

	/// Wrapper around kURLEncode. Does the same with a different interface.
	static void URLInPlace(KStringRef& sBuffer, URIPart URIpart = URIPart::Query)
	{
		sBuffer = URL(sBuffer, URIpart).str();
	}

	/// Wrapper around kHTMLEntityEncode. Does the same.
	DEKAF2_NODISCARD
	static KString HTML(KStringView sIn)
	{
		return KHTMLEntity::Encode(sIn);
	}

	/// Wrapper around kHTMLEntityEncode. Does the same with a different interface.
	static void HTMLInPlace(KStringRef& sBuffer)
	{
		sBuffer = HTML(sBuffer).str();
	}

	/// Wrapper around kHTMLEntityEncode. Does the same.
	DEKAF2_NODISCARD
	static KString XML(KStringView sIn)
	{
		return KHTMLEntity::Encode(sIn);
	}

	/// Wrapper around kHTMLEntityEncode. Does the same with a different interface.
	static void XMLInPlace(KStringRef& sBuffer)
	{
		sBuffer = HTML(sBuffer).str();
	}

}; // KEncode

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Collection of string decoders
class DEKAF2_PUBLIC KDecode
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Decode a hexadecimal input string
	DEKAF2_NODISCARD
	static KString Hex(KStringView sIn)
	{
		return kUnHex(sIn);
	}

	/// Decode a hexadecimal input string
	static void HexInPlace(KStringRef& sBuffer)
	{
		sBuffer = Hex(sBuffer).str();
	}

	/// Wrapper around KBase64::Decode. Does the same.
	DEKAF2_NODISCARD
	static KString Base64(KStringView sIn)
	{
		return KBase64::Decode(sIn);
	}

	/// Wrapper around KBase64::Decode. Does the same with a different interface.
	static void Base64InPlace(KStringRef& sBuffer)
	{
		sBuffer = Base64(sBuffer).str();
	}

	/// Wrapper around KBase64Url::Decode. Does the same.
	DEKAF2_NODISCARD
	static KString Base64Url(KStringView sIn)
	{
		return KBase64Url::Decode(sIn);
	}

	/// Wrapper around KBase64Url::Decode. Does the same with a different interface.
	static void Base64UrlInPlace(KStringRef& sBuffer)
	{
		sBuffer = Base64Url(sBuffer).str();
	}

	/// Wrapper around KQuotedPrintable::Decode. Does the same.
	DEKAF2_NODISCARD
	static KString QuotedPrintable(KStringView sIn, bool bDotStuffing = true)
	{
		return KQuotedPrintable::Decode(sIn, bDotStuffing);
	}

	/// Wrapper around KQuotedPrintable::Decode. Does the same with a different interface.
	static void QuotedPrintableInPlace(KStringRef& sBuffer, bool bDotStuffing = true)
	{
		sBuffer = QuotedPrintable(sBuffer, bDotStuffing).str();
	}

	/// Wrapper around kUrlDecode. Does the same.
	DEKAF2_NODISCARD
	static KString URL(KStringView sIn, URIPart URIpart = URIPart::Query)
	{
		return kUrlDecode<KString>(sIn, URIpart == URIPart::Query);
	}

	/// Wrapper around kUrlDecode. Does the same.
	static void URLInPlace(KStringRef& sBuffer, URIPart URIpart = URIPart::Query)
	{
		kUrlDecode(sBuffer, URIpart == URIPart::Query);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same.
	DEKAF2_NODISCARD
	static KString HTML(KStringView sIn)
	{
		return KHTMLEntity::Decode(sIn);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same with a different interface.
	static void HTMLInPlace(KStringRef& sBuffer)
	{
		sBuffer = HTML(sBuffer).str();
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same.
	DEKAF2_NODISCARD
	static KString XML(KStringView sIn)
	{
		return KHTMLEntity::Decode(sIn);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same with a different interface.
	static void XMLInPlace(KStringRef& sBuffer)
	{
		sBuffer = HTML(sBuffer).str();
	}

}; // KDecode

// alias for the old class name
using KEnc = KEncode;
// alias for the old class name
using KDec = KDecode;

DEKAF2_NAMESPACE_END
