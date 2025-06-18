/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

#include "krsa.h"
#include "bits/kdigest.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KRSAEncrypt::KRSAEncrypt(const KRSAKey& Key, bool bEME_OAEP)
//---------------------------------------------------------------------------
{
	m_ctx = ::EVP_PKEY_CTX_new(Key.GetEVPPKey(), nullptr);

	if (!m_ctx ||
		::EVP_PKEY_encrypt_init(m_ctx) <= 0 ||
		::EVP_PKEY_CTX_set_rsa_padding(m_ctx, bEME_OAEP ? RSA_PKCS1_OAEP_PADDING : RSA_PKCS1_PADDING) <= 0)
	{
		SetError(KDigest::GetOpenSSLError("cannot initialize context"));
	}

} // ctor

//---------------------------------------------------------------------------
KRSAEncrypt::~KRSAEncrypt()
//---------------------------------------------------------------------------
{
	if (m_ctx)
	{
		::EVP_PKEY_CTX_free(m_ctx);
	}

} // dtor

//---------------------------------------------------------------------------
KString KRSAEncrypt::Encrypt(KStringView sInput) const
//---------------------------------------------------------------------------
{
	KString sEncrypted;

	if (!HasError())
	{
		std::size_t iOutLen;
		// get outlen
		if (::EVP_PKEY_encrypt(m_ctx, nullptr, &iOutLen, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()) == 1)
		{
			sEncrypted.resize(iOutLen);

			if (::EVP_PKEY_encrypt(m_ctx, reinterpret_cast<unsigned char*>(&sEncrypted[0]), &iOutLen, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()) > 0)
			{
				if (iOutLen < sEncrypted.size())
				{
					sEncrypted.resize(iOutLen);
				}

				return sEncrypted;
			}

			sEncrypted.clear();
		}
	}

	SetError(KDigest::GetOpenSSLError("cannot encrypt"));

	return sEncrypted;

} // Encrypt

//---------------------------------------------------------------------------
KRSADecrypt::KRSADecrypt(const KRSAKey& Key, bool bEME_OAEP)
//---------------------------------------------------------------------------
{
	m_ctx = ::EVP_PKEY_CTX_new(Key.GetEVPPKey(), nullptr);

	if (!m_ctx ||
		::EVP_PKEY_decrypt_init(m_ctx) <= 0 ||
		::EVP_PKEY_CTX_set_rsa_padding(m_ctx, bEME_OAEP ? RSA_PKCS1_OAEP_PADDING : RSA_PKCS1_PADDING) <= 0)
	{
		SetError(KDigest::GetOpenSSLError("cannot initialize context"));
	}

} // ctor

//---------------------------------------------------------------------------
KRSADecrypt::~KRSADecrypt()
//---------------------------------------------------------------------------
{
	if (m_ctx)
	{
		::EVP_PKEY_CTX_free(m_ctx);
	}

} // dtor

//---------------------------------------------------------------------------
KString KRSADecrypt::Decrypt(KStringView sInput) const
//---------------------------------------------------------------------------
{
	KString sDecrypted;

	if (!HasError())
	{
		std::size_t iOutLen;
		// get outlen
		if (::EVP_PKEY_decrypt(m_ctx, nullptr, &iOutLen, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()) == 1)
		{
			sDecrypted.resize(iOutLen);

			if (::EVP_PKEY_decrypt(m_ctx, reinterpret_cast<unsigned char*>(&sDecrypted[0]), &iOutLen, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()) > 0)
			{
				sDecrypted.resize(iOutLen);
				return sDecrypted;
			}

			sDecrypted.clear();
		}
	}

	SetError(KDigest::GetOpenSSLError("cannot decrypt"));

	return sDecrypted;

} // Decrypt

DEKAF2_NAMESPACE_END
