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
#include <dekaf2/kaes.h>
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
		if (!kReadAll(sFilename, sContent)) throw KError { kFormat("cannot read file: {}", sFilename) };
		return sContent;
	}

	//-----------------------------------------------------------------------------
	KAES::Algorithm GetAlgorithm(KStringView sAlgorithm)
	//-----------------------------------------------------------------------------
	{
		switch (sAlgorithm.CaseHash())
		{
			case "aes"_casehash: return KAES::AES;
		}
		throw KError { kFormat("invalid algorithm: {}", sAlgorithm) };
	}

	//-----------------------------------------------------------------------------
	KAES::Mode GetMode(KStringView sMode)
	//-----------------------------------------------------------------------------
	{
		switch (sMode.CaseHash())
		{
			case "ecb"_casehash: return KAES::ECB;
			case "cbc"_casehash: return KAES::CBC;
			case "ccm"_casehash: return KAES::CCM;
			case "gcm"_casehash: return KAES::GCM;
		}
		throw KError { kFormat("invalid mode: {}", sMode) };
	}

	//-----------------------------------------------------------------------------
	KAES::Bits GetBits(KStringView sBits)
	//-----------------------------------------------------------------------------
	{
		switch (sBits.Hash())
		{
			case "128"_hash: return KAES::B128;
			case "192"_hash: return KAES::B192;
			case "256"_hash: return KAES::B256;
		}
		throw KError { kFormat("invalid block length: {}", sBits) };
	}

	//-----------------------------------------------------------------------------
	KString GetPlaintextInput(KStringViewZ sInput, bool bCompress)
	//-----------------------------------------------------------------------------
	{
		KString sContent;

		if (bCompress)
		{
			KZSTD ZSTD(sContent);
			ZSTD.Write((kFileExists(sInput)) ? GetFileContent(sInput) : KString(sInput));
			ZSTD.close();
		}
		else
		{
			sContent = (kFileExists(sInput)) ? GetFileContent(sInput) : KString(sInput);
		}

		return sContent;
	}

	//-----------------------------------------------------------------------------
	KString GetCiphertextInput(KStringViewZ sInput, bool bBase64, bool bHexEncoded)
	//-----------------------------------------------------------------------------
	{
		KString sContent;

		if (kFileExists(sInput)) sContent = GetFileContent(sInput);
		else sContent = sInput;

		if (bBase64)
		{
			KDec::Base64InPlace(sContent);
		}
		else if (bHexEncoded)
		{
			KDec::HexInPlace(sContent);
		}

		return sContent;
	}

	//-----------------------------------------------------------------------------
	KOutStream GetOutStream(KStringViewZ sOutfile)
	//-----------------------------------------------------------------------------
	{
		if (!sOutfile.empty())
		{
			KOutFile of;
			of.open(sOutfile, std::ios_base::out | std::ios_base::trunc);
			if (!of.is_open()) throw KError { kFormat("cannot open output file: {}", sOutfile) };
			return of;
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
		KString      sAlgorithm    = Options("a,algorithm  : algorithm, defaults to AES", "AES");
		KStringView  sBits         = Options("b,bit        : block length, either 128, 192, or 256, defaults to 256", 256);
		KString      sMode         = Options("m,mode       : encryption mode, either ECB, CBC, CCM, or GCM, defaults to GCM", "GCM");
		KStringView  sPassword     = Options("p,password   : password");
		KStringViewZ sEncrypt      = Options("e,encrypt <plain |file> : encrypt, followed by plaintext  or filename to encrypt", "");
		KStringViewZ sDecrypt      = Options("x,decrypt <cipher|file> : decrypt, followed by ciphertext or filename to decrypt", "");
		KStringViewZ sOutfile      = Options("o,output  <file>        : output file name, defaults to stdout", "");

		bool         bHexEncoded   = Options("hex          : ciphertext is hex encoded"   , false);
		bool         bBase64       = Options("base64       : ciphertext is base64 encoded", false);
		bool         bBase64one    = Options("one64        : ciphertext is base64 encoded without linebreaks", false);
		bool         bCompress     = Options("c,compress   : compress before encryption/decompress after decryption", false);

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		if (bBase64one) bBase64 = true;
		if (!sEncrypt.empty() && !sDecrypt.empty()) throw KError { "can only either encrypt or decrypt" };
		if ( sEncrypt.empty() &&  sDecrypt.empty()) throw KError { "missing input, use either encrypt or decrypt option" };

		auto algorithm = GetAlgorithm(sAlgorithm);
		auto direction = (sEncrypt.empty()) ? KAES::Direction::Decrypt : KAES::Direction::Encrypt;
		auto mode      = GetMode(sMode);
		auto bits      = GetBits(sBits);
		auto sContent  = (direction == KAES::Encrypt)
			? GetPlaintextInput(sEncrypt, bCompress)
			: GetCiphertextInput(sDecrypt, bBase64, bHexEncoded);

		KString sOut;
		{
			KAES AES(sOut, direction, algorithm, mode, bits);
			AES.SetThrowOnError(true);
			AES.SetPassword(sPassword);
			AES.Add(sContent);
			AES.Finalize();
		}

		auto OutStream = GetOutStream(sOutfile);

		if (direction == KAES::Direction::Encrypt)
		{
			if (bBase64) OutStream.Write(KEnc::Base64(sOut, !bBase64one));
			else if (bHexEncoded) OutStream.Write(KEnc::Hex(sOut));
			else OutStream.Write(sOut);
		}
		else // decrypt
		{
			if (bCompress) KUnZSTD(sOut).Read(OutStream);
			else OutStream.Write(sOut);
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
		kPrintLine(">> {}: {}", "krypt", ex.what());
	}
	return 1;
}
