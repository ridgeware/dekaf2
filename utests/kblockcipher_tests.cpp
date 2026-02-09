#include "catch.hpp"
#include <dekaf2/kaes.h>
#include <dekaf2/ksystem.h>
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
}

#endif
