#include "catch.hpp"
#include <dekaf2/kaes.h>
#include <vector>

#if DEKAF2_HAS_AES

using namespace dekaf2;

TEST_CASE("KAES")
{
#if DEKAF2_AES_WITH_ECB

	SECTION("AES ECB 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(sEncrypted, KAES::ECB, KAES::B128);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::ECB, KAES::B128);
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
		KToAES Encryptor(sEncrypted, KAES::ECB, KAES::B256);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::ECB, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

#endif

	SECTION("AES CBC 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(sEncrypted, KAES::CBC, KAES::B128);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CBC, KAES::B128);
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
		KToAES Encryptor(sEncrypted, KAES::CBC, KAES::B192);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CBC, KAES::B192);
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
		KToAES Encryptor(sEncrypted, KAES::CBC, KAES::B256);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CBC, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}

#if DEKAF2_AES_WITH_CCM
	SECTION("AES CCM 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(sEncrypted, KAES::CCM, KAES::B128);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CCM, KAES::B128);
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
		KToAES Encryptor(sEncrypted, KAES::CCM, KAES::B192);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CCM, KAES::B192);
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
		KToAES Encryptor(sEncrypted, KAES::CCM, KAES::B256);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CCM, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		CHECK ( Decryptor.Finalize() );

		CHECK ( sDecrypted == sClearText );
	}
#endif

	SECTION("AES GCM 128")
	{
		KString sClearText = "this is a secret message for your eyes only";
		KStringView sPassword = "MySecretPassword";

		KString sEncrypted;
		KToAES Encryptor(sEncrypted, KAES::GCM, KAES::B128);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::GCM, KAES::B128);
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
		KToAES Encryptor(sEncrypted, KAES::GCM, KAES::B192);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::GCM, KAES::B192);
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
		KToAES Encryptor(sEncrypted, KAES::GCM, KAES::B256);
		Encryptor.SetPassword(sPassword);
		Encryptor(sClearText);
		CHECK ( Encryptor.Finalize() );

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::GCM, KAES::B256);
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
			KToAES Encryptor(sEncrypted1, KAES::GCM, KAES::B256);
			Encryptor.SetPassword("MySecretPassword");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted2;
		{
			KToAES Encryptor(sEncrypted2, KAES::GCM, KAES::B256);
			Encryptor.SetPassword("MyOtherPassword");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted3;
		{
			KToAES Encryptor(sEncrypted3, KAES::GCM, KAES::B256);
			Encryptor.SetPassword("MySecretPassword", "salt1");
			Encryptor(sClearText);
			CHECK ( Encryptor.Finalize() );
		}

		KString sEncrypted4;
		{
			KToAES Encryptor(sEncrypted4, KAES::GCM, KAES::B256);
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

}

#endif
