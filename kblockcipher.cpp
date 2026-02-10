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

#include "kaes.h"

#if DEKAF2_HAS_AES

#include "kencode.h"
#include "klog.h"
#include "kwrite.h"  // for kGetWritePosition()
#include "kscopeguard.h"
#include "kcrashexit.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
#include <openssl/core_names.h>
#endif
#include <openssl/rand.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KBlockCipher::KBlockCipher(
	Direction    direction,
	Algorithm    algorithm,
	Mode         mode,
	Bits         bits,
	bool         bInlineIV,
	bool         bInlineTag
)
//---------------------------------------------------------------------------
: m_Direction(direction)
, m_Algorithm(algorithm)
, m_Mode(mode)
, m_bInlineIV(bInlineIV)
, m_bInlineTag(bInlineTag)
{
	Initialize(algorithm, bits);

} // ctor

//---------------------------------------------------------------------------
KBlockCipher::KBlockCipher(KBlockCipher&& other) noexcept
//---------------------------------------------------------------------------
: m_Cipher(other.m_Cipher)
, m_evpctx(other.m_evpctx)
, m_iKeyLength(other.m_iKeyLength)
, m_iIVLength(other.m_iIVLength)
, m_iTagLength(other.m_iTagLength)
, m_iBlockSize(other.m_iBlockSize)
, m_sCipherName(other.m_sCipherName)
, m_sIV(std::move(other.m_sIV))
, m_sTag(std::move(other.m_sTag))
, m_OutStream(other.m_OutStream)
, m_OutString(other.m_OutString)
, m_iStartOfString(other.m_iStartOfString)
, m_iStartOfStream(other.m_iStartOfStream)
, m_Direction(other.m_Direction)
, m_bInlineIV(other.m_bInlineIV)
, m_bInlineTag(other.m_bInlineTag)
, m_bKeyIsSet(other.m_bKeyIsSet)
, m_bInitCompleted(other.m_bInitCompleted)
{
	other.m_Cipher    = nullptr;
	other.m_evpctx    = nullptr;
	other.m_sCipherName.clear();
	other.m_OutStream = nullptr;
	other.m_OutString = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KBlockCipher::~KBlockCipher()
//---------------------------------------------------------------------------
{
	Finalize();
	Release();

} // dtor

//---------------------------------------------------------------------------
const evp_cipher_st* KBlockCipher::GetCipher(Algorithm algorithm, Mode mode, Bits bits)
//---------------------------------------------------------------------------
{
	switch (algorithm)
	{
		case AES:
			switch (mode)
			{
				case ECB:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ecb();
						case B192: return ::EVP_aes_192_ecb();
						case B256: return ::EVP_aes_256_ecb();
					}
					break;

				case CBC:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_cbc();
						case B192: return ::EVP_aes_192_cbc();
						case B256: return ::EVP_aes_256_cbc();
					}
					break;

				case OFB:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ofb();
						case B192: return ::EVP_aes_192_ofb();
						case B256: return ::EVP_aes_256_ofb();
					}
					break;

				case CFB1:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_cfb1();
						case B192: return ::EVP_aes_192_cfb1();
						case B256: return ::EVP_aes_256_cfb1();
					}
					break;

				case CFB8:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_cfb8();
						case B192: return ::EVP_aes_192_cfb8();
						case B256: return ::EVP_aes_256_cfb8();
					}
					break;

				case CFB128:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_cfb128();
						case B192: return ::EVP_aes_192_cfb128();
						case B256: return ::EVP_aes_256_cfb128();
					}
					break;

				case CTR:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ctr();
						case B192: return ::EVP_aes_192_ctr();
						case B256: return ::EVP_aes_256_ctr();
					}
					break;

				case CCM:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ccm();
						case B192: return ::EVP_aes_192_ccm();
						case B256: return ::EVP_aes_256_ccm();
					}
					break;

				case GCM:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_gcm();
						case B192: return ::EVP_aes_192_gcm();
						case B256: return ::EVP_aes_256_gcm();
					}
					break;

				case OCB:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ocb();
						case B192: return ::EVP_aes_192_ocb();
						case B256: return ::EVP_aes_256_ocb();
					}
					break;

				case XTS:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_xts();
						case B192: break;
						case B256: return ::EVP_aes_256_xts();
					}
					break;

			} // AES
			break;

		case ARIA:
#if DEKAF2_HAS_ARIA
			switch (mode)
			{
				case ECB:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_ecb();
						case B192: return ::EVP_aria_192_ecb();
						case B256: return ::EVP_aria_256_ecb();
					}
					break;

				case CBC:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_cbc();
						case B192: return ::EVP_aria_192_cbc();
						case B256: return ::EVP_aria_256_cbc();
					}
					break;

				case OFB:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_ofb();
						case B192: return ::EVP_aria_192_ofb();
						case B256: return ::EVP_aria_256_ofb();
					}
					break;

				case CFB1:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_cfb1();
						case B192: return ::EVP_aria_192_cfb1();
						case B256: return ::EVP_aria_256_cfb1();
					}
					break;

				case CFB8:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_cfb8();
						case B192: return ::EVP_aria_192_cfb8();
						case B256: return ::EVP_aria_256_cfb8();
					}
					break;

				case CFB128:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_cfb128();
						case B192: return ::EVP_aria_192_cfb128();
						case B256: return ::EVP_aria_256_cfb128();
					}
					break;

				case CTR:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_ctr();
						case B192: return ::EVP_aria_192_ctr();
						case B256: return ::EVP_aria_256_ctr();
					}
					break;

				case CCM:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_ccm();
						case B192: return ::EVP_aria_192_ccm();
						case B256: return ::EVP_aria_256_ccm();
					}
					break;

				case GCM:
					switch (bits)
					{
						case B128: return ::EVP_aria_128_gcm();
						case B192: return ::EVP_aria_192_gcm();
						case B256: return ::EVP_aria_256_gcm();
					}
					break;

				case OCB:
				case XTS:
					// these don't exist for ARIA
					break;

			} // ARIA
#endif
			break;

		case Camellia:
			switch (mode)
			{
				case ECB:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_ecb();
						case B192: return ::EVP_camellia_192_ecb();
						case B256: return ::EVP_camellia_256_ecb();
					}
					break;

				case CBC:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_cbc();
						case B192: return ::EVP_camellia_192_cbc();
						case B256: return ::EVP_camellia_256_cbc();
					}
					break;

				case OFB:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_ofb();
						case B192: return ::EVP_camellia_192_ofb();
						case B256: return ::EVP_camellia_256_ofb();
					}
					break;

				case CFB1:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_cfb1();
						case B192: return ::EVP_camellia_192_cfb1();
						case B256: return ::EVP_camellia_256_cfb1();
					}
					break;

				case CFB8:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_cfb8();
						case B192: return ::EVP_camellia_192_cfb8();
						case B256: return ::EVP_camellia_256_cfb8();
					}
					break;

				case CFB128:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_cfb128();
						case B192: return ::EVP_camellia_192_cfb128();
						case B256: return ::EVP_camellia_256_cfb128();
					}
					break;

				case CTR:
					switch (bits)
					{
						case B128: return ::EVP_camellia_128_ctr();
						case B192: return ::EVP_camellia_192_ctr();
						case B256: return ::EVP_camellia_256_ctr();
					}
					break;

				case CCM:
				case GCM:
				case OCB:
				case XTS:
					// these don't exist for Camellia
					break;

			} // Camellia
			break;

		case ChaCha20_Poly1305:
			return EVP_chacha20_poly1305();
	}

	return nullptr;

} // GetCipher

//---------------------------------------------------------------------------
KStringView KBlockCipher::ToString(Algorithm algorithm)
//---------------------------------------------------------------------------
{
	switch (algorithm)
	{
		case AES:               return "AES";
		case ARIA:              return "ARIA";
		case Camellia:          return "Camellia";
		case ChaCha20_Poly1305: return "ChaCha20_Poly1305";
	}
	return "";
}

//---------------------------------------------------------------------------
KStringView KBlockCipher::ToString(Mode mode)
//---------------------------------------------------------------------------
{
	switch (mode)
	{
		case ECB:    return "ECB";
		case CBC:    return "CBC";
		case OFB:    return "OFB";
		case CFB1:   return "CFB1";
		case CFB8:   return "CFB8";
		case CFB128: return "CFB128";
		case CTR:    return "CTR";
		case CCM:    return "CCM";
		case GCM:    return "GCM";
		case OCB:    return "OCB";
		case XTS:    return "XTS";
	}
	return "";
}

//---------------------------------------------------------------------------
KStringView KBlockCipher::ToString(Bits bits)
//---------------------------------------------------------------------------
{
	switch (bits)
	{
		case B128: return "128";
		case B192: return "192";
		case B256: return "256";
	}
	return "";
}

#if	DEKAF2_HAS_HKDF
//---------------------------------------------------------------------------
KString KBlockCipher::CreateKeyFromPasswordHKDF(uint16_t iKeyLen, KStringView sPassword, KStringView sSalt)
//---------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER >= 0x030000000L

	::EVP_KDF* kdf = ::EVP_KDF_fetch(nullptr, "HKDF", nullptr);
	::EVP_KDF_CTX* kctx = ::EVP_KDF_CTX_new(kdf);
	::EVP_KDF_free(kdf);

	::OSSL_PARAM params[5], *p = params;
	*p++ = ::OSSL_PARAM_construct_utf8_string (OSSL_KDF_PARAM_DIGEST, const_cast<char*>(SN_sha256), 0);
	*p++ = ::OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY   , const_cast<char*>(sPassword.data()), sPassword.size());
	if (!sSalt.empty())
	{
		*p++ = ::OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<char*>(sSalt.data()), sSalt.size());
	}
	*p   = ::OSSL_PARAM_construct_end();

	KString sKey(iKeyLen, '\0');

	if (::EVP_KDF_derive(kctx, reinterpret_cast<unsigned char*>(&sKey[0]), sKey.size(), params) <= 0)
	{
		sKey.clear();
		kWarning(GetOpenSSLError("cannot generate key"));
	}

	::EVP_KDF_CTX_free(kctx);

	return sKey;

#else

	KString sKey(iKeyLen, '\0');
	std::size_t iOutlen = sKey.size();

	EVP_PKEY_CTX* pctx = ::EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);

	if (::EVP_PKEY_derive_init(pctx) <= 0                    ||
		::EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0  ||
		::EVP_PKEY_CTX_set1_hkdf_key(pctx, reinterpret_cast<unsigned char*>(const_cast<char*>(sPassword.data())), static_cast<int>(sPassword.size())) <= 0 ||
		(!sSalt.empty() && ::EVP_PKEY_CTX_set1_hkdf_salt(pctx, reinterpret_cast<unsigned char*>(const_cast<char*>(sSalt.data())), static_cast<int>(sSalt.size())) <= 0)  ||
		::EVP_PKEY_derive(pctx, reinterpret_cast<unsigned char*>(&sKey[0]), &iOutlen) <= 0)
	{
		sKey.clear();
		kWarning(GetOpenSSLError("cannot generate key"));
	}

	::EVP_PKEY_CTX_free(pctx);

	return sKey;

#endif

} // CreateKeyFromPasswordHKDF
#endif

//---------------------------------------------------------------------------
KString KBlockCipher::CreateKeyFromPasswordPKCS5
(
 uint16_t    iKeyLen,
 KStringView sPassword,
 KStringView sSalt,
 Digest      digest,
 uint16_t    iIterations
)
//---------------------------------------------------------------------------
{
	KString sKey;
	sKey.resize(iKeyLen);

	if (1 != ::PKCS5_PBKDF2_HMAC
	(
		sPassword.data(), static_cast<int>(sPassword.size()),
		reinterpret_cast<const unsigned char*>(sSalt.data()), static_cast<int>(sSalt.size()),
		iIterations,
		GetMessageDigest(digest),
		iKeyLen,
		reinterpret_cast<unsigned char*>(&sKey[0])
	))
	{
		sKey.clear();
		kWarning(GetOpenSSLError("cannot generate key"));
	}

	return sKey;

} // CreateKeyFromPasswordPKCS5

//---------------------------------------------------------------------------
bool KBlockCipher::SetKey(KStringView sKey)
//---------------------------------------------------------------------------
{
	if (sKey.size() < GetNeededKeyLength())
	{
		return SetError(kFormat("key length is {} bytes, but at least {} bytes are required", sKey.size(), m_iKeyLength));
	}

	// continue the initialization by adding the key
	// we use EVP_CipherInit() and not EVP_CipherInit_ex2() because the latter
	// is only supported from v3.0.0 onward
	if (!::EVP_CipherInit(
		m_evpctx,
		nullptr,
		reinterpret_cast<const unsigned char*>(sKey.data()),
		nullptr,
		m_Direction)
	)
	{
		SetError(GetOpenSSLError(kFormat("cannot initialize cipher {}", m_sCipherName)));
		return false;
	}

	m_bKeyIsSet = true;

	return true;

} // SetKey

//---------------------------------------------------------------------------
bool KBlockCipher::SetPassword(KStringView sPassword, KStringView sSalt)
//---------------------------------------------------------------------------
{
	return SetKey(CreateKeyFromPassword(GetNeededKeyLength(), sPassword, sSalt));

} // SetPassword

//---------------------------------------------------------------------------
bool KBlockCipher::SetInitializationVector(KStringView sIV)
//---------------------------------------------------------------------------
{
	if (!GetNeededIVLength())
	{
		return SetError("initialization vector not supported for selected cipher");
	}

	if (GetNeededIVLength() != sIV.size())
	{
		return SetError(kFormat("initialization vector for {} must have {} bits, but has only {}",
		                        m_sCipherName, GetNeededIVLength() * 8, sIV.size() * 8));
	}

	m_sIV = sIV;
	m_sLastIV = m_sIV;

	return true;

} // SetInitializationVector

//---------------------------------------------------------------------------
bool KBlockCipher::SetAutoIncrementNonceAsIV(uint64_t iCount)
//---------------------------------------------------------------------------
{
	if (!GetNeededIVLength())
	{
		return SetError("initialization vector not supported for selected cipher");
	}

	if (GetMode() != Mode::GCM &&
	    GetMode() != Mode::CCM &&
	    GetAlgorithm() != Algorithm::ChaCha20_Poly1305)
	{
		return SetError(kFormat("selected cipher {} in mode {} does not support nonce IVs",
		                        GetCipherName(), ToString(GetMode())));
	}

	if (iCount == 0) ++iCount;
	m_iNonceIV = iCount;

	return true;

} // SetAutoIncrementNonceAsIV

//---------------------------------------------------------------------------
uint64_t KBlockCipher::GetNextIncrementalIV()
//---------------------------------------------------------------------------
{
	// we use this computation to make sure we start with 1 instead of 2
	// when per default setting the nonce counter to 1 at start
	if (m_iNonceIV > 0)
	{
		// make sure we never return the same nonce twice
		return m_iNonceIV++;
	}
	else
	{
		// 0 means: we do not use incremental nonces
		return 0;
	}

} // GetNextIncrementalIV

//---------------------------------------------------------------------------
KString KBlockCipher::PrintIncrementalIV(uint64_t iIV)
//---------------------------------------------------------------------------
{
	KString sIV(GetNeededIVLength(), '\0');

	auto it = sIV.end();

	for (auto ct = sizeof(iIV); ct-- > 0 && it != sIV.begin();)
	{
		*--it = iIV & 0xff;
		iIV >>= 8;
	}

	if (iIV != 0)
	{
		kDebug(1, "incremental IV too large for IV length of {} - shortened", sIV.size());
	}

	return sIV;

} // PrintIncrementalIV

//---------------------------------------------------------------------------
bool KBlockCipher::SetAuthenticationTag(KStringView sTag)
//---------------------------------------------------------------------------
{
	if (!m_iTagLength) return SetError("authentication tag not supported for selected cipher");

	m_sTag = sTag;
	m_sLastTag = sTag;

	return true;

} // SetAuthenticationTag

//---------------------------------------------------------------------------
bool KBlockCipher::SetOutput(KStringRef& sOutput)
//---------------------------------------------------------------------------
{
	if (m_OutStream || m_OutString) return SetError("output was already set");

	m_iStartOfString = sOutput.size();

	if (m_iStartOfString)
	{
		kDebug(2, "output string has initial size {} - will append new data, this might not be what you want", m_iStartOfString);
	}

	m_OutStream = nullptr;
	m_OutString = &sOutput;

	return true;

} // SetOutput

//---------------------------------------------------------------------------
bool KBlockCipher::SetOutput(KOutStream& OutStream)
//---------------------------------------------------------------------------
{
	if (m_OutStream || m_OutString) return SetError("output was already set");

	m_OutStream = &OutStream;
	m_OutString = nullptr;

	if (m_Direction == Encrypt &&
		((m_bInlineIV  && GetNeededIVLength()) ||
		 (m_bInlineTag && m_iTagLength)))
	{
		// check if the stream is repositionable, and note the current position
		// to insert the tag later
		m_iStartOfStream = kGetWritePosition(*m_OutStream);
		if (m_iStartOfStream < 0) return SetError("cannot read current output stream position");
	}

	return true;

} // SetOutput

//---------------------------------------------------------------------------
bool KBlockCipher::SetTag()
//---------------------------------------------------------------------------
{
	if (m_sTag.empty()) return SetError("missing authentication tag");
	if (m_sTag.size() != m_iTagLength) return SetError(kFormat("tag size of {} but need {}", m_sTag.size(), m_iTagLength));

	kDebug(3, "set tag {}", KEncode::Hex(m_sTag));

	if (!::EVP_CIPHER_CTX_ctrl(m_evpctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(m_iTagLength), m_sTag.data()))
	{
		return SetError(GetOpenSSLError(kFormat("{}: cannot set tag", "decryption")));
	}

	m_sLastTag = m_sTag;
	m_bTagIsSet = true;

	return true;

} // SetTag

//---------------------------------------------------------------------------
bool KBlockCipher::SetIV()
//---------------------------------------------------------------------------
{
	if (m_sIV.empty()) return SetError("missing initialization vector");
	if (m_sIV.size() != m_iIVLength) return SetError(kFormat("IV size of {} but need {}", m_sIV.size(), m_iIVLength));

	kDebug(3, "set IV {}", KEncode::Hex(m_sIV));

	// complete the initialization by adding the IV
	// we use EVP_CipherInit() and not EVP_CipherInit_ex2() because the latter
	// is only supported from v3.0.0 onward
	if (!::EVP_CipherInit(
		m_evpctx,
		nullptr,
		nullptr,
		reinterpret_cast<const unsigned char*>(m_sIV.data()),
		m_Direction)
	)
	{
		return SetError(GetOpenSSLError(kFormat("cannot set IV {}", m_sCipherName)));
	}

	m_sLastIV = m_sIV;
	m_bIVIsSet = true;

	return true;

} // SetIV

//---------------------------------------------------------------------------
void KBlockCipher::PrepareNextRound()
//---------------------------------------------------------------------------
{
	m_sTag.clear();
	m_sIV.clear();
	m_bInitCompleted = false;
	m_bTagIsSet = false;
	m_bIVIsSet = false;

} // PrepareNextRound

//---------------------------------------------------------------------------
void KBlockCipher::Release() noexcept
//---------------------------------------------------------------------------
{
	if (m_evpctx)
	{
		::EVP_CIPHER_CTX_free(m_evpctx);
		m_evpctx = nullptr;
	}

} // Release

//---------------------------------------------------------------------------
bool KBlockCipher::Initialize(Algorithm algorithm, Bits bits)
//---------------------------------------------------------------------------
{
	m_Cipher      = GetCipher(algorithm, GetMode(), bits);

	if (!m_Cipher)
	{
#if !DEKAF2_HAS_ARIA
		if (algorithm == ARIA) return SetError("you need at least OpenSSL v1.1.1 for ARIA support");
#endif
		return SetError(kFormat("no cipher for {}-{}-{}", ToString(algorithm), ToString(GetMode()), ToString(bits)));
	}

#if OPENSSL_VERSION_NUMBER >= 0x030000000L
	m_sCipherName = ::EVP_CIPHER_get0_name (m_Cipher);
#else
	m_sCipherName = ::EVP_CIPHER_name (m_Cipher);
#endif
	m_evpctx      = ::EVP_CIPHER_CTX_new();

	if (!m_evpctx)
	{
		return SetError(GetOpenSSLError("cannot create context"));
	}

	// start the initialization
	// we use EVP_CipherInit() and not EVP_CipherInit_ex2() because the latter
	// is only supported from v3.0.0 onward
	if (!::EVP_CipherInit(
		m_evpctx,
		m_Cipher,
		nullptr,
		nullptr,
		m_Direction)
	)
	{
		return SetError(GetOpenSSLError(kFormat("cannot initialize cipher {}", m_sCipherName)));
	}

	// get required key and IV lengths
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
	m_iBlockSize  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_block_size (m_evpctx));
	m_iKeyLength  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_key_length (m_evpctx));
	m_iIVLength   = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_iv_length  (m_evpctx));
	m_iTagLength  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_tag_length (m_evpctx));
#else
	m_iBlockSize  = static_cast<std::size_t>(::EVP_CIPHER_CTX_block_size     (m_evpctx));
	m_iKeyLength  = static_cast<std::size_t>(::EVP_CIPHER_CTX_key_length     (m_evpctx));
	m_iIVLength   = static_cast<std::size_t>(::EVP_CIPHER_CTX_iv_length      (m_evpctx));
	if      (GetMode() == CCM) m_iTagLength = 12;
	else if (GetMode() == GCM) m_iTagLength = 16;
	else                       m_iTagLength =  0;
#endif

	kDebug(3, "direction: {}, cipher: {}, keylen: {}, IV len: {}{}, taglen: {}{}, blocksize: {}",
		   m_Direction ? "encryption" : "decryption",
		   m_sCipherName,
		   m_iKeyLength,
		   m_iIVLength,
		   m_bInlineIV ? " (inline)" : "",
		   m_iTagLength,
		   m_bInlineTag ? " (inline)" : "",
		   m_iBlockSize);

	return true;

} // Initialize

//---------------------------------------------------------------------------
bool KBlockCipher::StartNewData()
//---------------------------------------------------------------------------
{
	if (m_bInitCompleted) return true;

	if (!m_OutString) return SetError("no output was set");
	if (!m_bKeyIsSet) return SetError("no key or password was set");

	if (!m_evpctx || HasError()) return false;

	m_bInitCompleted = true;

	if (GetDirection() == Encrypt)
	{
		if (GetNeededIVLength() > 0)
		{
			if (m_sIV.empty())
			{
				auto iIV = GetNextIncrementalIV();

				if (iIV)
				{
					m_sIV = PrintIncrementalIV(iIV);
				}
				else
				{
					// if sIV is empty, just generate a random one
					m_sIV.resize(GetNeededIVLength());

					if (1 != ::RAND_bytes(reinterpret_cast<unsigned char*>(&m_sIV[0]), static_cast<int>(m_sIV.size())))
					{
						SetError(GetOpenSSLError(kFormat("cannot get random IV bytes {}", m_sCipherName)));
						return false;
					}
				}
			}

			kDebug(3, "iv {}", KEncode::Hex(m_sIV));

			// complete the initialization by adding the IV
			// we use EVP_CipherInit() and not EVP_CipherInit_ex2() because the latter
			// is only supported from v3.0.0 onward
			if (!::EVP_CipherInit(
				m_evpctx,
				nullptr,
				nullptr,
				reinterpret_cast<const unsigned char*>(m_sIV.data()),
				m_Direction)
			)
			{
				SetError(GetOpenSSLError(kFormat("cannot initialize cipher {}", m_sCipherName)));
				return false;
			}

			m_sLastIV = m_sIV;
		}

		if (m_bInlineTag)
		{
			// check if we will compute a tag and inline it, and if yes keep an empty
			// placeholder at the very beginning of the ciphertext
			if (m_iTagLength)
			{
				kDebug(3, "inline: appending {} of size {} at pos {}",
				          "tag placeholder",
				          m_iTagLength,
				          m_OutString->size());

				m_OutString->append(m_iTagLength, '\0');
			}
		}

		if (m_bInlineIV)
		{
			// check if we need an inline IV, and if yes insert it at the
			// beginning of the ciphertext
			if (GetNeededIVLength())
			{
				kAssert(m_sIV.size() == GetNeededIVLength(), "IV length != required length");

				kDebug(3, "inline: appending {} of size {} at pos {}",
				          "IV",
				          m_sIV.size(),
				          m_OutString->size());

				m_OutString->append(m_sIV);
			}
		}
	}
	else // decrypt
	{
		if (m_bInlineTag)
		{
			// extract an inserted auth tag from the start of the ciphertext
			m_iGetTagLength = m_iTagLength;
		}

		if (m_bInlineIV)
		{
			// extract an inserted IV from the start of the ciphertext
			m_iGetIVLength = m_iIVLength;
		}
		else if (GetNeededIVLength() > 0)
		{
			if (m_sIV.empty())
			{
				auto iIV = GetNextIncrementalIV();

				if (iIV)
				{
					m_sIV = PrintIncrementalIV(iIV);
				}
			}
		}
	}

	return true;

} // StartNewData

//---------------------------------------------------------------------------
bool KBlockCipher::AddString(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!m_evpctx || HasError()) return false;

	if (!m_bInitCompleted && !StartNewData()) return false;

	kDebug(3, "{}: adding string of size {}",
	          GetDirection() == Decrypt ? "decryption" : "encryption",
	          sInput.size());

	if (GetDirection() == Decrypt)
	{
		if (m_iTagLength && !m_bTagIsSet)
		{
			// shall we extract a leading auth tag from the input ciphertext?
			if (m_iGetTagLength)
			{
				auto iCopy = std::min(std::size_t(m_iGetTagLength), sInput.size());
				m_sTag.append(sInput.substr(0, iCopy));
				sInput.remove_prefix(iCopy);
				m_iGetTagLength -= iCopy;

				// return right here if we need more input
				if (m_iGetTagLength > 0) return true;
			}

			// and set the tag for decoding - we now either have it from inline or set from outside
			if (!SetTag()) return false;
		}

		if (m_iIVLength && !m_bIVIsSet)
		{
			// shall we extract a leading IV from the input ciphertext?
			if (m_iGetIVLength)
			{
				auto iCopy = std::min(std::size_t(m_iGetIVLength), sInput.size());
				m_sIV.append(sInput.substr(0, iCopy));
				sInput.remove_prefix(iCopy);
				m_iGetIVLength -= iCopy;

				// return right here if we need more input
				if (m_iGetIVLength > 0) return true;
			}

			// set the IV for decoding - we now either have it from inline or set from outside
			if (!SetIV()) return false;
		}
	}

	if (sInput.empty()) return true;

	// finally go on with encrypting or decrypting the ciphertext
	auto iOrigSize = m_OutString->size();
	// check if we have to provide more output to accomodate a block size of > 1 - a block
	// size of 1 means that there is no buffering
	auto iLocalBlockSize = (GetBlockSize() <= 1) ? 0 : GetBlockSize();
	m_OutString->resize(iOrigSize + sInput.size() + iLocalBlockSize);
	// we need operator[] to keep this compatible to C++11 / GCC6
	auto pOut = reinterpret_cast<unsigned char*>(&m_OutString->operator[](0) + iOrigSize);
	int iOutLen;

	if (GetMode() == Mode::CCM)
	{
		// tell the total input size for CCM - we can only call AddString once..
		if (!::EVP_CipherUpdate(
			m_evpctx,
			nullptr,
			&iOutLen,
			nullptr,
			static_cast<int>(sInput.size()))
		)
		{
			return SetError(GetOpenSSLError(kFormat("{} failed", m_Direction ? "encryption" : "decryption")));
		}
	}

	if (!::EVP_CipherUpdate(
		m_evpctx,
		pOut,
		&iOutLen,
		reinterpret_cast<const unsigned char*>(sInput.data()),
		static_cast<int>(sInput.size()))
	)
	{
		return SetError(GetOpenSSLError(kFormat("{} failed", m_Direction ? "encryption" : "decryption")));
	}

	kAssert(m_OutString->size() - iOrigSize >= static_cast<uint16_t>(iOutLen), "iLocalBlockSize too small");

	if (iLocalBlockSize) m_OutString->resize(iOrigSize + iOutLen);

	return true;

} // AddString

//---------------------------------------------------------------------------
bool KBlockCipher::AddStream(KStringView sInput)
//---------------------------------------------------------------------------
{
	KStringRef sBuffer;
	m_OutString = &sBuffer;
	KAtScopeEnd(m_OutString = nullptr);

	if (!AddString(sInput)) return false;

	m_OutStream->Write(sBuffer);
	return m_OutStream->Good();

} // AddStream

//---------------------------------------------------------------------------
bool KBlockCipher::Add(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (sInput.empty()) return true;
	if (m_OutString)    return AddString(sInput);
	if (m_OutStream)    return AddStream(sInput);

	return false;

} // Encrypt

//---------------------------------------------------------------------------
bool KBlockCipher::Add(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	if (GetMode() == Mode::CCM)
	{
		// in CCM mode we have to push the whole data in one run
		KString sBuffer;
		if (!kReadAll(InputStream, sBuffer)) return SetError("cannot read whole stream");
		return Add(sBuffer);
	}
	else
	{
		// in all other modes push blocks of input data
		std::array<char, KDefaultCopyBufSize> Buffer;

		for (;;)
		{
			auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());
			if (!Add(KStringView(Buffer.data(), iReadChunk))) return false;
			if (iReadChunk < Buffer.size()) return true;
		}
	}

} // Add

//---------------------------------------------------------------------------
bool KBlockCipher::Add(KInStream&& InputStream)
//---------------------------------------------------------------------------
{
	return Add(InputStream);

} // Add

//---------------------------------------------------------------------------
bool KBlockCipher::FinalizeString()
//---------------------------------------------------------------------------
{
	if (!m_evpctx || HasError()) return false;

#if OPENSSL_VERSION_NUMBER < 0x030000000L
	// do not call EVP_CipherFinal_ex() in CCM mode - it fails with old OpenSSL versions (and is not needed)
	if (GetDirection() == Decrypt && GetMode() == Mode::CCM) return true;
#endif

	// clear the outstring on any error return
	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard  = [this]() noexcept { m_OutString->clear(); };
	// construct the scope guard with a type
	auto guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	auto iOrigSize = m_OutString->size();

	if (GetBlockSize() > 1)
	{
		// prepare for a final flush from the operation
		m_OutString->resize(iOrigSize + m_iBlockSize);
	}

	// we need operator[] to keep this compatible to C++11 / GCC6
	auto pOut = reinterpret_cast<unsigned char*>(&m_OutString->operator[](0) + iOrigSize);

	int iOutLen;

	if (!::EVP_CipherFinal_ex(m_evpctx, pOut, &iOutLen))
	{
		return SetError(GetOpenSSLError(kFormat("{}: finalization failed", m_Direction ? "encryption" : "decryption")));
	}

	if (m_iTagLength && GetDirection() == Encrypt)
	{
		m_sTag.resize(m_iTagLength);

		if (!::EVP_CIPHER_CTX_ctrl(m_evpctx, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(m_iTagLength), &m_sTag[0]))
		{
			return SetError(GetOpenSSLError(kFormat("{}: cannot get tag", "encryption")));
		}

		if (m_bInlineTag && !m_OutStream && m_OutString)
		{
			kDebug(3, "{}: inserting tag of size {} inline at pos {}: {}",
			          "encryption",
			          m_sTag.size(),
			          m_iStartOfString,
			          KEncode::Hex(m_sTag));

			// insert the auth tag right at the begin of the string (which could be offset
			// if the string was not empty initially)
			std::copy(m_sTag.begin(), m_sTag.end(), m_OutString->begin() + m_iStartOfString);
		}

		m_sLastTag = m_sTag;
	}

	// disable the guard, the string is valid
	guard.release();

	if (GetBlockSize() > 1)
	{
		m_OutString->resize(iOrigSize + iOutLen);
	}
	else
	{
		// check if the assumption holds true that blocksize 1 means no further flush
		kAssert(iOutLen == 0, "blocksize is 1, but outlen is > 0");
	}

	if (GetDirection() == Encrypt)
	{
		kDebug(3, "{}: output has size {}, starts with {}",
		          "encryption",
		          m_OutString->size(),
		          KEncode::Hex(m_OutString->substr(0, 20)));
	}
	else
	{
		kDebug(3, "{}: output has size {}, starts with {}",
		          "decryption",
		          m_OutString->size(),
		          kEscapeForLogging(m_OutString->substr(0, 30)));
	}

	// do not release the context, we may want to run a new round
	// with the same context
	PrepareNextRound();

	m_OutString = nullptr;

	return true;

} // FinalizeString

//---------------------------------------------------------------------------
bool KBlockCipher::FinalizeStream()
//---------------------------------------------------------------------------
{
	KStringRef sBuffer;
	m_OutString = &sBuffer;
	KAtScopeEnd(m_OutString = nullptr);

	// we can not easily erase the written output in case the finalization
	// is invalid - we simply return with false and leave the action to the caller
	if (!FinalizeString()) return false;

	m_OutStream->Write(sBuffer);

	if (m_iTagLength &&
	    GetDirection() == Encrypt &&
	    m_bInlineTag &&
	    m_OutStream)
	{
		kDebug(3, "{}: inserting tag of size {} inline at pos {}: {}",
		          "encryption",
		          m_sTag.size(),
		          static_cast<size_t>(m_iStartOfStream),
		          KEncode::Hex(m_sTag));

		// insert the auth tag right at the begin of the stream
		auto CurPos = kGetWritePosition(*m_OutStream);

		if (m_iStartOfStream < 0                               ||
			CurPos < 0                                         ||
			!kSetWritePosition(*m_OutStream, m_iStartOfStream) ||
			!m_OutStream->Write(m_sTag)                        ||
			!kSetWritePosition(*m_OutStream, CurPos))
		{
			return SetError("cannot insert authentication tag into stream");
		}
	}

	m_OutStream->Flush();

	if (!m_OutStream->Good()) return false;

	m_OutStream = nullptr;

	return true;

} // FinalizeStream

//---------------------------------------------------------------------------
bool KBlockCipher::Finalize()
//---------------------------------------------------------------------------
{
	if (m_OutString) return FinalizeString();
	if (m_OutStream) return FinalizeStream();

	return false;

} // Finalize

//---------------------------------------------------------------------------
bool KBlockCipher::SingleRound(KStringView sInput, KStringRef& sOutput)
//---------------------------------------------------------------------------
{
	return SetOutput(sOutput) && Add(sInput) && Finalize();

} // SingleRound

DEKAF2_NAMESPACE_END

#endif
