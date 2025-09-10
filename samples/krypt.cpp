/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#include <dekaf2/koptions.h>
#include <dekaf2/kerror.h>
#include <dekaf2/kblockcipher.h>
#include <dekaf2/kencode.h>
#include <dekaf2/kcompression.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Krypt : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	KString GetFileContent(KStringViewZ sFilename)
	//-----------------------------------------------------------------------------
	{
		KString sContent;
		if (!kReadAll((sFilename.empty()) ? KIn : KInFile(sFilename), sContent))
		{
			throw KError(kFormat("cannot read file: {}", sFilename));
		}
		return sContent;
	}

	//-----------------------------------------------------------------------------
	KBlockCipher::Algorithm GetAlgorithm(KStringView sAlgorithm)
	//-----------------------------------------------------------------------------
	{
		switch (sAlgorithm.CaseHash())
		{
			case "aes"_casehash:      return KBlockCipher::AES;
			case "aria"_casehash:     return KBlockCipher::ARIA;
			case "camellia"_casehash: return KBlockCipher::Camellia;
			case "chacha"_casehash:   return KBlockCipher::ChaCha20_Poly1305;
		}
		throw KError { kFormat("invalid algorithm: {}", sAlgorithm) };
	}

	//-----------------------------------------------------------------------------
	KBlockCipher::Mode GetMode(KStringView sMode)
	//-----------------------------------------------------------------------------
	{
		switch (sMode.CaseHash())
		{
			case "ecb"_casehash:    return KBlockCipher::ECB;
			case "cbc"_casehash:    return KBlockCipher::CBC;
			case "ofb"_casehash:    return KBlockCipher::OFB;
			case "cfb1"_casehash:   return KBlockCipher::CFB1;
			case "cfb8"_casehash:   return KBlockCipher::CFB8;
			case "cfb128"_casehash: return KBlockCipher::CFB128;
			case "ctr"_casehash:    return KBlockCipher::CTR;
			case "ccm"_casehash:    return KBlockCipher::CCM;
			case "gcm"_casehash:    return KBlockCipher::GCM;
			case "ocb"_casehash:    return KBlockCipher::OCB;
			case "xts"_casehash:    return KBlockCipher::XTS;
		}
		throw KError { kFormat("invalid mode: {}", sMode) };
	}

	//-----------------------------------------------------------------------------
	KBlockCipher::Bits GetBits(KStringView sBits)
	//-----------------------------------------------------------------------------
	{
		switch (sBits.Hash())
		{
			case "128"_hash: return KBlockCipher::B128;
			case "192"_hash: return KBlockCipher::B192;
			case "256"_hash: return KBlockCipher::B256;
		}
		throw KError { kFormat("invalid block length: {}", sBits) };
	}

	//-----------------------------------------------------------------------------
	KString GetPlaintextInput(KStringViewZ sInput, bool bCompress)
	//-----------------------------------------------------------------------------
	{
		KString sContent;

#if DEKAF2_HAS_LIBZSTD
		if (bCompress)
		{
			KZSTD ZSTD(sContent);
			ZSTD.Write((sInput.empty() || kFileExists(sInput)) ? GetFileContent(sInput) : KString(sInput));
			ZSTD.close();
		}
		else
#endif
		{
			sContent = (sInput.empty() || kFileExists(sInput)) ? GetFileContent(sInput) : KString(sInput);
		}

		return sContent;
	}

	//-----------------------------------------------------------------------------
	KString GetCiphertextInput(KStringViewZ sInput, bool bBase64, bool bHexEncoded)
	//-----------------------------------------------------------------------------
	{
		KString sContent = (sInput.empty() || kFileExists(sInput)) ? GetFileContent(sInput) : KString(sInput);

		if (bBase64)
		{
			return KDec::Base64(sContent);
		}
		else if (bHexEncoded)
		{
			return KDec::Hex(sContent);
		}
		else
		{
			return sContent;
		}
	}

	//-----------------------------------------------------------------------------
	KOutStream GetOutStream(KStringViewZ sOutfile, KOutFile& OutFile)
	//-----------------------------------------------------------------------------
	{
		if (!sOutfile.empty())
		{
			OutFile.open(sOutfile, std::ios_base::out | std::ios_base::trunc);
			if (!OutFile.is_open()) throw KError { kFormat("cannot open output file: {}", sOutfile) };
			return OutFile.OutStream();
		}
		else
		{
			return KOut;
		}
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	int Main(int argc, char** argv)
	//-----------------------------------------------------------------------------
	{
		// initialize without signal handler thread
		KInit(false);

		// we will throw when setting errors
		SetThrowOnError(true);

		// setup CLI option parsing
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		// define cli options
		KStringView  sAlgorithm    = Options("a,algorithm  : algorithm: AES, ARIA, Camellia, ChaCha, defaults to AES", "AES");
		KStringView  sBits         = Options("b,bit        : key size, either 128, 192, or 256 bit, defaults to 256", "256");
		KStringView  sMode         = Options("m,mode       : encryption mode, either ECB, CBC, OCB, OFB, CFB1, CFB8, CFB128, CTR, CCM, GCM, or XTS, defaults to GCM", "GCM");
		KStringView  sPassword     = Options("p,password   : password");
		bool         bEncrypt      = Options("e,encrypt    : encrypt", "");
		bool         bDecrypt      = Options("x,decrypt    : decrypt", "");
		KStringViewZ sInput        = Options("i,input  <file|data> : input file name or input data, defaults to stdin", "");
		KStringViewZ sOutfile      = Options("o,output <file>      : output file name, defaults to stdout", "");
		bool         bHexEncoded   = Options("hex          : ciphertext is hex encoded"   , false);
		bool         bBase64       = Options("base64       : ciphertext is base64 encoded", false);
		bool         bBase64one    = Options("one64        : ciphertext is base64 encoded without linebreaks", false);
#if DEKAF2_HAS_LIBZSTD
		bool         bCompress     = Options("c,compress   : compress before encryption/decompress after decryption", false);
#else
		bool         bCompress     = false;
#endif

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		if (bBase64one) bBase64 = true;
		if ( bEncrypt &&  bDecrypt) throw KError { "can only either encrypt or decrypt" };
		if (!bEncrypt && !bDecrypt) throw KError { "please set either encrypt or decrypt option" };

		auto algorithm = GetAlgorithm(sAlgorithm);
		auto direction = (bDecrypt) ? KBlockCipher::Decrypt : KBlockCipher::Encrypt;
		auto mode      = GetMode(sMode);
		auto bits      = GetBits(sBits);
		auto sContent  = (direction == KBlockCipher::Encrypt)
			? GetPlaintextInput(sInput, bCompress)
			: GetCiphertextInput(sInput, bBase64, bHexEncoded);

		KString sOut;
		{
			KBlockCipher Cipher(direction, algorithm, mode, bits);
			Cipher.SetThrowOnError(true);
			Cipher.SetOutput(sOut);
			Cipher.SetPassword(sPassword);
			Cipher.Add(sContent);
			Cipher.Finalize();
		}

		KOutFile OutFile;
		auto OutStream = GetOutStream(sOutfile, OutFile);

		if (direction == KBlockCipher::Encrypt)
		{
			if (bBase64) OutStream.Write(KEnc::Base64(sOut, !bBase64one));
			else if (bHexEncoded) OutStream.Write(KEnc::Hex(sOut));
			else OutStream.Write(sOut);
		}
		else // decrypt
		{
#if DEKAF2_HAS_LIBZSTD
			if (bCompress)
			{
				KUnZSTD(sOut).Read(OutStream);
			}
			else
#endif
			{
				OutStream.Write(sOut);
			}
		}

		// only append a linefeed if we're not writing into a file nor pipe
		if (sOutfile.empty() && kStdOutIsTerminal()) OutStream.Write('\n');

		return 0;

	} // Main

}; // Krypt

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return Krypt().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", "krypt", ex.what());
	}
	return 1;
}
