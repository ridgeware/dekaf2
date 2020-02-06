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

#include "kstringview.h"
#include "kstring.h"
#include "kbase64.h"
#include "kurlencode.h"
#include "khtmlentities.h"
#include "kquotedprintable.h"


namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KEnc
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------


	static void HexAppend(KString& sOut, char chIn);
	
	static KString Hex(KStringView sIn);

	static void HexInPlace(KString& sBuffer)
	{
		KString sRet = Hex(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KBase64::Encode. Does the same.
	static KString Base64(KStringView sIn)
	{
		return KBase64::Encode(sIn);
	}

	/// Wrapper around KBase64::Encode. Does the same with a different interface.
	static void Base64InPlace(KString& sBuffer)
	{
		KString sRet = Base64(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KBase64Url::Encode. Does the same.
	static KString Base64Url(KStringView sIn)
	{
		return KBase64Url::Encode(sIn);
	}

	/// Wrapper around KBase64Url::Encode. Does the same with a different interface.
	static void Base64UrlInPlace(KString& sBuffer)
	{
		KString sRet = Base64Url(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KQuotedPrintable::Encode. Does the same.
	static KString QuotedPrintable(KStringView sIn, bool bForMailHeaders = false)
	{
		return KQuotedPrintable::Encode(sIn, bForMailHeaders);
	}

	/// Wrapper around KQuotedPrintable::Encode. Does the same with a different interface.
	static void QuotedPrintableInPlace(KString& sBuffer, bool bForMailHeaders = false)
	{
		KString sRet = QuotedPrintable(sBuffer, bForMailHeaders);
		sBuffer.swap(sRet);
	}

	/// Wrapper around kURLEncode. Does the same with a different interface.
	static KString URL(KStringView sIn, URIPart URIpart = URIPart::Query)
	{
		KString sRet;
		kUrlEncode(sIn, sRet, URIpart);
		return sRet;
	}

	/// Wrapper around kURLEncode. Does the same with a different interface.
	static void URLInPlace(KString& sBuffer, URIPart URIpart = URIPart::Query)
	{
		KString sRet = URL(sBuffer, URIpart);
		sBuffer.swap(sRet);
	}

	/// Wrapper around kHTMLEntityEncode. Does the same.
	static KString HTML(KStringView sIn)
	{
		return KHTMLEntity::Encode(sIn);
	}

	/// Wrapper around kHTMLEntityEncode. Does the same with a different interface.
	static void HTMLInPlace(KString& sBuffer)
	{
		KString sRet = HTML(sBuffer);
		sBuffer.swap(sRet);
	}

}; // KEnc

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KDec
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static KString Hex(KStringView sIn);

	static void HexInPlace(KString& sBuffer)
	{
		KString sRet = Hex(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KBase64::Decode. Does the same.
	static KString Base64(KStringView sIn)
	{
		return KBase64::Decode(sIn);
	}

	/// Wrapper around KBase64::Decode. Does the same with a different interface.
	static void Base64InPlace(KString& sBuffer)
	{
		KString sRet = Base64(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KBase64Url::Decode. Does the same.
	static KString Base64Url(KStringView sIn)
	{
		return KBase64Url::Decode(sIn);
	}

	/// Wrapper around KBase64Url::Decode. Does the same with a different interface.
	static void Base64UrlInPlace(KString& sBuffer)
	{
		KString sRet = Base64Url(sBuffer);
		sBuffer.swap(sRet);
	}

	/// Wrapper around KQuotedPrintable::Decode. Does the same.
	static KString QuotedPrintable(KStringView sIn, bool bDotStuffing = true)
	{
		return KQuotedPrintable::Decode(sIn, bDotStuffing);
	}

	/// Wrapper around KQuotedPrintable::Decode. Does the same with a different interface.
	static void QuotedPrintableInPlace(KString& sBuffer, bool bDotStuffing = true)
	{
		KString sRet = QuotedPrintable(sBuffer, bDotStuffing);
		sBuffer.swap(sRet);
	}

	/// Wrapper around kUrlDecode. Does the same.
	static KString URL(KStringView sIn, URIPart URIpart = URIPart::Query)
	{
		return kUrlDecode<KString>(sIn, URIpart == URIPart::Query);
	}

	/// Wrapper around kUrlDecode. Does the same.
	static void URLInPlace(KString& sBuffer, URIPart URIpart = URIPart::Query)
	{
		kUrlDecode(sBuffer, URIpart == URIPart::Query);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same.
	static KString HTML(KStringView sIn)
	{
		return KHTMLEntity::Decode(sIn);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same.
	static KString XML(KStringView sIn)
	{
		return KHTMLEntity::Decode(sIn);
	}

	/// Wrapper around KHTMLEntity::Decode. Does the same with a different interface.
	static void HTMLInPlace(KString& sBuffer)
	{
		KString sRet = HTML(sBuffer);
		sBuffer.swap(sRet);
	}

}; // KDec

} // of namespace dekaf2

