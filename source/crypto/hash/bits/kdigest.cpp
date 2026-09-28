/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#include <dekaf2/crypto/hash/bits/kdigest.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
const evp_md_st* KDigest::GetMessageDigest(Digest digest)
//---------------------------------------------------------------------------
{
	switch (digest)
	{
		case MD5:     return EVP_md5();
		case SHA1:    return EVP_sha1();
		case SHA224:  return EVP_sha224();
		case SHA256:  return EVP_sha256();
		case SHA384:  return EVP_sha384();
		case SHA512:  return EVP_sha512();
#if DEKAF2_HAS_BLAKE2
		case BLAKE2S: return EVP_blake2s256();
		case BLAKE2B: return EVP_blake2b512();
#endif
	}

	return nullptr;

} // GetMessageDigest

//---------------------------------------------------------------------------
const KStringViewZ KDigest::ToString(Digest digest)
//---------------------------------------------------------------------------
{
	switch (digest)
	{
		case MD5:     return "md5";
		case SHA1:    return "sha1";
		case SHA224:  return "sha224";
		case SHA256:  return "sha256";
		case SHA384:  return "sha384";
		case SHA512:  return "sha512";
#if DEKAF2_HAS_BLAKE2
		case BLAKE2S: return "blake2s256";
		case BLAKE2B: return "blake2b512";
#endif
	}

	return "";

} // ToString

//---------------------------------------------------------------------------
KString KDigest::GetOpenSSLError(KStringView sMessage)
//---------------------------------------------------------------------------
{
	KString sError(sMessage);
	bool    bFirst { true };

	// drain the queue: entries left behind would be attributed to later, unrelated
	// calls in this thread - SSL_get_error() for one reads the queue
	for (auto ec = ::ERR_get_error(); ec; ec = ::ERR_get_error())
	{
		char szBuffer[256];
		::ERR_error_string_n(ec, szBuffer, sizeof(szBuffer));

		if (!sError.empty())
		{
			sError += bFirst ? ": " : "; ";
		}

		bFirst = false;
		sError += szBuffer;
	}

	return sError;

} // GetOpenSSLError

//---------------------------------------------------------------------------
bool KDigest::ConstantTimeCompare(KStringView a, KStringView b)
//---------------------------------------------------------------------------
{
	if (a.size() != b.size())
	{
		return false;
	}

	if (a.empty())
	{
		return true;
	}

#if OPENSSL_VERSION_NUMBER >= 0x010000000L
	return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
#else
	volatile unsigned char result = 0;

	for (std::size_t i = 0; i < a.size(); ++i)
	{
		result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
	}

	return result == 0;
#endif

} // ConstantTimeCompare

DEKAF2_NAMESPACE_END
