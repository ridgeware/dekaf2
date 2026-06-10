#include "catch.hpp"
#include <dekaf2/crypto/cipher/kaes.h>
#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/random/krandom.h>
#include <vector>

#if DEKAF2_HAS_AES

using namespace dekaf2;

TEST_CASE("KAES")
{
	SECTION("AES ECB 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::ECB, KAES::B128);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::ECB, KAES::B128);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES ECB 256")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::ECB, KAES::B256);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::ECB, KAES::B256);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CBC 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CBC, KAES::B128);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CBC, KAES::B128);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CBC 192")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CBC, KAES::B192);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CBC, KAES::B192);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CBC 256")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CBC, KAES::B256);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CBC, KAES::B256);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CCM 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CCM, KAES::B128);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CCM, KAES::B128);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CCM 192")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CCM, KAES::B192);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CCM, KAES::B192);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES CCM 256")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::CCM, KAES::B256);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::CCM, KAES::B256);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES GCM 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::GCM, KAES::B128);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::GCM, KAES::B128);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES GCM 192")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::GCM, KAES::B192);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::GCM, KAES::B192);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("AES GCM 256")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(KAES::GCM, KAES::B256);
		Encryptor.SetOutput(sEncrypted);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(KAES::GCM, KAES::B256);
		Decryptor.SetOutput(sDecrypted);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

	SECTION("Key generation")
	{
		KStringView sClearText = "this is a secret message for your eyes only";

		KString sEncrypted1;
		{
			KToAES Encryptor(KAES::GCM, KAES::B256);
			Encryptor.SetOutput(sEncrypted1);
			Encryptor.SetPassword("MySecretPassword");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted2;
		{
			KToAES Encryptor(KAES::GCM, KAES::B256);
			Encryptor.SetOutput(sEncrypted2);
			Encryptor.SetPassword("MyOtherPassword");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted3;
		{
			KToAES Encryptor(KAES::GCM, KAES::B256);
			Encryptor.SetOutput(sEncrypted3);
			Encryptor.SetPassword("MySecretPassword", "salt1");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted4;
		{
			KToAES Encryptor(KAES::GCM, KAES::B256);
			Encryptor.SetOutput(sEncrypted4);
			Encryptor.SetPassword("MySecretPassword", "salt2");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		CHECK ( sEncrypted1 != sEncrypted2 );
		CHECK ( sEncrypted2 != sEncrypted3 );
		CHECK ( sEncrypted1 != sEncrypted3 );
		CHECK ( sEncrypted3 != sEncrypted4 );
		CHECK ( sEncrypted1 != sEncrypted4 );
	}

	SECTION("Incremental IV")
	{
		KBlockCipher Enc(
			KBlockCipher::Encrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			true
		);

		KBlockCipher Dec(
			KBlockCipher::Decrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			true
		);

		auto sKey = kGetRandom(Enc.GetNeededKeyLength());

		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Dec.SetKey(sKey) );
		CHECK ( Enc.SetAutoIncrementNonceAsIV() );
		CHECK ( Dec.SetAutoIncrementNonceAsIV() );

		KString sOrig = "abcdefghijklmnopqrstuvwxyz0123456789";
		KString sEncrypted;
		KString sDecrypted;

		CHECK ( Enc.SetOutput(sEncrypted) );
		CHECK ( Enc.Add(sOrig)     );
		CHECK ( Enc.Add("abcdefg") );
		sOrig += "abcdefg";
		CHECK ( Enc.Finalize() );

		KString sStart = sEncrypted.Left(15);
		sEncrypted.remove_prefix(15);

		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.Add(sStart)     );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize()      );

		CHECK ( sOrig == sDecrypted );

		sOrig = "01abcdefghijklmnopqrstuvwxyz0123456789";
		sEncrypted.clear();
		sDecrypted.clear();

		CHECK ( Enc.SetOutput(sEncrypted) );
		CHECK ( Enc.Add(sOrig)     );
		CHECK ( Enc.Add("01abcdefg") );
		sOrig += "01abcdefg";
		CHECK ( Enc.Finalize() );

		sStart = sEncrypted.Left(15);
		sEncrypted.remove_prefix(15);

		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.Add(sStart)     );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize()      );

		CHECK ( sOrig == sDecrypted );
	}

	SECTION("Incremental IV inline")
	{
		KBlockCipher Enc(
			KBlockCipher::Encrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			true,
			true
		);

		KBlockCipher Dec(
			KBlockCipher::Decrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			true,
			true
		);

		auto sKey = kGetRandom(Enc.GetNeededKeyLength());

		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Dec.SetKey(sKey) );
		CHECK ( Enc.SetAutoIncrementNonceAsIV() );
		CHECK ( Dec.SetAutoIncrementNonceAsIV() );

		KString sOrig = "abcdefghijklmnopqrstuvwxyz0123456789";
		KString sEncrypted;
		KString sDecrypted;

		CHECK ( Enc.SetOutput(sEncrypted) );
		CHECK ( Enc.Add(sOrig)     );
		CHECK ( Enc.Add("abcdefg") );
		sOrig += "abcdefg";
		CHECK ( Enc.Finalize() );

		KString sStart = sEncrypted.Left(15);
		sEncrypted.remove_prefix(15);

		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.Add(sStart)     );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize()      );

		CHECK ( sOrig == sDecrypted );
	}

	SECTION("Incremental IV no inline tag")
	{
		KBlockCipher Enc(
			KBlockCipher::Encrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			false
		);

		KBlockCipher Dec(
			KBlockCipher::Decrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			false
		);

		auto sKey = kGetRandom(Enc.GetNeededKeyLength());

		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Dec.SetKey(sKey) );
		CHECK ( Enc.SetAutoIncrementNonceAsIV() );
		CHECK ( Dec.SetAutoIncrementNonceAsIV() );

		KString sOrig = "abcdefghijklmnopqrstuvwxyz0123456789";
		KString sEncrypted;
		KString sDecrypted;

		CHECK ( Enc.SetOutput(sEncrypted) );
		CHECK ( Enc.Add(sOrig)     );
		CHECK ( Enc.Add("abcdefg") );
		sOrig += "abcdefg";
		CHECK ( Enc.Finalize() );

		auto sTag = Enc.GetAuthenticationTag();
		auto sIV = Enc.GetInitializationVector();

		KString sStart = sEncrypted.Left(15);
		sEncrypted.remove_prefix(15);

		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.SetAuthenticationTag(sTag) );
		CHECK ( Dec.SetInitializationVector(sIV) );
		CHECK ( Dec.Add(sStart)     );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize()      );

		CHECK ( sOrig == sDecrypted );
	}

	SECTION("CreateKeyFromPasswordHKDF matches KHKDF::DeriveKey")
	{
		auto sViaBlockCipher = KBlockCipher::CreateKeyFromPasswordHKDF(32, "my secret ikm", "my salt", "my info");
		CHECK ( sViaBlockCipher.size() == 32 );

		// must produce the same result as KHKDF::DeriveKey with the same args
		auto sViaKHKDF = KHKDF::DeriveKey("my salt", "my secret ikm", "my info", 32);
		CHECK ( sViaBlockCipher == sViaKHKDF );
	}

	SECTION("CreateKeyFromPasswordHKDF known-answer (RFC 5869 TC1)")
	{
		// RFC 5869 Test Case 1 IKM (22 bytes of 0x0b)
		KString sIKM(22, '\x0b');
		KString sSalt = kUnHex("000102030405060708090a0b0c");
		KString sInfo = kUnHex("f0f1f2f3f4f5f6f7f8f9");

		auto sKey = KBlockCipher::CreateKeyFromPasswordHKDF(42, sIKM, sSalt, sInfo);
		CHECK ( kHex(sKey) == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865" );
	}

	SECTION("CreateKeyFromPasswordHKDF known-answer (RFC 5869 TC3, empty salt)")
	{
		// RFC 5869 Test Case 3: empty salt, empty info
		KString sIKM(22, '\x0b');

		auto sKey = KBlockCipher::CreateKeyFromPasswordHKDF(42, sIKM, "", "");
		CHECK ( kHex(sKey) == "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8" );
	}
}

TEST_CASE("KBlockCipher move construction")
{
	// regression: the move ctor used to omit m_Algorithm/m_Mode (which have no default
	// member initializer), leaving them indeterminate in the moved-to object -> reading
	// them via GetMode()/GetAlgorithm() (as the encrypt/decrypt paths do) was undefined
	// behavior. It also default-constructed the base classes, dropping their state.
	SECTION("a moved-to cipher keeps algorithm/mode and encrypts+decrypts correctly")
	{
		KStringView sPassword = "MySecretPassword";
		KString     sClear    = "this is a secret message for your eyes only";

		KString sEncrypted;
		{
			KBlockCipher Tmp(KBlockCipher::Encrypt, KBlockCipher::AES, KBlockCipher::GCM, KBlockCipher::B256);
			KBlockCipher Enc(std::move(Tmp));   // exercise the move ctor

			CHECK ( Enc.GetAlgorithm() == KBlockCipher::AES );
			CHECK ( Enc.GetMode()      == KBlockCipher::GCM );

			CHECK ( Enc.SetPassword(sPassword) );
			CHECK ( Enc.SetOutput(sEncrypted) );
			CHECK ( Enc.Add(sClear) );
			CHECK ( Enc.Finalize() );
		}
		CHECK_FALSE ( sEncrypted.empty() );

		KString sDecrypted;
		KBlockCipher Dec(KBlockCipher::Decrypt, KBlockCipher::AES, KBlockCipher::GCM, KBlockCipher::B256);
		CHECK ( Dec.SetPassword(sPassword) );
		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize() );
		CHECK ( sDecrypted == sClear );
	}

	SECTION("the throw-on-error flag survives a move (base-class state is transferred)")
	{
		KBlockCipher Tmp(KBlockCipher::Encrypt);
		Tmp.SetThrowOnError(true);
		KBlockCipher Enc(std::move(Tmp));
		CHECK ( Enc.WouldThrowOnError() );
	}

	SECTION("KBlockCipher lives in a std::vector (growth relocates via the noexcept move ctor)")
	{
		// vector growth relocates existing elements with move_if_noexcept(): since the
		// move ctor is noexcept and the copy ctor is deleted, reallocation uses the move
		// CTOR - never the (deleted) move-assignment. So a vector of ciphers works for
		// emplace_back/growth/iteration even though the type is not move-assignable.
		KStringView sPassword = "MySecretPassword";
		KString     sClear    = "secret payload living in a vector";
		KString     sEncrypted;

		{
			std::vector<KBlockCipher> Ciphers;
			for (int i = 0; i < 8; ++i)   // no reserve() -> forces several reallocations
			{
				Ciphers.emplace_back(KBlockCipher::Encrypt, KBlockCipher::AES, KBlockCipher::GCM, KBlockCipher::B256);
			}
			CHECK ( Ciphers.size() == 8 );

			// after all the internal relocations the elements are still valid, usable
			// ciphers (this would break if the move ctor left m_Algorithm/m_Mode
			// indeterminate)
			CHECK ( Ciphers.back().GetAlgorithm() == KBlockCipher::AES );
			CHECK ( Ciphers.back().GetMode()      == KBlockCipher::GCM );
			CHECK ( Ciphers.back().SetPassword(sPassword) );
			CHECK ( Ciphers.back().SetOutput(sEncrypted) );
			CHECK ( Ciphers.back().Add(sClear) );
			CHECK ( Ciphers.back().Finalize() );
		}
		CHECK_FALSE ( sEncrypted.empty() );

		// the ciphertext produced by the vector-resident cipher decrypts correctly
		KString sDecrypted;
		KBlockCipher Dec(KBlockCipher::Decrypt, KBlockCipher::AES, KBlockCipher::GCM, KBlockCipher::B256);
		CHECK ( Dec.SetPassword(sPassword) );
		CHECK ( Dec.SetOutput(sDecrypted) );
		CHECK ( Dec.Add(sEncrypted) );
		CHECK ( Dec.Finalize() );
		CHECK ( sDecrypted == sClear );
	}
}

TEST_CASE("KBlockCipher destructor never throws")
{
	// a destructor is implicitly noexcept, so a thrown exception would call
	// std::terminate(). A GCM decryption finalize fails on a tampered / auth-mismatched
	// ciphertext; with SetThrowOnError(true) that throws - and if the caller relied on
	// the destructor to finalize, the throw used to escape the noexcept destructor.
	KStringView sPassword = "MySecretPassword";

	KString sCipher;
	{
		KToAES Enc;                       // AES-256-GCM, inline IV + tag
		Enc.SetPassword(sPassword);
		Enc.SetOutput(sCipher);
		Enc.Add("top secret payload");
		REQUIRE ( Enc.Finalize() );
	}
	REQUIRE_FALSE ( sCipher.empty() );

	// corrupt the encrypted payload (the inline tag sits at the FRONT, so flipping the
	// last byte hits ciphertext) so that GCM tag verification fails at finalize
	sCipher.back() = static_cast<char>(sCipher.back() ^ 0xFF);

	SECTION("relying on the destructor to finalize a failed decrypt does not terminate")
	{
		{
			KString  sOut;
			KFromAES Dec;
			Dec.SetThrowOnError(true);
			Dec.SetPassword(sPassword);
			Dec.SetOutput(sOut);
			Dec.Add(sCipher);
			// deliberately NOT calling Finalize(): the destructor must finalize without
			// letting the resulting error throw out of the noexcept destructor
		}
		// reaching this line proves the destructor swallowed the error instead of
		// calling std::terminate()
		CHECK ( true );
	}

	SECTION("an explicit Finalize() still reports the failure (throw contract intact)")
	{
		KString  sOut;
		KFromAES Dec;
		Dec.SetThrowOnError(true);
		Dec.SetPassword(sPassword);
		Dec.SetOutput(sOut);
		Dec.Add(sCipher);
		CHECK_THROWS ( Dec.Finalize() );
	}

	SECTION("without throw-on-error a failed decrypt just returns false")
	{
		KString  sOut;
		KFromAES Dec;
		Dec.SetPassword(sPassword);
		Dec.SetOutput(sOut);
		Dec.Add(sCipher);
		CHECK_FALSE ( Dec.Finalize() );
	}
}

#endif
