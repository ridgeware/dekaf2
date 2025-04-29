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
#include "kencode.h"
#include "klog.h"
#include "kwrite.h"  // for kGetWritePosition()
#include "kscopeguard.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/kdferr.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/evperr.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KAES::KAES(
	KStringRef&  sOutput,
	Direction    direction,
	Algorithm    algorithm,
	Mode         mode,
	Bits         bits,
	bool         bInlineIVandTag
)
//---------------------------------------------------------------------------
: m_OutString(&sOutput)
, m_Direction(direction)
, m_bInlineIVandTag(bInlineIVandTag)
{
	Initialize(algorithm, mode, bits);

} // ctor

//---------------------------------------------------------------------------
KAES::KAES(
	KOutStream&  OutStream,
	Direction    direction,
	Algorithm    algorithm,
	Mode         mode,
	Bits         bits,
	bool         bInlineIVandTag
)
//---------------------------------------------------------------------------
: m_OutStream(&OutStream)
, m_Direction(direction)
, m_bInlineIVandTag(bInlineIVandTag)
{
	if (Initialize(algorithm, mode, bits))
	{
		if (m_Direction == Encrypt && m_bInlineIVandTag && m_iTagLength)
		{
			// check if the stream is repositionable, and note the current position
			// to insert the tag later
			m_StartOfStream = kGetWritePosition(*m_OutStream);
			if (m_StartOfStream < 0) SetError("cannot read stream position");
		}
	}

} // ctor

//---------------------------------------------------------------------------
KAES::KAES(KAES&& other) noexcept
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
, m_StartOfStream(other.m_StartOfStream)
, m_Direction(other.m_Direction)
, m_bInlineIVandTag(other.m_bInlineIVandTag)
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
KAES::~KAES()
//---------------------------------------------------------------------------
{
	Finalize();
	Release();

} // dtor

//---------------------------------------------------------------------------
KString KAES::GetOpenSSLError()
//---------------------------------------------------------------------------
{
	KString sError;

	auto ec = ::ERR_get_error();

	if (ec)
	{
		sError.resize(256);
		::ERR_error_string_n(ec, &sError[0], sError.size());
		auto iSize = ::strlen(&sError[0]);
		sError.resize(iSize);
	}

	return sError;

} // GetOpenSSLError

//---------------------------------------------------------------------------
const evp_cipher_st* KAES::GetCipher(Algorithm algorithm, Mode mode, Bits bits)
//---------------------------------------------------------------------------
{
	switch (algorithm)
	{
		case AES:
			switch (mode)
			{

#if DEKAF2_AES_WITH_ECB
				case ECB:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ecb();
						case B192: return ::EVP_aes_192_ecb();
						case B256: return ::EVP_aes_256_ecb();
					}
					break;
#endif
				case CBC:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_cbc();
						case B192: return ::EVP_aes_192_cbc();
						case B256: return ::EVP_aes_256_cbc();
					}
					break;

#if DEKAF2_AES_WITH_CCM
				case CCM:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_ccm();
						case B192: return ::EVP_aes_192_ccm();
						case B256: return ::EVP_aes_256_ccm();
					}
					break;
#endif
				case GCM:
					switch (bits)
					{
						case B128: return ::EVP_aes_128_gcm();
						case B192: return ::EVP_aes_192_gcm();
						case B256: return ::EVP_aes_256_gcm();
					}
					break;

			} // AES
			break;
	}

	return nullptr;

} // GetCipher

//---------------------------------------------------------------------------
KString KAES::CreateKeyFromPasswordHKDF(uint16_t iKeyLen, KStringView sPassword, KStringView sSalt)
//---------------------------------------------------------------------------
{
	::EVP_KDF* kdf = ::EVP_KDF_fetch(nullptr, "HKDF", nullptr);
	::EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
	::EVP_KDF_free(kdf);

	::OSSL_PARAM params[5], *p = params;
	*p++ = ::OSSL_PARAM_construct_utf8_string (OSSL_KDF_PARAM_DIGEST, const_cast<char*>(SN_sha256), 0);
	*p++ = ::OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY   , const_cast<char*>("secret") , 6);
	*p++ = ::OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO  , const_cast<char*>("label")  , 5);
	*p++ = ::OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT  , const_cast<char*>("salt")   , 4);
	*p   = ::OSSL_PARAM_construct_end();

	KString sKey;
	sKey.resize(iKeyLen);

	if (::EVP_KDF_derive(kctx, reinterpret_cast<unsigned char*>(&sKey[0]), sKey.size(), params) <= 0)
	{
		sKey.clear();
		kWarning("cannot generate key: {}", GetOpenSSLError());
	}

	::EVP_KDF_CTX_free(kctx);

	return sKey;

} // CreateKeyFromPassword

//---------------------------------------------------------------------------
KString KAES::CreateKeyFromPasswordPKCS5
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
		kWarning("cannot generate key: {}", GetOpenSSLError());
	}

	return sKey;

} // CreateKeyFromPassword

//---------------------------------------------------------------------------
bool KAES::Initialize(
	Algorithm   algorithm,
	Mode        mode,
	Bits        bits
)
//---------------------------------------------------------------------------
{
	m_Cipher      = GetCipher(algorithm, mode, bits);
	m_sCipherName = ::EVP_CIPHER_get0_name (m_Cipher);
	m_evpctx      = ::EVP_CIPHER_CTX_new();

	if (!m_evpctx)
	{
		return SetError(kFormat("cannot create context: {}", GetOpenSSLError()));
	}

	// start the initialization
	if (!::EVP_CipherInit_ex2(
		m_evpctx,
		m_Cipher,
		nullptr,
		nullptr,
		m_Direction,
		nullptr)
	)
	{
		return SetError(kFormat("cannot initialize cipher {}: {}", m_sCipherName, GetOpenSSLError()));
	}

	// get required key and IV lengths
	m_iKeyLength  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_key_length (m_evpctx));
	m_iIVLength   = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_iv_length  (m_evpctx));
	m_iTagLength  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_tag_length (m_evpctx));
	m_iBlockSize  = static_cast<std::size_t>(::EVP_CIPHER_CTX_get_block_size (m_evpctx));

	kDebug(3, "direction: {}, cipher: {}, keylen: {}, IV len: {}, taglen: {}, blocksize: {}",
		   m_Direction ? "encrypt" : "decrypt", m_sCipherName, m_iKeyLength, m_iIVLength,
		   m_iTagLength, m_iBlockSize);

	return true;

} // Initialize

//---------------------------------------------------------------------------
bool KAES::SetKey(KStringView sKey)
//---------------------------------------------------------------------------
{
	if (sKey.size() < GetNeededKeyLength())
	{
		return SetError(kFormat("key length is {} bytes, but at least {} bytes are required", sKey.size(), m_iKeyLength));
	}

	// continue the initialization by adding the key
	if (!::EVP_CipherInit_ex2(
		m_evpctx,
		nullptr,
		reinterpret_cast<const unsigned char*>(sKey.data()),
		nullptr,
		m_Direction,
		nullptr)
	)
	{
		SetError(kFormat("cannot initialize cipher {}: {}", m_sCipherName, GetOpenSSLError()));
		Release();
		return false;
	}

	m_bKeyIsSet = true;

	return true;

} // SetKey

//---------------------------------------------------------------------------
bool KAES::SetPassword(KStringView sPassword, KStringView sSalt)
//---------------------------------------------------------------------------
{
	return SetKey(CreateKeyFromPasswordHKDF(GetNeededKeyLength(), sPassword, sSalt));

} // SetPassword

//---------------------------------------------------------------------------
bool KAES::CompleteInitialization()
//---------------------------------------------------------------------------
{
	if (m_bInitCompleted) return true;

	if (!m_bKeyIsSet) return SetError("no key or password was set");

	if (!m_evpctx || HasError()) return false;

	m_bInitCompleted = true;

	if (GetDirection() == Encrypt)
	{
		if (GetNeededIVLength() > 0)
		{
			if (m_sIV.empty())
			{
				// if sIV is empty, just generate a random one
				m_sIV.resize(GetNeededIVLength());

				if (1 != ::RAND_bytes(reinterpret_cast<unsigned char*>(&m_sIV[0]), static_cast<int>(m_sIV.size())))
				{
					SetError(kFormat("cannot get random IV bytes {}: {}", m_sCipherName, GetOpenSSLError()));
					Release();
					return false;
				}
			}

			// complete the initialization by adding key and IV
			if (!::EVP_CipherInit_ex2(
				m_evpctx,
				nullptr,
				nullptr,
				reinterpret_cast<const unsigned char*>(m_sIV.data()),
				m_Direction,
				nullptr)
			)
			{
				SetError(kFormat("cannot initialize cipher {}: {}", m_sCipherName, GetOpenSSLError()));
				Release();
				return false;
			}
		}

		if (m_bInlineIVandTag)
		{
			if (!m_OutString) return SetError("no output string");

			// check if we will compute a tag, and if yes keep an empty
			// placeholder at the very beginning of the ciphertext
			if (m_iTagLength)
			{
				m_OutString->append(m_iTagLength, '\0');
			}
			if (GetNeededIVLength())
			{
				m_OutString->append(m_sIV);
			}
		}
	}
	else // decrypt
	{
		if (m_bInlineIVandTag)
		{
			// extract an inserted auth tag from the start of the ciphertext
			m_iGetTagLength = m_iTagLength;
			// extract an inserted IV from the start of the ciphertext
			m_iGetIVLength  = m_iIVLength;
		}
	}

	return true;

} // CompleteInitialization

//---------------------------------------------------------------------------
bool KAES::SetInitializationVector(KStringView sIV)
//---------------------------------------------------------------------------
{
	if (!GetNeededIVLength()) return SetError("initialization vector not supported for selected cipher");
	if (GetNeededIVLength() != sIV.size())
	{
		SetError(kFormat("initialization vector for {} must have {} bits, but has only {}",
						 m_sCipherName, GetNeededIVLength() * 8, m_sIV.size() * 8));
	}
	m_sIV = sIV;
	return true;
}

//---------------------------------------------------------------------------
bool KAES::SetAuthenticationTag(KStringView sTag)
//---------------------------------------------------------------------------
{
	if (!m_iTagLength) return SetError("authentication tag not supported for selected cipher");
	m_sTag = sTag;
	return true;
}

//---------------------------------------------------------------------------
void KAES::SetOutput(KStringRef& sOutput)
//---------------------------------------------------------------------------
{
	m_OutStream = nullptr;
	m_OutString = &sOutput;
}

//---------------------------------------------------------------------------
void KAES::SetOutput(KOutStream& OutStream)
//---------------------------------------------------------------------------
{
	m_OutStream = &OutStream;
	m_OutString = nullptr;
};

//---------------------------------------------------------------------------
void KAES::Release() noexcept
//---------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	if (m_evpctx)
	{
		::EVP_CIPHER_CTX_free(m_evpctx);
		m_evpctx = nullptr;
	}

	DEKAF2_LOG_EXCEPTION

} // Release

//---------------------------------------------------------------------------
bool KAES::AddString(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!m_evpctx || HasError()) return false;

	if (!m_bInitCompleted)
	{
		if (!CompleteInitialization()) return false;
	}

	if (GetDirection() == Decrypt)
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

		// shall we extract a leading IV from the input ciphertext?
		if (m_iGetIVLength)
		{
			auto iCopy = std::min(std::size_t(m_iGetIVLength), sInput.size());
			m_sIV.append(sInput.substr(0, iCopy));
			sInput.remove_prefix(iCopy);
			m_iGetIVLength -= iCopy;

			// return right here if we need more input
			if (m_iGetIVLength > 0) return true;

			// complete the initialization by adding the IV
			if (!::EVP_CipherInit_ex2(
									  m_evpctx,
									  nullptr,
									  nullptr,
									  reinterpret_cast<const unsigned char*>(m_sIV.data()),
									  m_Direction,
									  nullptr)
				)
			{
				Release();
				return SetError(kFormat("cannot set IV {}: {}", m_sCipherName, GetOpenSSLError()));
			}
		}
	}

	// finally go on with encrypting or decrypting the ciphertext
	auto iOrigSize = m_OutString->size();
	m_OutString->resize(iOrigSize + sInput.size() + m_iBlockSize);
	auto pOut = reinterpret_cast<unsigned char*>(m_OutString->data()) + iOrigSize;
	int iOutLen;

	if (!::EVP_CipherUpdate(
		m_evpctx,
		reinterpret_cast<unsigned char*>(pOut),
		&iOutLen,
		reinterpret_cast<const unsigned char*>(sInput.data()),
		static_cast<int>(sInput.size()))
	)
	{
		return SetError(kFormat("{} failed: {}", m_Direction ? "encryption" : "decryption", GetOpenSSLError()));
	}

	m_OutString->resize(iOrigSize + iOutLen);

	return true;

} // EncryptString

//---------------------------------------------------------------------------
bool KAES::AddStream(KStringView sInput)
//---------------------------------------------------------------------------
{
	KStringRef sBuffer;
	m_OutString = &sBuffer;
	KAtScopeEnd(m_OutString = nullptr);

	if (!AddString(sInput)) return false;

	m_OutStream->Write(sBuffer);
	return m_OutStream->Good();

} // EncryptStream

//---------------------------------------------------------------------------
bool KAES::Add(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (sInput.empty()) return true;
	if (m_OutString)    return AddString(sInput);
	if (m_OutStream)    return AddStream(sInput);

	return false;

} // Encrypt

//---------------------------------------------------------------------------
bool KAES::Add(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	std::array<char, KDefaultCopyBufSize> Buffer;

	for (;;)
	{
		auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());
		if (!Add(KStringView(Buffer.data(), iReadChunk))) return false;
		if (iReadChunk < Buffer.size()) return true;
	}

} // Update

//---------------------------------------------------------------------------
bool KAES::Add(KInStream&& InputStream)
//---------------------------------------------------------------------------
{
	return Add(InputStream);

} // Encrypt

//---------------------------------------------------------------------------
bool KAES::FinalizeString()
//---------------------------------------------------------------------------
{
	if (!m_evpctx || HasError()) return false;

	auto iOrigSize = m_OutString->size();
	m_OutString->resize(iOrigSize + m_iBlockSize);
	auto pOut = reinterpret_cast<unsigned char*>(m_OutString->data()) + iOrigSize;

	if (m_iTagLength && GetDirection() == Decrypt)
	{
		if (m_sTag.empty()) return SetError("missing authentication tag");
		if (m_sTag.size() != m_iTagLength) return SetError(kFormat("tag size of {} but need {}", m_sTag.size(), m_iTagLength));
		::EVP_CIPHER_CTX_ctrl(m_evpctx, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(m_iTagLength), m_sTag.data());
	}

	int iOutLen;

	if (!::EVP_CipherFinal_ex(m_evpctx, pOut, &iOutLen))
	{
		return SetError(kFormat("{}: finalization failed: {}",
								m_Direction ? "encryption" : "decryption",
								GetOpenSSLError()));
	}

	if (m_iTagLength && GetDirection() == Encrypt)
	{
		m_sTag.resize(m_iTagLength);
		::EVP_CIPHER_CTX_ctrl(m_evpctx, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(m_iTagLength), &m_sTag[0]);

		if (m_bInlineIVandTag && !m_OutStream && m_OutString)
		{
			// insert the auth tag right at the begin of the string
			std::copy(m_sTag.begin(), m_sTag.begin() + m_sTag.size(), m_OutString->begin());
		}
	}

	m_OutString->resize(iOrigSize + iOutLen);

	Release();

	m_OutString = nullptr;

	return true;

} // FinalizeString

//---------------------------------------------------------------------------
bool KAES::FinalizeStream()
//---------------------------------------------------------------------------
{
	KStringRef sBuffer;
	m_OutString = &sBuffer;
	KAtScopeEnd(m_OutString = nullptr);

	if (!FinalizeString()) return false;

	m_OutStream->Write(sBuffer);

	if (m_iTagLength && GetDirection() == Encrypt && m_bInlineIVandTag && m_OutStream)
	{
		// insert the auth tag right at the begin of the stream
		if (m_StartOfStream < 0) return SetError("cannot insert authentication tag into stream");
		if (!kSetWritePosition(*m_OutStream, m_StartOfStream)) return SetError("cannot insert authentication tag into stream");
		m_OutStream->Write(m_sTag);
		if (!kForward(*m_OutStream)) return SetError("cannot insert authentication tag into stream");
	}

	m_OutStream->Flush();

	if (!m_OutStream->Good()) return false;

	m_OutStream = nullptr;

	return true;

} // FinalizeStream

//---------------------------------------------------------------------------
bool KAES::Finalize()
//---------------------------------------------------------------------------
{
	if (m_OutString) return FinalizeString();
	if (m_OutStream) return FinalizeStream();

	return false;

} // Finalize

DEKAF2_NAMESPACE_END
