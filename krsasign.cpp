/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2019, Ridgeware, Inc.
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

#include <openssl/evp.h>
#include <openssl/pem.h>
#include "krsasign.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KRSASign::KRSASign(Digest digest, KStringView sMessage)
//---------------------------------------------------------------------------
// The real update function is EVP_SignUpdate, but as that is defined as a macro
// (on EVP_DigestUpdate) the compiler cannot take it as a function argument. Therefore
// we insert the aliased target directly. Needs changes should the alias change in
// OpenSSL.
: KMessageDigestBase(digest, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate))
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KString KRSASign::Sign(const KRSAKey& Key) const
//---------------------------------------------------------------------------
{
	KString sSignature;

	if (evpctx && !Key.empty())
	{
		sSignature.resize(EVP_PKEY_size(Key.GetEVPPKey()));
		unsigned int iDigestLen { 0 };

		if (1 != EVP_SignFinal(evpctx, reinterpret_cast<unsigned char*>(sSignature.data()), &iDigestLen, Key.GetEVPPKey()))
		{
			kDebug(1, "cannot read signature");
		}

		sSignature.resize(iDigestLen);
	}
	else
	{
		kDebug(1, "no context");
	}

	return sSignature;

} // Signature

//---------------------------------------------------------------------------
KRSAVerify::KRSAVerify(Digest digest, KStringView sMessage)
//---------------------------------------------------------------------------
// The real update function is EVP_VerifyUpdate, but as that is defined as a macro
// (on EVP_DigestUpdate) the compiler cannot take it as a function argument. Therefore
// we insert the aliased target directly. Needs changes should the alias change in
// OpenSSL.
: KMessageDigestBase(digest, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate))
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
bool KRSAVerify::Verify(const KRSAKey& Key, KStringView _sSignature) const
//---------------------------------------------------------------------------
{
	if (evpctx && !Key.empty())
	{
		if (1 != EVP_VerifyFinal(evpctx, reinterpret_cast<const unsigned char*>(_sSignature.data()), static_cast<int>(_sSignature.size()), Key.GetEVPPKey()))
		{
			kDebug(1, "cannot verify signature");
			return false;
		}
		// this was the right signature
		return true;
	}
	else
	{
		kDebug(1, "no context");
	}

	return false;

} // Verify

DEKAF2_NAMESPACE_END
