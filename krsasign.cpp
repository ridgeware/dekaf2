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

namespace dekaf2 {


//---------------------------------------------------------------------------
KRSABase::KRSABase(KRSABase&& other)
//---------------------------------------------------------------------------
	: KMessageDigestBase(std::move(other))
	, evppkey(other.evppkey)
{
	other.evppkey = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KRSABase& KRSABase::operator=(KRSABase&& other)
//---------------------------------------------------------------------------
{
	KMessageDigestBase::operator=(std::move(other));
	evppkey = other.evppkey;
	other.evppkey = nullptr;
	return *this;

} // move assignment

//---------------------------------------------------------------------------
KRSABase::KRSABase(ALGORITHM Algorithm, UpdateFunc _Updater, KStringView sPubKey, KStringView sPrivKey)
//---------------------------------------------------------------------------
	: KMessageDigestBase(Algorithm, _Updater)
	, m_bOwnKeyPointer(true)
{
	std::unique_ptr<BIO, decltype(&BIO_free_all)> pubkey_bio(BIO_new(BIO_s_mem()), BIO_free_all);

	if (static_cast<size_t>(BIO_write(pubkey_bio.get(), sPubKey.data(), sPubKey.size())) != sPubKey.size())
	{
		kWarning("cannot load public key");
		return;
	}

	evppkey = PEM_read_bio_PUBKEY(pubkey_bio.get(), nullptr, nullptr, nullptr); // last nullptr would be pubkey password
	if (!evppkey)
	{
		kWarning("cannot load public key");
		return;
	}

	if (!sPrivKey.empty())
	{
		std::unique_ptr<BIO, decltype(&BIO_free_all)> privkey_bio(BIO_new(BIO_s_mem()), BIO_free_all);
		if (static_cast<size_t>(BIO_write(privkey_bio.get(), sPrivKey.data(), sPrivKey.size())) != sPrivKey.size())
		{
			kWarning("cannot load private key");
			return;
		}

		RSA* privkey = PEM_read_bio_RSAPrivateKey(privkey_bio.get(), nullptr, nullptr, nullptr); // last parm would be privkey password
		if (privkey == nullptr)
		{
			kWarning("cannot load private key");
			return;
		}

		if (EVP_PKEY_assign_RSA(static_cast<EVP_PKEY*>(evppkey), privkey) == 0)
		{
			RSA_free(privkey);
			kWarning("cannot load private key");
			return;
		}
	}

} // ctor

//---------------------------------------------------------------------------
KRSABase::KRSABase(ALGORITHM Algorithm, UpdateFunc _Updater, KRSAKey& PubKey)
//---------------------------------------------------------------------------
	: KMessageDigestBase(Algorithm, _Updater)
	, evppkey(PubKey.GetEVPPKey())
	, m_bOwnKeyPointer(false)
{
} // ctor

//---------------------------------------------------------------------------
void KRSABase::Release()
//---------------------------------------------------------------------------
{
	if (evppkey)
	{
		if (m_bOwnKeyPointer)
		{
			EVP_PKEY_free(static_cast<EVP_PKEY*>(evppkey));
		}
		evppkey = nullptr;
	}

	KMessageDigestBase::Release();

} // Release

//---------------------------------------------------------------------------
KRSASign::KRSASign(ALGORITHM Algorithm, KStringView sPubKey, KStringView sPrivKey, KStringView sMessage)
//---------------------------------------------------------------------------
// The real update function is EVP_SignUpdate, but as that is defined as a macro
// (on EVP_DigestUpdate) the compiler cannot take it as a function argument. Therefore
// we insert the aliased target directly. Needs changes should the alias change in
// OpenSSL.
: KRSABase(Algorithm, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate), sPubKey, sPrivKey)
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KRSASign::KRSASign(ALGORITHM Algorithm, KRSAKey& Key, KStringView sMessage)
//---------------------------------------------------------------------------
// see comment about EVP_DigestUpdate in other constructor
: KRSABase(Algorithm, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate), Key)
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KRSASign::KRSASign(KRSASign&& other)
//---------------------------------------------------------------------------
	: KRSABase(std::move(other))
	, m_sSignature(std::move(other.m_sSignature))
{
} // move ctor

//---------------------------------------------------------------------------
KRSASign& KRSASign::operator=(KRSASign&& other)
//---------------------------------------------------------------------------
{
	KRSABase::operator=(std::move(other));
	m_sSignature = std::move(other.m_sSignature);
	return *this;
	
} // move

//---------------------------------------------------------------------------
const KString& KRSASign::Signature() const
//---------------------------------------------------------------------------
{
	if (m_sSignature.empty())
	{
		if (evpctx && evppkey)
		{
			m_sSignature.resize(EVP_PKEY_size(static_cast<EVP_PKEY*>(evppkey)));
			unsigned int iDigestLen { 0 };

			if (1 != EVP_SignFinal(static_cast<EVP_MD_CTX*>(evpctx), reinterpret_cast<unsigned char*>(m_sSignature.data()), &iDigestLen, static_cast<EVP_PKEY*>(evppkey)))
			{
				kDebug(1, "cannot read signature");
			}

			m_sSignature.resize(iDigestLen);
		}
		else
		{
			kDebug(1, "no context");
		}
	}

	return m_sSignature;

} // Signature

//---------------------------------------------------------------------------
KRSAVerify::KRSAVerify(ALGORITHM Algorithm, KStringView sPubKey, KStringView sMessage)
//---------------------------------------------------------------------------
// The real update function is EVP_VerifyUpdate, but as that is defined as a macro
// (on EVP_DigestUpdate) the compiler cannot take it as a function argument. Therefore
// we insert the aliased target directly. Needs changes should the alias change in
// OpenSSL.
: KRSABase(Algorithm, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate), sPubKey)
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KRSAVerify::KRSAVerify(ALGORITHM Algorithm, KRSAKey& Key, KStringView sMessage)
//---------------------------------------------------------------------------
// see comment about EVP_DigestUpdate in other constructor
: KRSABase(Algorithm, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate), Key)
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KRSAVerify::KRSAVerify(KRSAVerify&& other)
//---------------------------------------------------------------------------
	: KRSABase(std::move(other))
	, m_sSignature(std::move(other.m_sSignature))
{
} // move ctor

//---------------------------------------------------------------------------
KRSAVerify& KRSAVerify::operator=(KRSAVerify&& other)
//---------------------------------------------------------------------------
{
	KRSABase::operator=(std::move(other));
	m_sSignature = std::move(other.m_sSignature);
	return *this;

} // move ctor

//---------------------------------------------------------------------------
bool KRSAVerify::Verify(KStringView sSignature) const
//---------------------------------------------------------------------------
{
	if (m_sSignature.empty())
	{
		if (evpctx && evppkey)
		{
			if (1 != EVP_VerifyFinal(static_cast<EVP_MD_CTX*>(evpctx), reinterpret_cast<const unsigned char*>(sSignature.data()), sSignature.size(), static_cast<EVP_PKEY*>(evppkey)))
			{
				kDebug(1, "cannot verify signature");
				m_sSignature = "fail";
				return false;
			}
			// this was the right signature - we store it for future comparisons
			m_sSignature = sSignature;
		}
		else
		{
			kDebug(1, "no context");
		}
	}

	return m_sSignature == sSignature;

} // Signature

} // end of namespace dekaf2


