#include "catch.hpp"

#include <dekaf2/crypto/rsa/kcsr.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
// ASN1_STRING_get0_data() only exists from OpenSSL 1.1.0 on
KStringView Asn1View(const ASN1_STRING* String)
//-----------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	return { reinterpret_cast<const char*>(::ASN1_STRING_get0_data(String)),
	         static_cast<std::size_t>(::ASN1_STRING_length(String)) };
#else
	return { reinterpret_cast<const char*>(String->data),
	         static_cast<std::size_t>(String->length) };
#endif
}

} // end of anonymous namespace

TEST_CASE("KCSR")
{
	KRSAKey Key(2048);
	REQUIRE ( Key.empty() == false );

	SECTION("CreateAndVerify")
	{
		KCSR Csr(Key, { "www.example.com", "example.com" }, "us", "TestOrg");
		REQUIRE ( Csr.empty()    == false );
		CHECK   ( Csr.HasError() == false );

		auto sPEM = Csr.GetPEM();
		CHECK ( sPEM.starts_with("-----BEGIN CERTIFICATE REQUEST-----") );

		auto sDER = Csr.GetDER();
		CHECK ( sDER.empty() == false );

		// reparse from DER and verify the self signature (proof of possession)
		const auto* pDER = reinterpret_cast<const unsigned char*>(sDER.data());
		auto* Req = ::d2i_X509_REQ(nullptr, &pDER, static_cast<long>(sDER.size()));
		REQUIRE ( Req != nullptr );

		auto* PubKey = ::X509_REQ_get_pubkey(Req);
		REQUIRE ( PubKey != nullptr );
		CHECK   ( ::X509_REQ_verify(Req, PubKey) == 1 );
		::EVP_PKEY_free(PubKey);

		// CN is the first domain
		char szCN[256];
		REQUIRE ( ::X509_NAME_get_text_by_NID(::X509_REQ_get_subject_name(Req), NID_commonName, szCN, sizeof(szCN)) > 0 );
		CHECK   ( KStringView(szCN) == "www.example.com" );

		// both domains are SAN entries
		auto* Exts = ::X509_REQ_get_extensions(Req);
		REQUIRE ( Exts != nullptr );

		auto* SANs = static_cast<GENERAL_NAMES*>(::X509V3_get_d2i(Exts, NID_subject_alt_name, nullptr, nullptr));
		REQUIRE ( SANs != nullptr );
		REQUIRE ( sk_GENERAL_NAME_num(SANs) == 2 );

		std::vector<KString> Domains;

		for (int i = 0; i < sk_GENERAL_NAME_num(SANs); ++i)
		{
			auto* Name = sk_GENERAL_NAME_value(SANs, i);
			REQUIRE ( Name->type == GEN_DNS );
			Domains.push_back(KString(Asn1View(Name->d.dNSName)));
		}

		CHECK ( Domains[0] == "www.example.com" );
		CHECK ( Domains[1] == "example.com"     );

		sk_GENERAL_NAME_pop_free(SANs, GENERAL_NAME_free);
		sk_X509_EXTENSION_pop_free(Exts, X509_EXTENSION_free);
		::X509_REQ_free(Req);
	}

	SECTION("Errors")
	{
		KCSR Csr;
		CHECK ( Csr.empty() == true );
		CHECK ( Csr.Create(Key, {}) == false );
		CHECK ( Csr.HasError()      == true  );

		KRSAKey NoKey;
		CHECK ( Csr.Create(NoKey, { "www.example.com" }) == false );
	}
}
