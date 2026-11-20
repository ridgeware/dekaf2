#include "catch.hpp"
#include <dekaf2/crypto/cipher/kaes.h>
#include <dekaf2/crypto/kdf/khkdf.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <sstream>
#include <dekaf2/core/format/kformat.h>
#include <memory>
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

	SECTION("a move in the middle of an encryption leaves the output to the moved-to cipher")
	{
		KStringView sPassword = "MySecretPassword";
		KString     sClear    = "this is a secret message for your eyes only";

		KString sEncrypted;
		{
			std::unique_ptr<KBlockCipher> Enc;
			{
				KBlockCipher Tmp(KBlockCipher::Encrypt, KBlockCipher::AES, KBlockCipher::GCM, KBlockCipher::B256);
				CHECK ( Tmp.SetPassword(sPassword) );
				CHECK ( Tmp.SetOutput(sEncrypted) );
				CHECK ( Tmp.Add(sClear) );
				Enc = std::make_unique<KBlockCipher>(std::move(Tmp));
				// the destruction of the moved-from cipher must not finalize into the output
			}
			CHECK ( Enc->Finalize() );
		}

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

TEST_CASE("KBlockCipher empty text")
{
	// A text of no bytes is a text: an authenticating mode has a tag for it, and the tag
	// has to hold. Everything that passes IV and tag on to OpenSSL used to happen with the
	// first input only - so with an empty Add(), or without any, finalizing worked on a
	// cipher that had seen neither.

	const auto Hex = [](KStringView sHex) { return kUnHex(sHex); };

	const auto Cipher = [](KBlockCipher::Direction Direction, KBlockCipher::Mode Mode, KBlockCipher::Bits Bits, bool bInline)
	{
		return KBlockCipher(Direction, KBlockCipher::AES, Mode, Bits, bInline, bInline);
	};

	SECTION("GCM known answers (McGrew/Viega, The Galois/Counter Mode of Operation, test cases 1 and 13)")
	{
		struct Case { KBlockCipher::Bits Bits; std::size_t iKeyLen; KStringView sTag; };

		for (const auto& C : { Case { KBlockCipher::B128, 16, "58e2fccefa7e3061367f1d57a4e7455a" },
		                       Case { KBlockCipher::B256, 32, "530f8afbc74536b9a963b4f1c4cb738b" } })
		{
			const KString sKey (C.iKeyLen, '\0');
			const KString sIV  (12, '\0');

			for (int iHow = 0; iHow < 3; ++iHow)
			{
				INFO ( kFormat("key of {} bytes, variant {}", C.iKeyLen, iHow) );

				KString sOut;
				auto Enc = Cipher(KBlockCipher::Encrypt, KBlockCipher::GCM, C.Bits, false);
				CHECK ( Enc.SetKey(sKey) );
				CHECK ( Enc.SetInitializationVector(sIV) );

				if (iHow == 2)
				{
					CHECK ( Enc.SingleRound("", sOut) );
				}
				else
				{
					CHECK ( Enc.SetOutput(sOut) );
					if (iHow == 1) CHECK ( Enc.Add("") );   // iHow == 0: no Add() at all
					CHECK ( Enc.Finalize() );
				}

				CHECK ( sOut.empty() );
				CHECK ( kHex(Enc.GetAuthenticationTag()) == C.sTag );
				CHECK ( Enc.GetInitializationVector() == sIV );

				// and back
				KString sClear { "untouched" };
				sClear.clear();
				auto Dec = Cipher(KBlockCipher::Decrypt, KBlockCipher::GCM, C.Bits, false);
				CHECK ( Dec.SetKey(sKey) );
				CHECK ( Dec.SetInitializationVector(sIV) );
				CHECK ( Dec.SetAuthenticationTag(Hex(C.sTag)) );

				if (iHow == 2)
				{
					CHECK ( Dec.SingleRound("", sClear) );
				}
				else
				{
					CHECK ( Dec.SetOutput(sClear) );
					if (iHow == 1) CHECK ( Dec.Add("") );
					CHECK ( Dec.Finalize() );
				}

				CHECK ( sClear.empty() );
			}
		}
	}

	SECTION("the tag of an empty text is checked")
	{
		for (auto Mode : { KBlockCipher::GCM, KBlockCipher::CCM })
		{
			INFO ( kFormat("mode {}", Mode == KBlockCipher::GCM ? "GCM" : "CCM") );

			auto Enc = Cipher(KBlockCipher::Encrypt, Mode, KBlockCipher::B256, false);
			const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
			const auto sIV  = kGetRandom(Enc.GetNeededIVLength());

			KString sOut;
			CHECK ( Enc.SetKey(sKey) );
			CHECK ( Enc.SetInitializationVector(sIV) );
			CHECK ( Enc.SetOutput(sOut) );
			CHECK ( Enc.Finalize() );
			CHECK ( sOut.empty() );

			const KString sTag = Enc.GetAuthenticationTag();
			CHECK ( sTag.size() == (Mode == KBlockCipher::GCM ? 16u : 12u) );

			// the same key and IV make the same tag, every time
			{
				KString sOut2;
				auto Enc2 = Cipher(KBlockCipher::Encrypt, Mode, KBlockCipher::B256, false);
				CHECK ( Enc2.SetKey(sKey) );
				CHECK ( Enc2.SetInitializationVector(sIV) );
				CHECK ( Enc2.SetOutput(sOut2) );
				CHECK ( Enc2.Add("") );
				CHECK ( Enc2.Finalize() );
				CHECK ( Enc2.GetAuthenticationTag() == sTag );
			}

			// the right tag
			{
				KString sClear;
				auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, false);
				CHECK ( Dec.SetKey(sKey) );
				CHECK ( Dec.SetInitializationVector(sIV) );
				CHECK ( Dec.SetAuthenticationTag(sTag) );
				CHECK ( Dec.SetOutput(sClear) );
				CHECK ( Dec.Finalize() );
				CHECK ( sClear.empty() );
			}

			// a wrong tag
			{
				KString sWrong = sTag;
				sWrong.back() = static_cast<char>(sWrong.back() ^ 0x01);

				KString sClear;
				auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, false);
				CHECK ( Dec.SetKey(sKey) );
				CHECK ( Dec.SetInitializationVector(sIV) );
				CHECK ( Dec.SetAuthenticationTag(sWrong) );
				CHECK ( Dec.SetOutput(sClear) );
				CHECK_FALSE ( Dec.Finalize() );
				CHECK ( sClear.empty() );
			}

			// another key
			{
				KString sClear;
				auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, false);
				CHECK ( Dec.SetKey(kGetRandom(Dec.GetNeededKeyLength())) );
				CHECK ( Dec.SetInitializationVector(sIV) );
				CHECK ( Dec.SetAuthenticationTag(sTag) );
				CHECK ( Dec.SetOutput(sClear) );
				CHECK_FALSE ( Dec.Finalize() );
			}

			// no tag at all
			{
				KString sClear;
				auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, false);
				CHECK ( Dec.SetKey(sKey) );
				CHECK ( Dec.SetInitializationVector(sIV) );
				CHECK ( Dec.SetOutput(sClear) );
				CHECK_FALSE ( Dec.Finalize() );
			}
		}
	}

	SECTION("inline IV and tag: the ciphertext of an empty text is tag and IV, and decrypts")
	{
		for (auto Mode : { KBlockCipher::GCM, KBlockCipher::CCM })
		{
			for (int iHow = 0; iHow < 3; ++iHow)
			{
				INFO ( kFormat("mode {}, variant {}", Mode == KBlockCipher::GCM ? "GCM" : "CCM", iHow) );

				auto Enc = Cipher(KBlockCipher::Encrypt, Mode, KBlockCipher::B256, true);
				const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
				CHECK ( Enc.SetKey(sKey) );

				// something is in the output already: the ciphertext is appended
				KString sOut { "head" };

				if (iHow == 2)
				{
					CHECK ( Enc.SingleRound("", sOut) );
				}
				else
				{
					CHECK ( Enc.SetOutput(sOut) );
					if (iHow == 1) CHECK ( Enc.Add("") );
					CHECK ( Enc.Finalize() );
				}

				const std::size_t iTag = Enc.GetAuthenticationTag().size();
				const std::size_t iIV  = Enc.GetNeededIVLength();

				REQUIRE ( iTag >= 12 );
				REQUIRE ( sOut.size() == 4 + iTag + iIV );
				CHECK   ( sOut.starts_with("head") );
				CHECK   ( sOut.substr(4, iTag)    == Enc.GetAuthenticationTag()    );
				CHECK   ( sOut.substr(4 + iTag)   == Enc.GetInitializationVector() );
				CHECK   ( Enc.GetAuthenticationTag() != KString(iTag, '\0') );

				const KString sCipher = sOut.substr(4);

				{
					KString sClear;
					auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
					CHECK ( Dec.SetKey(sKey) );
					CHECK ( Dec.SingleRound(sCipher, sClear) );
					CHECK ( sClear.empty() );
				}

				// one bit of the tag, and one of the IV
				for (std::size_t iPos : { std::size_t(0), iTag })
				{
					KString sBad = sCipher;
					sBad[iPos] = static_cast<char>(sBad[iPos] ^ 0x80);

					KString sClear;
					auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
					CHECK ( Dec.SetKey(sKey) );
					CHECK_FALSE ( Dec.SingleRound(sBad, sClear) );
					CHECK ( sClear.empty() );
				}

				// cut off inside the tag, inside the IV, and nothing at all
				for (std::size_t iLen : { std::size_t(0), iTag / 2, iTag, iTag + iIV - 1 })
				{
					KString sClear;
					auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
					CHECK ( Dec.SetKey(sKey) );
					CHECK_FALSE ( Dec.SingleRound(sCipher.substr(0, iLen), sClear) );
					CHECK ( sClear.empty() );
				}
			}
		}
	}

	SECTION("a cipher is good for the next round after an empty one, and for an empty one after a full")
	{
		KToAES   Enc;
		KFromAES Dec;
		CHECK ( Enc.SetPassword("MySecretPassword") );
		CHECK ( Dec.SetPassword("MySecretPassword") );

		for (KStringView sText : { "", "first", "", "", "a longer text of more than one block of sixteen bytes", "" })
		{
			KString sCipher;
			KString sClear;
			CHECK ( Enc.SingleRound(sText, sCipher) );
			CHECK ( sCipher.size() == sText.size() + 16 + Enc.GetNeededIVLength() );
			CHECK ( Dec.SingleRound(sCipher, sClear) );
			CHECK ( sClear == sText );
		}
	}

	SECTION("ChaCha20-Poly1305")
	{
		KBlockCipher Enc(KBlockCipher::Encrypt, KBlockCipher::ChaCha20_Poly1305);
		KBlockCipher Dec(KBlockCipher::Decrypt, KBlockCipher::ChaCha20_Poly1305);
		const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Dec.SetKey(sKey) );

		for (KStringView sText : { "hello", "", "a longer text of more than one block of sixteen bytes" })
		{
			INFO ( kFormat("text of {} bytes", sText.size()) );

			KString sCipher;
			KString sClear;
			CHECK ( Enc.SingleRound(sText, sCipher) );
			// a Poly1305 tag of 16 bytes, the nonce, the text
			CHECK ( Enc.GetAuthenticationTag().size() == 16 );
			CHECK ( sCipher.size() == 16u + Enc.GetNeededIVLength() + sText.size() );
			CHECK ( sCipher.substr(0, 16) == Enc.GetAuthenticationTag() );
			CHECK ( Dec.SingleRound(sCipher, sClear) );
			CHECK ( sClear == sText );

			// one bit of the tag, and one of the last byte (nonce or text)
			for (std::size_t iPos : { std::size_t(0), sCipher.size() - 1 })
			{
				KString sBad = sCipher;
				sBad[iPos] = static_cast<char>(sBad[iPos] ^ 0x01);
				KString sNot;
				// (a cipher of its own: one that has failed stays failed)
				KBlockCipher Dec2(KBlockCipher::Decrypt, KBlockCipher::ChaCha20_Poly1305);
				CHECK ( Dec2.SetKey(sKey) );
				CHECK_FALSE ( Dec2.SingleRound(sBad, sNot) );
				CHECK ( sNot.empty() );
			}
		}
	}

	SECTION("ChaCha20-Poly1305 known answer (RFC 8439, 2.8.2)")
	{
		// the RFC's AEAD example, with its additional data
		KBlockCipher Enc(KBlockCipher::Encrypt, KBlockCipher::ChaCha20_Poly1305, KBlockCipher::GCM, KBlockCipher::B256, false, false);
		const KString sKey = kUnHex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
		const KString sIV  = kUnHex("070000004041424344454647");
		const KString sAAD = kUnHex("50515253c0c1c2c3c4c5c6c7");
		const KStringView sText = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";

		KString sCipher;
		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Enc.SetInitializationVector(sIV) );
		CHECK ( Enc.SetAuthenticatedData(sAAD) );
		CHECK ( Enc.SingleRound(sText, sCipher) );
		CHECK ( kHex(Enc.GetAuthenticationTag()) == "1ae10b594f09e26a7e902ecbd0600691" );
		// the ciphertext does not depend on the additional data, the tag does
		CHECK ( kHex(sCipher) ==
			"d31a8d34648e60db7b86afbc53ef7ec2"
			"a4aded51296e08fea9e2b5a736ee62d6"
			"3dbea45e8ca9671282fafb69da92728b"
			"1a71de0a9e060b2905d6a5b67ecd3b36"
			"92ddbd7f2d778b8c9803aee328091b58"
			"fab324e4fad675945585808b4831d7bc"
			"3ff4def08e4b7a9de576d26586cec64b"
			"6116" );
		CHECK ( Enc.GetAuthenticationTag().size() == 16 );
	}

	SECTION("to a stream")
	{
		for (KStringView sText : { "", "a text that goes to a stream" })
		{
			INFO ( kFormat("text of {} bytes", sText.size()) );

			KToAES Enc;
			CHECK ( Enc.SetPassword("MySecretPassword") );

			KString sCipher;
			{
				// (a stream that can be repositioned: the tag is written to the front at the end)
				std::ostringstream oss;
				KOutStream Out(oss);
				CHECK ( Enc.SetOutput(Out) );
				if (!sText.empty()) CHECK ( Enc.Add(sText) );
				CHECK ( Enc.Finalize() );
				sCipher = oss.str();
			}

			CHECK ( sCipher.size() == 16u + Enc.GetNeededIVLength() + sText.size() );
			CHECK ( Enc.GetAuthenticationTag().size() == 16 );
			CHECK ( sCipher.substr(0, 16) == Enc.GetAuthenticationTag() );

			KString  sClear;
			KFromAES Dec;
			CHECK ( Dec.SetPassword("MySecretPassword") );
			CHECK ( Dec.SingleRound(sCipher, sClear) );
			CHECK ( sClear == sText );
		}
	}

	SECTION("the destructor finalizes an empty text, too")
	{
		KString sCipher;
		std::size_t iExpected { 0 };
		{
			KToAES Enc;
			CHECK ( Enc.SetPassword("MySecretPassword") );
			CHECK ( Enc.SetOutput(sCipher) );
			iExpected = 16u + Enc.GetNeededIVLength();
		}
		CHECK ( sCipher.size() == iExpected );

		KString  sClear;
		KFromAES Dec;
		CHECK ( Dec.SetPassword("MySecretPassword") );
		CHECK ( Dec.SingleRound(sCipher, sClear) );
		CHECK ( sClear.empty() );
	}

	SECTION("modes without a tag")
	{
		// CBC and ECB pad: no text is one block of padding
		for (auto Mode : { KBlockCipher::CBC, KBlockCipher::ECB })
		{
			INFO ( kFormat("mode {}", Mode == KBlockCipher::CBC ? "CBC" : "ECB") );

			auto Enc = Cipher(KBlockCipher::Encrypt, Mode, KBlockCipher::B256, true);
			auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
			const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
			CHECK ( Enc.SetKey(sKey) );
			CHECK ( Dec.SetKey(sKey) );

			KString sCipher;
			KString sClear;
			CHECK ( Enc.SingleRound("", sCipher) );
			CHECK ( sCipher.size() == Enc.GetNeededIVLength() + 16 );
			if (Enc.GetNeededIVLength())
			{
				// the IV in the output is the one that was used
				CHECK ( sCipher.substr(0, Enc.GetNeededIVLength()) == Enc.GetInitializationVector() );
			}
			CHECK ( Dec.SingleRound(sCipher, sClear) );
			CHECK ( sClear.empty() );

			// no input is not a ciphertext for them: not even the padding is there
			KString sNothing;
			CHECK_FALSE ( Dec.SingleRound("", sNothing) );
		}
	}

	SECTION("several rounds with one object")
	{
		// Every round has to start from a clean state, whatever the round before left
		// behind. The reference is a new object for every text.
		for (auto Mode : { KBlockCipher::ECB, KBlockCipher::CBC, KBlockCipher::GCM })
		{
			INFO ( kFormat("mode {}", Mode == KBlockCipher::ECB ? "ECB" : Mode == KBlockCipher::CBC ? "CBC" : "GCM") );

			auto Enc = Cipher(KBlockCipher::Encrypt, Mode, KBlockCipher::B256, true);
			auto Dec = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
			const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
			CHECK ( Enc.SetKey(sKey) );
			CHECK ( Dec.SetKey(sKey) );

			// texts that end inside a block, on a block boundary, and no text at all
			for (KStringView sText : { "first text", "0123456789abcdef", "", "second text, longer than one block" })
			{
				INFO ( sText );

				KString sCipher;
				KString sClear;
				CHECK ( Enc.SingleRound(sText, sCipher) );

				// a new object reads what the reused one wrote
				auto Fresh = Cipher(KBlockCipher::Decrypt, Mode, KBlockCipher::B256, true);
				CHECK ( Fresh.SetKey(sKey) );
				KString sFresh;
				CHECK ( Fresh.SingleRound(sCipher, sFresh) );
				CHECK ( sFresh == sText );

				// and so does the reused one
				CHECK ( Dec.SingleRound(sCipher, sClear) );
				CHECK ( sClear == sText );
			}
		}
	}

	SECTION("CBC with a given IV: known answer")
	{
		// AES-128-CBC of the empty text is the encryption of one block of padding (16 times
		// 0x10) xored with the IV. With key and IV zero that is AES-128-ECB of that block
		// under the zero key - which the ECB mode of this same class computes, from input.
		const KString sKey (16, '\0');
		const KString sIV  (16, '\0');

		KString sExpected;
		{
			KString sBoth;
			auto Ecb = Cipher(KBlockCipher::Encrypt, KBlockCipher::ECB, KBlockCipher::B128, false);
			CHECK ( Ecb.SetKey(sKey) );
			CHECK ( Ecb.SingleRound(KString(16, '\x10'), sBoth) );
			REQUIRE ( sBoth.size() == 32 );
			sExpected = sBoth.substr(0, 16);
		}

		KString sCipher;
		auto Enc = Cipher(KBlockCipher::Encrypt, KBlockCipher::CBC, KBlockCipher::B128, false);
		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Enc.SetInitializationVector(sIV) );
		CHECK ( Enc.SetOutput(sCipher) );
		CHECK ( Enc.Finalize() );
		CHECK ( kHex(sCipher) == kHex(sExpected) );

		// and with another IV another ciphertext: the IV is in it
		KString sCipher2;
		auto Enc2 = Cipher(KBlockCipher::Encrypt, KBlockCipher::CBC, KBlockCipher::B128, false);
		CHECK ( Enc2.SetKey(sKey) );
		CHECK ( Enc2.SetInitializationVector(KString(16, '\x01')) );
		CHECK ( Enc2.SingleRound("", sCipher2) );
		CHECK ( sCipher2.size() == 16 );
		CHECK ( sCipher2 != sCipher );

		KString sClear;
		auto Dec = Cipher(KBlockCipher::Decrypt, KBlockCipher::CBC, KBlockCipher::B128, false);
		CHECK ( Dec.SetKey(sKey) );
		CHECK ( Dec.SetInitializationVector(KString(16, '\x01')) );
		CHECK ( Dec.SingleRound(sCipher2, sClear) );
		CHECK ( sClear.empty() );
	}
}

TEST_CASE("KBlockCipher failed decryption")
{
	SECTION("a ciphertext with a wrong tag leaves no output")
	{
		struct Kind { KBlockCipher::Algorithm Algorithm; KBlockCipher::Mode Mode; KStringView sName; };

		for (const auto& K : { Kind { KBlockCipher::AES,               KBlockCipher::GCM, "AES-256-GCM"       },
		                       Kind { KBlockCipher::AES,               KBlockCipher::CCM, "AES-256-CCM"       },
		                       Kind { KBlockCipher::ChaCha20_Poly1305, KBlockCipher::GCM, "ChaCha20-Poly1305" } })
		{
			INFO ( K.sName );

			KBlockCipher Enc(KBlockCipher::Encrypt, K.Algorithm, K.Mode);
			const auto sKey = kGetRandom(Enc.GetNeededKeyLength());

			KString sCipher;
			CHECK ( Enc.SetKey(sKey) );
			CHECK ( Enc.SingleRound("hello world", sCipher) );

			// one bit of the tag
			sCipher[0] = static_cast<char>(sCipher[0] ^ 0x01);

			// to an empty string, and behind what a string holds already
			for (KStringView sBefore : { "", "before" })
			{
				KString sClear(sBefore);
				KBlockCipher Dec(KBlockCipher::Decrypt, K.Algorithm, K.Mode);
				CHECK ( Dec.SetKey(sKey) );
				CHECK_FALSE ( Dec.SingleRound(sCipher, sClear) );
				CHECK ( sClear == sBefore );
			}
		}
	}
}

TEST_CASE("KBlockCipher authenticated data")
{
	// additional data that is authenticated with the text, but not part of the ciphertext

	struct Kind { KBlockCipher::Algorithm Algorithm; KBlockCipher::Mode Mode; KBlockCipher::Bits Bits; KStringView sName; };

	static constexpr Kind Kinds[] =
	{
		{ KBlockCipher::AES,               KBlockCipher::GCM, KBlockCipher::B128, "AES-128-GCM"       },
		{ KBlockCipher::AES,               KBlockCipher::GCM, KBlockCipher::B256, "AES-256-GCM"       },
		{ KBlockCipher::AES,               KBlockCipher::CCM, KBlockCipher::B128, "AES-128-CCM"       },
		{ KBlockCipher::AES,               KBlockCipher::CCM, KBlockCipher::B256, "AES-256-CCM"       },
		{ KBlockCipher::ChaCha20_Poly1305, KBlockCipher::GCM, KBlockCipher::B256, "ChaCha20-Poly1305" }
	};

	static constexpr KStringView Texts[] = { "", "hello", "a longer text of more than one block of sixteen bytes" };

	SECTION("GCM known answers (McGrew/Viega, The Galois/Counter Mode of Operation, test cases 4 and 16)")
	{
		const KString sText = kUnHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
		                             "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39");
		const KString sAAD  = kUnHex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
		const KString sIV   = kUnHex("cafebabefacedbaddecaf888");

		struct Case { KBlockCipher::Bits Bits; KStringView sKey; KStringView sCipher; KStringView sTag; };

		for (const auto& C : { Case { KBlockCipher::B128,
		                              "feffe9928665731c6d6a8f9467308308",
		                              "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e"
		                              "21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
		                              "5bc94fbc3221a5db94fae95ae7121a47" },
		                       Case { KBlockCipher::B256,
		                              "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
		                              "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa"
		                              "8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662",
		                              "76fc6ece0f4e1768cddf8853bb2d551b" } })
		{
			INFO ( kFormat("key of {} bytes", C.sKey.size() / 2) );

			KString sCipher;
			KBlockCipher Enc(KBlockCipher::Encrypt, KBlockCipher::AES, KBlockCipher::GCM, C.Bits, false, false);
			CHECK ( Enc.SetKey(kUnHex(C.sKey)) );
			CHECK ( Enc.SetInitializationVector(sIV) );
			CHECK ( Enc.SetAuthenticatedData(sAAD) );
			CHECK ( Enc.SingleRound(sText, sCipher) );
			CHECK ( kHex(sCipher) == C.sCipher );
			CHECK ( kHex(Enc.GetAuthenticationTag()) == C.sTag );

			// and back
			KString sClear;
			KBlockCipher Dec(KBlockCipher::Decrypt, KBlockCipher::AES, KBlockCipher::GCM, C.Bits, false, false);
			CHECK ( Dec.SetKey(kUnHex(C.sKey)) );
			CHECK ( Dec.SetInitializationVector(sIV) );
			CHECK ( Dec.SetAuthenticationTag(kUnHex(C.sTag)) );
			CHECK ( Dec.SetAuthenticatedData(sAAD) );
			CHECK ( Dec.SingleRound(kUnHex(C.sCipher), sClear) );
			CHECK ( sClear == sText );
		}
	}

	SECTION("decryption needs the same data")
	{
		for (const auto& K : Kinds)
		{
			for (bool bInline : { true, false })
			{
				for (KStringView sText : Texts)
				{
					INFO ( kFormat("{}, {}, text of {} bytes", K.sName, bInline ? "inline" : "not inline", sText.size()) );

					KBlockCipher Enc(KBlockCipher::Encrypt, K.Algorithm, K.Mode, K.Bits, bInline, bInline);
					const auto sKey = kGetRandom(Enc.GetNeededKeyLength());

					KString sCipher;
					CHECK ( Enc.SetKey(sKey) );
					CHECK ( Enc.SetAuthenticatedData("alice") );
					CHECK ( Enc.SingleRound(sText, sCipher) );

					const KString sIV  = Enc.GetInitializationVector();
					const KString sTag = Enc.GetAuthenticationTag();

					// the data is not part of the output
					CHECK ( sCipher.size() == sText.size() + (bInline ? sTag.size() + sIV.size() : 0) );
					CHECK_FALSE ( sCipher.contains("alice") );

					// what was authenticated with the text: right, wrong, longer, none
					for (KStringView sData : { "alice", "alicf", "alice ", "" })
					{
						INFO ( kFormat("data '{}'", sData) );

						// (a cipher of its own: one that has failed stays failed)
						KBlockCipher Dec(KBlockCipher::Decrypt, K.Algorithm, K.Mode, K.Bits, bInline, bInline);
						CHECK ( Dec.SetKey(sKey) );

						if (!bInline)
						{
							CHECK ( Dec.SetInitializationVector(sIV) );
							CHECK ( Dec.SetAuthenticationTag(sTag) );
						}

						CHECK ( Dec.SetAuthenticatedData(sData) );

						KString sClear;

						if (sData == "alice")
						{
							CHECK ( Dec.SingleRound(sCipher, sClear) );
							CHECK ( sClear == sText );
						}
						else
						{
							CHECK_FALSE ( Dec.SingleRound(sCipher, sClear) );
							CHECK ( sClear.empty() );
						}
					}

					// a text without data does not pass for one with
					KString sPlainCipher;
					KBlockCipher Enc2(KBlockCipher::Encrypt, K.Algorithm, K.Mode, K.Bits, true, true);
					CHECK ( Enc2.SetKey(sKey) );
					CHECK ( Enc2.SingleRound(sText, sPlainCipher) );

					KString sNot;
					KBlockCipher Dec2(KBlockCipher::Decrypt, K.Algorithm, K.Mode, K.Bits, true, true);
					CHECK ( Dec2.SetKey(sKey) );
					CHECK ( Dec2.SetAuthenticatedData("alice") );
					CHECK_FALSE ( Dec2.SingleRound(sPlainCipher, sNot) );
					CHECK ( sNot.empty() );
				}
			}
		}
	}

	SECTION("the data counts for one round")
	{
		for (const auto& K : Kinds)
		{
			INFO ( K.sName );

			KBlockCipher Enc(KBlockCipher::Encrypt, K.Algorithm, K.Mode, K.Bits);
			KBlockCipher Dec(KBlockCipher::Decrypt, K.Algorithm, K.Mode, K.Bits);
			const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
			CHECK ( Enc.SetKey(sKey) );
			CHECK ( Dec.SetKey(sKey) );

			KString sFirst;
			KString sSecond;
			KString sThird;
			CHECK ( Enc.SetAuthenticatedData("first") );
			CHECK ( Enc.SingleRound("one", sFirst) );
			CHECK ( Enc.SingleRound("two", sSecond) );
			CHECK ( Enc.SetAuthenticatedData("third") );
			CHECK ( Enc.SingleRound("three", sThird) );

			KString sClear;
			CHECK ( Dec.SetAuthenticatedData("first") );
			CHECK ( Dec.SingleRound(sFirst, sClear) );
			CHECK ( sClear == "one" );

			// the second round had no data
			sClear.clear();
			CHECK ( Dec.SingleRound(sSecond, sClear) );
			CHECK ( sClear == "two" );

			sClear.clear();
			CHECK ( Dec.SetAuthenticatedData("third") );
			CHECK ( Dec.SingleRound(sThird, sClear) );
			CHECK ( sClear == "three" );
		}
	}

	SECTION("the ciphertext may come in pieces")
	{
		// the inlined tag and IV arrive over several calls, the data is due behind the IV
		KBlockCipher Enc(KBlockCipher::Encrypt);
		const auto sKey = kGetRandom(Enc.GetNeededKeyLength());
		const KStringView sText = "a longer text of more than one block of sixteen bytes";

		KString sCipher;
		CHECK ( Enc.SetKey(sKey) );
		CHECK ( Enc.SetAuthenticatedData("alice") );
		CHECK ( Enc.SetOutput(sCipher) );
		CHECK ( Enc.Add(sText.substr(0, 7)) );
		CHECK ( Enc.Add(sText.substr(7)) );
		CHECK ( Enc.Finalize() );

		for (KStringView sData : { "alice", "bob" })
		{
			INFO ( kFormat("data '{}'", sData) );

			KString sClear;
			KBlockCipher Dec(KBlockCipher::Decrypt);
			CHECK ( Dec.SetKey(sKey) );
			CHECK ( Dec.SetAuthenticatedData(sData) );
			CHECK ( Dec.SetOutput(sClear) );

			for (std::size_t iPos = 0; iPos < sCipher.size(); iPos += 5)
			{
				CHECK ( Dec.Add(KStringView(sCipher).substr(iPos, 5)) );
			}

			if (sData == "alice")
			{
				CHECK ( Dec.Finalize() );
				CHECK ( sClear == sText );
			}
			else
			{
				CHECK_FALSE ( Dec.Finalize() );
				CHECK ( sClear.empty() );
			}
		}
	}

	SECTION("to a stream")
	{
		for (KStringView sText : Texts)
		{
			INFO ( kFormat("text of {} bytes", sText.size()) );

			KToAES Enc;
			CHECK ( Enc.SetPassword("MySecretPassword") );
			CHECK ( Enc.SetAuthenticatedData("alice") );

			KString sCipher;
			{
				std::ostringstream oss;
				KOutStream Out(oss);
				CHECK ( Enc.SetOutput(Out) );
				if (!sText.empty()) CHECK ( Enc.Add(sText) );
				CHECK ( Enc.Finalize() );
				sCipher = oss.str();
			}

			KString  sClear;
			KFromAES Dec;
			CHECK ( Dec.SetPassword("MySecretPassword") );
			CHECK ( Dec.SetAuthenticatedData("alice") );
			CHECK ( Dec.SingleRound(sCipher, sClear) );
			CHECK ( sClear == sText );

			KString  sNot;
			KFromAES Dec2;
			CHECK ( Dec2.SetPassword("MySecretPassword") );
			CHECK_FALSE ( Dec2.SingleRound(sCipher, sNot) );
			CHECK ( sNot.empty() );
		}
	}

	SECTION("only with a tag, and before the text")
	{
		// a mode without a tag cannot authenticate anything
		for (auto Mode : { KBlockCipher::CBC, KBlockCipher::ECB, KBlockCipher::CTR })
		{
			KBlockCipher Enc(KBlockCipher::Encrypt, KBlockCipher::AES, Mode);
			CHECK_FALSE ( Enc.SetAuthenticatedData("alice") );
			CHECK ( Enc.HasError() );
		}

		// once the text has begun, the data comes too late
		KString sCipher;
		KBlockCipher Enc(KBlockCipher::Encrypt);
		CHECK ( Enc.SetPassword("MySecretPassword") );
		CHECK ( Enc.SetOutput(sCipher) );
		CHECK ( Enc.SetAuthenticatedData("alice") );
		CHECK ( Enc.SetAuthenticatedData("bob") ); // replaces the former
		CHECK ( Enc.Add("hello") );
		CHECK_FALSE ( Enc.SetAuthenticatedData("carol") );
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
