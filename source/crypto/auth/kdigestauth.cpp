/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/crypto/auth/kdigestauth.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/core/format/kformat.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KString kDigestAuthorization(KStringView sUsername,
                             KStringView sPassword,
                             KStringView sRealm,
                             KStringView sNonce,
                             KStringView sOpaque,
                             KStringView sQoP,
                             KStringView sMethod,
                             KStringView sURI,
                             uint32_t    iNonceCount,
                             KStringView sCNonce,
                             KStringView sBody)
//-----------------------------------------------------------------------------
{
	/*
	 Authorization: Digest username="Mufasa",
		 realm="testrealm@host.com",
		 nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
		 uri="/dir/index.html",
		 qop=auth,
		 nc=00000001,
		 cnonce="0a4f113b",
		 response="6629fae49393a05397450978507c4ef1",
		 opaque="5ccc069c403ebaf9f0171e9517f40e41"
	 */

	KMD5 HA1;
	KMD5 HA2;
	KMD5 Response;

	// RFC 2617: HA1 = MD5(username:realm:password)
	HA1 = sUsername;
	HA1 += ":";
	HA1 += sRealm;
	HA1 += ":";
	HA1 += sPassword;

	// different calculation for auth-int
	// auth or any: HA2 = MD5(method:digestURI)
	// auth-int:    HA2 = MD5(method:digestURI:MD5(entityBody))

	HA2 = sMethod;
	HA2 += ":";
	HA2 += sURI;

	if (sQoP == "auth-int")
	{
		HA2 += ":";
		HA2 += KMD5(sBody).HexDigest();
	}

	// without a qop: the response of RFC 2069, no nonce count, no cnonce
	const bool bSimpleResponse = sQoP != "auth" && sQoP != "auth-int";

	KString sNonceCount;

	Response = HA1.HexDigest();
	Response += ":";
	Response += sNonce;
	Response += ":";

	if (!bSimpleResponse)
	{
		sNonceCount = kFormat("{:08x}", iNonceCount);

		Response += sNonceCount;
		Response += ":";
		Response += sCNonce;
		Response += ":";
		Response += sQoP;
		Response += ":";
	}

	Response += HA2.HexDigest();

	KString sHeader;

	if (bSimpleResponse)
	{
		sHeader = kFormat(
				   "Digest"
				   " username=\"{}\","
				   " realm=\"{}\","
				   " nonce=\"{}\","
				   " uri=\"{}\","
				   " response=\"{}\"",
				   sUsername,
				   sRealm,
				   sNonce,
				   sURI,
				   Response.HexDigest()
				);
	}
	else
	{
		sHeader = kFormat(
				   "Digest"
				   " username=\"{}\","
				   " realm=\"{}\","
				   " nonce=\"{}\","
				   " uri=\"{}\","
				   " qop={},"
				   " nc={},"
				   " cnonce=\"{}\","
				   " response=\"{}\"",
				   sUsername,
				   sRealm,
				   sNonce,
				   sURI,
				   sQoP,
				   sNonceCount, // nc
				   sCNonce,
				   Response.HexDigest()
				);
	}

	// the opaque value goes back unchanged - and only if the server sent one
	if (!sOpaque.empty())
	{
		sHeader += kFormat(", opaque=\"{}\"", sOpaque);
	}

	return sHeader;

} // kDigestAuthorization

DEKAF2_NAMESPACE_END
