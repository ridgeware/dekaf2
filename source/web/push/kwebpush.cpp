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

#include <dekaf2/web/push/kwebpush.h>
#if DEKAF2_HAS_SQLITE3
#include <dekaf2/web/push/bits/kwebpushsqlitedb.h>
#endif
#include <dekaf2/web/push/bits/kwebpushksqldb.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
// extract origin (scheme://host[:port]) from a URL
static KString GetOrigin(KStringView sURL)
//-----------------------------------------------------------------------------
{
	KURL URL(sURL);
	KString sOrigin;
	sOrigin += URL.Protocol.Serialize();
	sOrigin += URL.Domain.Serialize();

	if (!URL.Port.empty())
	{
		sOrigin += ':';
		sOrigin += URL.Port.Serialize();
	}

	return sOrigin;

} // GetOrigin

//-----------------------------------------------------------------------------
static KString NormalizeContact(KString sContact)
//-----------------------------------------------------------------------------
{
	sContact.remove_prefix("mailto:");
	sContact.Trim();

	if (sContact.empty())
	{
		sContact = "admin@example.org";
	}

	return sContact;

} // NormalizeContact

//-----------------------------------------------------------------------------
KWebPush::KWebPush(std::unique_ptr<DB> DB, KString sContact)
//-----------------------------------------------------------------------------
: m_DB(std::move(DB))
, m_sContact(NormalizeContact(std::move(sContact)))
{
	Init();

} // ctor

#if DEKAF2_HAS_SQLITE3
//-----------------------------------------------------------------------------
KWebPush::KWebPush(KStringViewZ sDatabase, KString sContact)
//-----------------------------------------------------------------------------
: m_DB(std::make_unique<KWebPushSQLiteDB>(KString(sDatabase)))
, m_sContact(NormalizeContact(std::move(sContact)))
{
	Init();

} // ctor (SQLite convenience)
#endif

//-----------------------------------------------------------------------------
KWebPush::KWebPush(KSQL& db, KString sContact)
//-----------------------------------------------------------------------------
: m_DB(std::make_unique<KWebPushKSQLDB>(db))
, m_sContact(NormalizeContact(std::move(sContact)))
{
	Init();

} // ctor (KSQL convenience)

//-----------------------------------------------------------------------------
bool KWebPush::Init()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_DB)
	{
		return SetError("no database backend configured");
	}

	if (!m_DB->CreateTables())
	{
		return SetError(m_DB->GetLastError());
	}

	if (!LoadVAPIDKeys())
	{
		if (!GenerateVAPIDKeys())
		{
			return false;
		}
	}

	m_bReady = true;

	kDebug(2, "KWebPush initialized, public key: {}...", m_sPublicKeyB64.Left(20));

	return true;

} // Init

//-----------------------------------------------------------------------------
bool KWebPush::GenerateVAPIDKeys()
//-----------------------------------------------------------------------------
{
	if (!m_VAPIDKey.Generate())
	{
		return SetError(kFormat("cannot generate VAPID keys: {}", m_VAPIDKey.Error()));
	}

	KString sPrivateKeyB64 = KBase64Url::Encode(m_VAPIDKey.GetPrivateKeyRaw());
	m_sPublicKeyB64        = KBase64Url::Encode(m_VAPIDKey.GetPublicKeyRaw());
	KString sPrivateKeyPEM = m_VAPIDKey.GetPEM(true);

	if (!m_DB->StoreVAPIDKey("private_key", sPrivateKeyB64)
	 || !m_DB->StoreVAPIDKey("public_key",  m_sPublicKeyB64)
	 || !m_DB->StoreVAPIDKey("private_key_pem", sPrivateKeyPEM))
	{
		return SetError(kFormat("cannot store VAPID keys: {}", m_DB->GetLastError()));
	}

	kDebug(2, "generated new VAPID key pair");

	return true;

} // GenerateVAPIDKeys

//-----------------------------------------------------------------------------
bool KWebPush::LoadVAPIDKeys()
//-----------------------------------------------------------------------------
{
	m_sPublicKeyB64 = m_DB->LoadVAPIDKey("public_key");
	KString sPrivateKeyPEM = m_DB->LoadVAPIDKey("private_key_pem");

	if (m_sPublicKeyB64.empty() || sPrivateKeyPEM.empty())
	{
		return false;
	}

	if (!m_VAPIDKey.CreateFromPEM(sPrivateKeyPEM))
	{
		return SetError(kFormat("cannot load VAPID private key from PEM: {}", m_VAPIDKey.Error()));
	}

	kDebug(2, "loaded VAPID keys from database");

	return true;

} // LoadVAPIDKeys

//-----------------------------------------------------------------------------
bool KWebPush::Subscribe(Subscription sub)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_bReady)
	{
		return SetError("initialization failed");
	}

	if (!m_DB->StoreSubscription(sub))
	{
		return SetError(kFormat("cannot store subscription: {}", m_DB->GetLastError()));
	}

	kDebug(2, "stored push subscription for user '{}', endpoint: {}...",
	       sub.sUser, sub.sEndpoint.Left(60));

	return true;

} // Subscribe

//-----------------------------------------------------------------------------
bool KWebPush::Unsubscribe(KStringView sEndpoint)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_bReady)
	{
		return SetError("initialization failed");
	}

	if (!m_DB->RemoveSubscription(sEndpoint))
	{
		return SetError(kFormat("cannot delete subscription: {}", m_DB->GetLastError()));
	}

	kDebug(2, "removed push subscription: {}...", KStringView(sEndpoint).Left(60));

	return true;

} // Unsubscribe

//-----------------------------------------------------------------------------
bool KWebPush::UnsubscribeUser(KStringView sUser)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_bReady)
	{
		return SetError("initialization failed");
	}

	if (!m_DB->RemoveUserSubscriptions(sUser))
	{
		return SetError(kFormat("cannot delete subscriptions for user '{}': {}", sUser, m_DB->GetLastError()));
	}

	kDebug(2, "removed all push subscriptions for user '{}'", sUser);

	return true;

} // UnsubscribeUser

//-----------------------------------------------------------------------------
bool KWebPush::HasSubscriptions() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_bReady)
	{
		return false;
	}

	return m_DB->HasSubscriptions();

} // HasSubscriptions

//-----------------------------------------------------------------------------
std::vector<KWebPush::Subscription> KWebPush::ListSubscriptions(KStringView sUser) const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (!m_bReady)
	{
		return {};
	}

	return m_DB->GetSubscriptions(sUser);

} // ListSubscriptions

//-----------------------------------------------------------------------------
std::size_t KWebPush::SendToUser(KStringView sUser, const KJSON& jPayload)
//-----------------------------------------------------------------------------
{
	if (!m_bReady)
	{
		return 0;
	}

	KString sPayload = jPayload.dump();

	std::vector<Subscription> Subs;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		Subs = m_DB->GetSubscriptions(sUser);
	}

	std::size_t iSuccess = 0;

	for (const auto& sub : Subs)
	{
		if (SendPush(sub, sPayload))
		{
			++iSuccess;
		}
	}

	kDebug(2, "sent push to {}/{} subscriptions for user '{}'",
	       iSuccess, Subs.size(), sUser);

	return iSuccess;

} // SendToUser

//-----------------------------------------------------------------------------
std::size_t KWebPush::SendToAll(const KJSON& jPayload)
//-----------------------------------------------------------------------------
{
	if (!m_bReady)
	{
		return 0;
	}

	KString sPayload = jPayload.dump();

	std::vector<Subscription> Subs;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		Subs = m_DB->GetSubscriptions();
	}

	kDebug(2, "SendToAll: {} subscription(s)", Subs.size());

	std::size_t iSuccess = 0;

	for (const auto& sub : Subs)
	{
		if (SendPush(sub, sPayload))
		{
			++iSuccess;
		}
	}

	kDebug(2, "SendToAll: sent {}/{} successfully", iSuccess, Subs.size());

	return iSuccess;

} // SendToAll

//-----------------------------------------------------------------------------
KString KWebPush::BuildVAPIDToken(KStringView sAudience) const
//-----------------------------------------------------------------------------
{
	KJSON jHeader;
	jHeader["typ"] = "JWT";
	jHeader["alg"] = "ES256";

	auto iExp = static_cast<uint64_t>(KUnixTime::now().to_time_t()) + 12 * 3600;

	KJSON jPayload;
	jPayload["aud"] = sAudience;
	jPayload["exp"] = iExp;
	jPayload["sub"] = kFormat("mailto:{}", m_sContact);

	auto sHeaderB64  = KBase64Url::Encode(jHeader.dump());
	auto sPayloadB64 = KBase64Url::Encode(jPayload.dump());

	KString sSigningInput = sHeaderB64;
	sSigningInput += '.';
	sSigningInput += sPayloadB64;

	KECSign Signer;
	KString sRawSig = Signer.Sign(m_VAPIDKey, sSigningInput);

	if (sRawSig.empty())
	{
		kDebug(1, "failed to sign VAPID token: {}", Signer.Error());
		return {};
	}

	sSigningInput += '.';
	sSigningInput += KBase64Url::Encode(sRawSig);

	return sSigningInput;

} // BuildVAPIDToken

//-----------------------------------------------------------------------------
KString KWebPush::EncryptPayload(KStringView sPayload, KStringView sP256dhB64, KStringView sAuthB64) const
//-----------------------------------------------------------------------------
{
	// decode subscriber's public key (65 bytes) and auth secret (16 bytes)
	KString sUAPublic   = KBase64Url::Decode(sP256dhB64);
	KString sAuthSecret = KBase64Url::Decode(sAuthB64);

	if (sUAPublic.size() != 65)
	{
		kDebug(1, "subscriber p256dh has wrong size: {} (expected 65)", sUAPublic.size());
		return {};
	}

	if (sAuthSecret.size() != 16)
	{
		kDebug(1, "subscriber auth has wrong size: {} (expected 16)", sAuthSecret.size());
		return {};
	}

	// generate ephemeral key pair and derive shared secret via ECDH
	KECKey EphemeralKey(true);

	if (EphemeralKey.empty())
	{
		kDebug(1, "cannot generate ephemeral key: {}", EphemeralKey.Error());
		return {};
	}

	KString sECDHSecret = EphemeralKey.DeriveSharedSecret(sUAPublic);

	if (sECDHSecret.empty())
	{
		kDebug(1, "ECDH key exchange failed");
		return {};
	}

	KString sASPublic = EphemeralKey.GetPublicKeyRaw();

	// RFC 8291 key derivation via HKDF
	KString sPRK = KHKDF::Extract(sAuthSecret, sECDHSecret);

	KString sInfoCtx = "WebPush: info";
	sInfoCtx += '\0';
	sInfoCtx.append(sUAPublic);
	sInfoCtx.append(sASPublic);

	KString sIKM = KHKDF::Expand(sPRK, sInfoCtx, 32);

	// generate random salt
	unsigned char aSalt[16];
	::RAND_bytes(aSalt, 16);
	KString sSalt(reinterpret_cast<const char*>(aSalt), 16);

	KString sPRK2 = KHKDF::Extract(sSalt, sIKM);

	KString sCEKInfo = "Content-Encoding: aes128gcm";
	sCEKInfo += '\0';
	KString sCEK = KHKDF::Expand(sPRK2, sCEKInfo, 16);

	KString sNonceInfo = "Content-Encoding: nonce";
	sNonceInfo += '\0';
	KString sNonce = KHKDF::Expand(sPRK2, sNonceInfo, 12);

	// plaintext with padding delimiter (RFC 8188)
	KString sPlaintext;
	sPlaintext += sPayload;
	sPlaintext += '\x02';

	// AES-128-GCM encrypt
	KUniquePtr<EVP_CIPHER_CTX, ::EVP_CIPHER_CTX_free> cipherCtx(::EVP_CIPHER_CTX_new());

	if (!cipherCtx)
	{
		kDebug(1, "cannot create cipher context");
		return {};
	}

	if (::EVP_EncryptInit_ex(cipherCtx.get(), ::EVP_aes_128_gcm(), nullptr,
	                         reinterpret_cast<const unsigned char*>(sCEK.data()),
	                         reinterpret_cast<const unsigned char*>(sNonce.data())) != 1)
	{
		kDebug(1, "EVP_EncryptInit_ex failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	std::vector<unsigned char> aCiphertext(sPlaintext.size() + 16);
	int iOutLen = 0;

	if (::EVP_EncryptUpdate(cipherCtx.get(), aCiphertext.data(), &iOutLen,
	                        reinterpret_cast<const unsigned char*>(sPlaintext.data()),
	                        static_cast<int>(sPlaintext.size())) != 1)
	{
		kDebug(1, "EVP_EncryptUpdate failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	int iFinalLen = 0;

	if (::EVP_EncryptFinal_ex(cipherCtx.get(), aCiphertext.data() + iOutLen, &iFinalLen) != 1)
	{
		kDebug(1, "EVP_EncryptFinal_ex failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	int iCipherLen = iOutLen + iFinalLen;

	unsigned char aTag[16];

	if (::EVP_CIPHER_CTX_ctrl(cipherCtx.get(), EVP_CTRL_GCM_GET_TAG, 16, aTag) != 1)
	{
		kDebug(1, "GCM GET_TAG failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	// build aes128gcm content-coding:
	// salt(16) || rs(4) || idlen(1) || keyid(65) || ciphertext || tag(16)
	KString sBody;
	sBody.append(reinterpret_cast<const char*>(aSalt), 16);

	uint32_t iRS = 4096;
	unsigned char aRS[4] = {
		static_cast<unsigned char>((iRS >> 24) & 0xFF),
		static_cast<unsigned char>((iRS >> 16) & 0xFF),
		static_cast<unsigned char>((iRS >>  8) & 0xFF),
		static_cast<unsigned char>((iRS      ) & 0xFF)
	};
	sBody.append(reinterpret_cast<const char*>(aRS), 4);
	sBody += static_cast<char>(65);
	sBody.append(sASPublic);
	sBody.append(reinterpret_cast<const char*>(aCiphertext.data()), static_cast<std::size_t>(iCipherLen));
	sBody.append(reinterpret_cast<const char*>(aTag), 16);

	return sBody;

} // EncryptPayload

//-----------------------------------------------------------------------------
bool KWebPush::SendPush(const Subscription& sub, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	KString sAudience = GetOrigin(sub.sEndpoint);

	if (sAudience.empty())
	{
		kDebug(1, "cannot extract origin from endpoint: {}...", sub.sEndpoint.Left(60));
		return false;
	}

	KString sJWT = BuildVAPIDToken(sAudience);

	if (sJWT.empty())
	{
		kDebug(1, "failed to build VAPID token");
		return false;
	}

	KString sEncryptedBody = EncryptPayload(sPayload, sub.sP256dh, sub.sAuth);

	if (sEncryptedBody.empty())
	{
		kDebug(1, "failed to encrypt push payload");
		return false;
	}

	KString sAuthHeader = kFormat("vapid t={}, k={}", sJWT, m_sPublicKeyB64);

	KWebClient Client;
	Client.SetTimeout(10);
	Client.AddHeader(KHTTPHeader::AUTHORIZATION, sAuthHeader);
	Client.AddHeader(KHTTPHeader::CONTENT_ENCODING, "aes128gcm");
	Client.AddHeader("TTL", "86400");

	auto sResponse = Client.Post(sub.sEndpoint, sEncryptedBody, KMIME::BINARY);
	auto iStatus   = Client.GetStatusCode();

	kDebug(iStatus < 300 ? 3 : 1, "push to {}... returned HTTP {}", sub.sEndpoint.Left(60), iStatus);

	if (iStatus == 201)
	{
		return true;
	}

	// 403 BadJwtToken: VAPID key mismatch
	// 404 or 410: subscription expired/gone — remove
	if (iStatus == 403 || iStatus == 404 || iStatus == 410)
	{
		kDebug(1, "push subscription invalid (HTTP {}): {} — removing: {}...",
		       iStatus, sResponse.Left(300), sub.sEndpoint.Left(60));

		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_DB->RemoveSubscription(sub.sEndpoint);

		return false;
	}

	kDebug(1, "push notification failed with HTTP {}: {}", iStatus, sResponse.Left(200));

	return false;

} // SendPush

DEKAF2_NAMESPACE_END
