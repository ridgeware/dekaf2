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

#include "kdigest.h"
#include <openssl/evp.h>
#include <openssl/err.h>

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

} // GetMessageDigest

//---------------------------------------------------------------------------
KString KDigest::GetOpenSSLError(KStringView sMessage)
//---------------------------------------------------------------------------
{
	KString sError(sMessage);
	auto iPos = sMessage.size();

	auto ec = ::ERR_get_error();

	if (ec)
	{
		if (iPos)
		{
			sError += ": ";
			iPos += 2;
		}

		constexpr uint16_t iMaxError = 256;

		sError.resize(iPos + iMaxError);
		::ERR_error_string_n(ec, &sError[iPos], iMaxError);
		auto iErrorSize = ::strnlen(&sError[iPos], iMaxError);
		sError.resize(iPos + iErrorSize);
	}

	return sError;

} // GetOpenSSLError

DEKAF2_NAMESPACE_END
