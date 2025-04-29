#include "catch.hpp"
#include <dekaf2/kaes.h>
#include <vector>

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::ECB, KAES::B128);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::ECB, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CBC, KAES::B128);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CBC, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CCM, KAES::B128);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::CCM, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::GCM, KAES::B128);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

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
		Encryptor.Finalize();

		KString sDecrypted;
		KFromAES Decryptor(sDecrypted, KAES::GCM, KAES::B256);
		Decryptor.SetPassword(sPassword);
		Decryptor(sEncrypted);
		Decryptor.Finalize();

		CHECK ( sDecrypted == sClearText );
	}

}
