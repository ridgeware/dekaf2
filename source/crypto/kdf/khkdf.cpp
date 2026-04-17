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
 //
 */

#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/crypto/hash/khmac.h>
#include <dekaf2/core/logging/klog.h>
#include <openssl/evp.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KString KHKDF::Extract(KStringView sSalt, KStringView sIKM, enum Digest digest)
//---------------------------------------------------------------------------
{
	// RFC 5869 Section 2.2: if salt is not provided, use a string of HashLen zeros
	if (sSalt.empty())
	{
		auto iHashLen = static_cast<std::size_t>(::EVP_MD_get_size(GetMessageDigest(digest)));
		KString sZeroSalt(iHashLen, '\0');
		KHMAC hmac(digest, sZeroSalt, sIKM);
		return hmac.Digest();
	}

	KHMAC hmac(digest, sSalt, sIKM);

	return hmac.Digest();

} // Extract

//---------------------------------------------------------------------------
KString KHKDF::Expand(KStringView sPRK, KStringView sInfo, std::size_t iLength, enum Digest digest)
//---------------------------------------------------------------------------
{
	// RFC 5869 Section 2.3
	KString sResult;
	KString sPrev;
	uint8_t iCounter = 1;

	while (sResult.size() < iLength)
	{
		KString sInput;
		sInput += sPrev;
		sInput += sInfo;
		sInput += static_cast<char>(iCounter);

		KHMAC hmac(digest, sPRK, sInput);
		sPrev = hmac.Digest();

		sResult += sPrev;
		++iCounter;
	}

	sResult.resize(iLength);

	return sResult;

} // Expand

//---------------------------------------------------------------------------
KString KHKDF::DeriveKey(KStringView sSalt, KStringView sIKM, KStringView sInfo, std::size_t iLength, enum Digest digest)
//---------------------------------------------------------------------------
{
	return Expand(Extract(sSalt, sIKM, digest), sInfo, iLength, digest);

} // DeriveKey

DEKAF2_NAMESPACE_END
