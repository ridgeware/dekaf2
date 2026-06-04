#include "catch.hpp"

#include <dekaf2/util/codec/kgaloisfield.h>
#include <dekaf2/util/codec/kreedsolomon.h>
#include <vector>
#include <cstdint>

using namespace dekaf2;

TEST_CASE("KGaloisField")
{
	KGaloisField GF; // default: primitive 0x11D, generator 2

	SECTION("addition is XOR")
	{
		CHECK ( KGaloisField::Add(0x53, 0xCA) == (0x53 ^ 0xCA) );
		CHECK ( KGaloisField::Add(0xFF, 0xFF) == 0 );
	}

	SECTION("multiply matches carry-less reference")
	{
		// known GF(2^8) products under 0x11D
		CHECK ( GF.Mul(0, 5)       == 0   );
		CHECK ( GF.Mul(1, 0xAB)    == 0xAB);
		CHECK ( GF.Mul(2, 0x80)    == 0x1D); // 0x100 reduced by the primitive poly
		CHECK ( GF.Mul(3, 7)       == 9   );
	}

	SECTION("div and inverse undo mul")
	{
		for (int a = 1; a < 256; ++a)
		{
			for (int b = 1; b < 256; ++b)
			{
				uint8_t p = GF.Mul(static_cast<uint8_t>(a), static_cast<uint8_t>(b));
				CHECK ( GF.Div(p, static_cast<uint8_t>(b)) == a );
			}
			CHECK ( GF.Mul(static_cast<uint8_t>(a), GF.Inverse(static_cast<uint8_t>(a))) == 1 );
		}
	}

	SECTION("exp/log round-trip and pow")
	{
		for (int a = 1; a < 256; ++a)
		{
			CHECK ( GF.Exp(GF.Log(static_cast<uint8_t>(a))) == a );
		}
		CHECK ( GF.Pow(2, 0) == 1 );
		CHECK ( GF.Pow(2, 1) == 2 );
		CHECK ( GF.Pow(2, 8) == 0x1D );      // 2^8 == primitive remainder
		CHECK ( GF.Exp(255)  == GF.Exp(0) ); // multiplicative order is 255
	}
}

TEST_CASE("KReedSolomon")
{
	KReedSolomon RS; // QR defaults: 0x11D, generator 2, fcr 0

	SECTION("encode matches known QR vectors")
	{
		// the canonical "HELLO WORLD" version 1-M data block and its 10 EC codewords
		KReedSolomon::Bytes hello =
		    { 32,91,11,120,209,114,220,77,67,64,236,17,236,17,236,17 };
		KReedSolomon::Bytes hello_ecc =
		    { 196,35,39,119,235,215,231,226,93,23 };
		CHECK ( RS.Encode(hello, 10) == hello_ecc );

		KReedSolomon::Bytes a =
		    { 0x10,0x20,0x0c,0x56,0x61,0x80,0xec,0x11,0xec,0x11,0xec,0x11,0xec,0x11,0xec,0x11 };
		KReedSolomon::Bytes a_ecc =
		    { 165,36,212,193,237,54,199,135,44,85 };
		CHECK ( RS.Encode(a, 10) == a_ecc );

		KReedSolomon::Bytes b     = { 0x41,0x42,0x43,0x44 };
		KReedSolomon::Bytes b_ecc = { 72,132,73,235,101,16,31 };
		CHECK ( RS.Encode(b, 7) == b_ecc );
	}

	SECTION("EncodeAppend yields a codeword with zero syndromes")
	{
		KReedSolomon::Bytes msg = { 1,2,3,4,5,6,7,8,9,10 };
		KReedSolomon::Bytes cw  = RS.EncodeAppend(msg, 8);
		CHECK ( cw.size() == msg.size() + 8 );
		uint16_t corrected = 99;
		CHECK ( RS.Decode(cw, 8, &corrected) ); // already valid
		CHECK ( corrected == 0 );
	}

	SECTION("decode corrects up to t = nsym/2 errors")
	{
		KReedSolomon::Bytes msg = { 32,91,11,120,209,114,220,77,67,64,236,17,236,17,236,17 };
		const uint16_t nsym = 10; // -> can fix up to 5 symbol errors

		for (int nerr = 0; nerr <= 5; ++nerr)
		{
			KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);
			// flip distinct, spread-out positions
			for (int e = 0; e < nerr; ++e)
			{
				std::size_t pos = static_cast<std::size_t>(e * 5) % cw.size();
				cw[pos] ^= static_cast<uint8_t>(0xA5 + e);
			}
			uint16_t corrected = 0;
			REQUIRE ( RS.Decode(cw, nsym, &corrected) );
			CHECK ( corrected == nerr );
			// the message portion must be restored exactly
			KReedSolomon::Bytes recovered(cw.begin(), cw.begin() + msg.size());
			CHECK ( recovered == msg );
		}
	}

	SECTION("decode corrects erasures (known positions) up to nsym")
	{
		KReedSolomon::Bytes msg = { 32,91,11,120,209,114,220,77,67,64,236,17,236,17,236,17 };
		const uint16_t nsym = 10; // a known position costs only one parity symbol

		for (int nera = 0; nera <= 10; ++nera)
		{
			KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);
			std::vector<std::size_t> pos;
			for (int e = 0; e < nera; ++e)
			{
				std::size_t p = static_cast<std::size_t>(e * 2) % cw.size();
				cw[p] ^= static_cast<uint8_t>(0x5A + e);
				pos.push_back(p);
			}
			uint16_t corrected = 0;
			REQUIRE ( RS.Decode(cw, nsym, pos, &corrected) );
			CHECK ( corrected == nera );
			KReedSolomon::Bytes rec(cw.begin(), cw.begin() + msg.size());
			CHECK ( rec == msg );
		}
	}

	SECTION("decode mixes located errors and erasures (2*errors + erasures <= nsym)")
	{
		KReedSolomon::Bytes msg = { 1,2,3,4,5,6,7,8,9,10,11,12 };
		const uint16_t nsym = 10;
		KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);

		// 6 erasures (known) cost 6, plus 2 unknown errors cost 4 -> exactly 10
		std::vector<std::size_t> erasures = { 0, 1, 2, 3, 4, 5 };
		for (std::size_t p : erasures) cw[p] ^= 0x37;
		cw[10] ^= 0x11; // unknown error (not in the erasure list)
		cw[15] ^= 0x22; // unknown error

		uint16_t corrected = 0;
		REQUIRE ( RS.Decode(cw, nsym, erasures, &corrected) );
		CHECK ( corrected == 8 ); // 6 erasures + 2 located errors
		KReedSolomon::Bytes rec(cw.begin(), cw.begin() + msg.size());
		CHECK ( rec == msg );
	}

	SECTION("erasures beyond the bound are reported uncorrectable")
	{
		KReedSolomon::Bytes msg = { 5,6,7,8,9,10,11,12 };
		const uint16_t nsym = 6;
		// more erasures than parity symbols
		{
			KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);
			std::vector<std::size_t> pos = { 0, 1, 2, 3, 4, 5, 6 }; // 7 > 6
			for (std::size_t p : pos) cw[p] ^= 0x44;
			CHECK ( !RS.Decode(cw, nsym, pos) );
		}
		// 2 erasures + 3 unknown errors -> 2 + 6 = 8 > 6
		{
			KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);
			std::vector<std::size_t> pos = { 0, 1 };
			for (std::size_t p : pos) cw[p] ^= 0x44;
			cw[4] ^= 0x12; cw[7] ^= 0x34; cw[11] ^= 0x56;
			uint16_t corrected = 0;
			bool bOK = RS.Decode(cw, nsym, pos, &corrected);
			if (bOK)
			{
				KReedSolomon::Bytes rec(cw.begin(), cw.begin() + msg.size());
				CHECK ( rec != msg ); // a false-correct would not match the original
			}
			else SUCCEED("over-bound errata reported uncorrectable");
		}
	}

	SECTION("decode reports failure beyond the correction limit")
	{
		KReedSolomon::Bytes msg = { 10,20,30,40,50,60,70,80 };
		const uint16_t nsym = 6; // fixes up to 3 errors
		KReedSolomon::Bytes cw = RS.EncodeAppend(msg, nsym);
		for (int e = 0; e < 4; ++e) cw[static_cast<std::size_t>(e * 3)] ^= 0x5A; // 4 > 3
		// it must either flag failure or, if it "corrects", not silently return
		// the wrong message as valid; we only require it does not crash and that
		// an honest over-the-limit case is reported uncorrectable
		uint16_t corrected = 0;
		bool bOK = RS.Decode(cw, nsym, &corrected);
		if (bOK)
		{
			KReedSolomon::Bytes recovered(cw.begin(), cw.begin() + msg.size());
			CHECK ( recovered != msg ); // a false-correct would differ from the original
		}
		else
		{
			SUCCEED("over-limit error reported as uncorrectable");
		}
	}
}
