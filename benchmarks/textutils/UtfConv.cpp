#include "UtfConv.h"
#include "UnicodeMacros.h"
#include <string.h>

using namespace std;

// implementation

// ---------------------------------------------------------------------------
//  Local static data
//
//  gUTFBytes
//	  A list of counts of trailing bytes for each initial byte in the input.
//
//  gUTFByteIndicator
//	  For a UTF8 sequence of n bytes, n>=2, the first byte of the
//	  sequence must contain n 1's followed by precisely 1 0 with the
//	  rest of the byte containing arbitrary bits.  This array stores
//	  the required bit pattern for validity checking.
//  gUTFByteIndicatorTest
//	  When bitwise and'd with the observed value, if the observed
//	  value is correct then a result matching gUTFByteIndicator will
//	  be produced.
//
//  gUTFOffsets
//	  A list of values to offset each result char type, according to how
//	  many source bytes when into making it.
//
//  gFirstByteMark
//	  A list of values to mask onto the first byte of an encoded sequence,
//	  indexed by the number of bytes used to create the sequence.
// ---------------------------------------------------------------------------
static const unsigned char gUTFBytes[256] =
{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,   0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	,   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	,   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
	,   3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

static const unsigned char gUTFByteIndicator[6] =
{
	0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};
static const unsigned char gUTFByteIndicatorTest[6] =
{
	0x80, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE
};

static const unsigned int gUTFOffsets[6] =
{
	0, 0x3080, 0xE2080, 0x3C82080, 0xFA082080, 0x82082080
};

static const unsigned char gFirstByteMark[7] =
{
	0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};

inline 
bool checkTrailingBytes(const unsigned char toCheck)
{
    return ((toCheck & 0xC0) == 0x80);
}

/*Converts a UTF-32 sequence to a UTF-8 sequence.  The
  function returns the number of items converted.
  The offset array maps by its index, the location in the source to the location in the target*/
int utf_conv::UTF32ToUTF8(const   UnicodeChar32* const	srcData
								, const unsigned int	srcCount
								, unsigned int*	const   charsEaten
								, char* const			toFill
								, const unsigned int	maxBytes
								, int* const			offsetArray
								, const unsigned int	maxOffsets)
{
	// Watch for pathological scenario. Shouldn't happen, but...
	if (!srcCount)
		return 0;

	//
	//  Get pointers to our start and end points of the input and output
	//  buffers.
	//
	const UnicodeChar32*	srcPtr = srcData;
	const UnicodeChar32*	srcEnd = srcPtr + srcCount;
	char*					outPtr = (maxBytes != 0) ? toFill : NULL;
	int*					offsetPtr = offsetArray;
	int*					offsetEnd = offsetPtr + maxOffsets;

	unsigned int numConverted = 0;

	while (srcPtr < srcEnd)
	{
		//
		//  get the next char out.
		//
		unsigned int curVal = *srcPtr;

		//
		//  If it is in the range reserved for surrogates, then it is invalid Unicode
		//
		bool bError = false;
		if (UD_IS_SURROGATE(curVal))
			bError = true;

		// Figure out how many bytes we need
		unsigned int encodedBytes;
		if (curVal < 0x80)
			encodedBytes = 1;
		else if (curVal < 0x800)
			encodedBytes = 2;
		else if (curVal < 0x10000)
			encodedBytes = 3;
		else if (curVal < 0x110000)
			encodedBytes = 4;
		else
			bError = true;

		if (bError)
		{
			return -1;
		}

		//
		//  If we cannot fully get this char into the output buffer,
		//  then leave it for the next time.
		//
		if ((maxBytes != 0) && ((numConverted + encodedBytes) > maxBytes))
			break;

		// We can do it, so update the source index
		srcPtr++;

		//
		//  And spit out the bytes. We spit them out in reverse order
		//  here, so bump up the output pointer and work down as we go.
		//
		if (outPtr)
		{
			outPtr += encodedBytes;
			switch (encodedBytes)
			{
			case 6 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 5 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 4 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 3 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 2 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 1 : *--outPtr = char
						(
						curVal | gFirstByteMark[encodedBytes]
						);
			}

			// Add the encoded bytes back in again to indicate we've eaten them
			outPtr += encodedBytes;
		}

		if (offsetPtr && (offsetPtr < offsetEnd))
			*offsetPtr++ = (int)numConverted;

		numConverted += encodedBytes;
	}

	// Fill in the chars we ate
	if (charsEaten)
		*charsEaten = (unsigned int)(srcPtr - srcData);

	if (offsetPtr && (offsetPtr < offsetEnd))
		*offsetPtr = (int)numConverted;

	// And return the bytes we filled in
	return (int)numConverted; //(outPtr - toFill);
}

/*Converts a UTF-16 sequence to a UTF-8 sequence.  The
  function returns the number of items converted.
  The offset array maps by its index, the location in the source to the location in the target*/
int utf_conv::UTF16ToUTF8(const   UnicodeChar* const	srcData
								, const unsigned int	srcCount
								, unsigned int* const 	charsEaten
								, char* const			toFill
								, const unsigned int	maxBytes
								, int* const			offsetArray
								, const unsigned int	maxOffsets
								, const bool			bDataPending)
{
	// Watch for pathological scenario. Shouldn't happen, but...
	if (!srcCount)
		return 0;

	//
	//  Get pointers to our start and end points of the input and output
	//  buffers.
	//
	const UnicodeChar*	srcPtr = srcData;
	const UnicodeChar*	srcEnd = srcPtr + srcCount;
	char*			    outPtr = (maxBytes != 0 ) ? toFill : NULL;
	int*			    offsetPtr = offsetArray;
	int*			    offsetEnd = offsetPtr + maxOffsets;

	unsigned int numConverted = 0;

	while (srcPtr < srcEnd)
	{
		//
		//  Tentatively get the next char out. We have to get it into a
		//  32 bit value, because it could be a surrogate pair.
		//
		unsigned int curVal = *srcPtr;

		//
		//  If its a leading surrogate, then lets see if we have the trailing
		//  available. If not, then give up now and leave it for next time.
		//
		unsigned int srcUsed = 1;
		bool bError = false;
		if (UD_IS_LEAD_SURROGATE(curVal))
		{
			if ((srcPtr + 1) >= srcEnd)
			{
				if (!bDataPending)
					bError = true;
				else
					break;
			}
			else if (!UD_IS_TRAIL_SURROGATE(*(srcPtr + 1))) // Make sure next char is in the proper range for a trailing surrogate
			{
				bError = true;
			}
			else
			{
				// Create the composite surrogate pair
				curVal = ((curVal - 0xD800) << 10) + ((*(srcPtr + 1) - 0xDC00) + 0x10000);

				// And indicate that we ate another one
				srcUsed++;
			}
		}
		else if (UD_IS_TRAIL_SURROGATE(curVal)) // trailing surrogate, without a leading surrogate preceding it
		{
			bError = true;
		}

		// Figure out how many bytes we need
		unsigned int encodedBytes;
		if (curVal < 0x80)
			encodedBytes = 1;
		else if (curVal < 0x800)
			encodedBytes = 2;
		else if (curVal < 0x10000)
			encodedBytes = 3;
		else if (curVal < 0x110000)
			encodedBytes = 4;
		else
		{
			bError = true;
		}

		if (bError)
		{
			return -1;
		}

		//
		//  If we cannot fully get this char into the output buffer,
		//  then leave it for the next time.
		//
		if ((maxBytes != 0) && ((numConverted + encodedBytes) > maxBytes))
			break;

		// We can do it, so update the source index
		srcPtr += srcUsed;

		//
		//  And spit out the bytes. We spit them out in reverse order
		//  here, so bump up the output pointer and work down as we go.
		//
		if (outPtr)
		{
			outPtr += encodedBytes;
			switch (encodedBytes)
			{
			case 6 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 5 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 4 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 3 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 2 : *--outPtr = char((curVal | 0x80UL) & 0xBFUL);
						curVal >>= 6;
			case 1 : *--outPtr = char
						(
						curVal | gFirstByteMark[encodedBytes]
						);
			}

			// Add the encoded bytes back in again to indicate we've eaten them
			outPtr += encodedBytes;
		}

		if (offsetPtr && (offsetPtr < offsetEnd))
		{
			*offsetPtr++ = (int)numConverted;
			for (unsigned int i = 1; (i < srcUsed) && (offsetPtr < offsetEnd); ++i)
				*offsetPtr++ = -1;
		}

		numConverted += encodedBytes;
	}

	// Fill in the chars we ate
	if (charsEaten)
		*charsEaten = (unsigned int)(srcPtr - srcData);

	if (offsetPtr && (offsetPtr < offsetEnd))
		*offsetPtr = (int)numConverted;

	// And return the bytes we filled in
	return (int)numConverted; //(outPtr - toFill);
}

/*Converts a UTF-8 sequence to UTF-16. The function returns the
  number of converted characters.  If the
  function returns -1 then there was an encoding error in srcData.
  The offset array maps by its index, the location in the target to the location in the source*/
int utf_conv::UTF8ToUTF16(const  char* const			srcData
								, const unsigned int	srcCount
								, unsigned int* const	bytesEaten
								, UnicodeChar* const	toFill
								, const unsigned int	maxChars
								, int* const 			offsetArray
								, const unsigned int	maxOffsets)
{
	// Watch for pathological scenario. Shouldn't happen, but...
	if (!srcCount)
		return 0;

	//
	//  Get pointers to our start and end points of the input and output
	//  buffers.
	//
	const unsigned char*  srcStart = (const unsigned char*)srcData;
	const unsigned char*  srcPtr = srcStart;
	const unsigned char*  srcEnd = srcPtr + srcCount;
	UnicodeChar*		  outPtr = (maxChars != 0) ? toFill : NULL;
	int*				  offsetPtr = offsetArray;
	int*				  offsetEnd = offsetPtr + maxOffsets;

	unsigned int numConverted = 0;

	//
	//  We now loop until we either run out of input data, or room to store
	//  output chars.
	//
	while ((srcPtr < srcEnd) && ((maxChars == 0) || (numConverted < maxChars)))
	{
		int srcRelPos = (int)(srcPtr - srcStart);
		// Special-case ASCII, which is a leading byte value of <= 127
		if (*srcPtr <= 127)
		{
			// Handle ASCII in groups instead of single character at a time.
			do
			{
				if (outPtr)
					*outPtr++ = UnicodeChar(*srcPtr);
				if (offsetPtr && (offsetPtr < offsetEnd))
					*offsetPtr++ = srcRelPos;
				numConverted++;
				srcPtr++;
				srcRelPos++;
			} while ((srcPtr != srcEnd) && (*srcPtr <= 127) &&
					  ((maxChars == 0) || (numConverted < maxChars) ) );
			if ((srcPtr == srcEnd) || ((maxChars != 0) && (numConverted >= maxChars)))
				break;
		}

		// See how many trailing src bytes this sequence is going to require
		const unsigned int trailingBytes = gUTFBytes[*srcPtr];

		//
		//  If there are not enough source bytes to do this one, then we
		//  are done. Note that we done >= here because we are implicitly
		//  counting the 1 byte we get no matter what.
		//
		//  If we break out here, then there is nothing to undo since we
		//  haven't updated any pointers yet.
		//
		if (srcPtr + trailingBytes >= srcEnd)
		{
			return -1;
		}

		// Looks ok, so lets build up the value
		// or at least let's try to do so--remembering that
		// we cannot assume the encoding to be valid:

		// first, test first byte
		if ((gUTFByteIndicatorTest[trailingBytes] & *srcPtr) != gUTFByteIndicator[trailingBytes])
		{
			return -1;
		}

		/***
		* http://www.unicode.org/reports/tr27/
		*
		* Table 3.1B. lists all of the byte sequences that are legal in UTF-8.
		* A range of byte values such as A0..BF indicates that any byte from A0 to BF (inclusive)
		* is legal in that position.
		* Any byte value outside of the ranges listed is illegal.
		* For example,
		* the byte sequence <C0 AF> is illegal  since C0 is not legal in the 1st Byte column.
		* The byte sequence <E0 9F 80> is illegal since in the row
		*	where E0 is legal as a first byte,
		*	9F is not legal as a second byte.
		* The byte sequence <F4 80 83 92> is legal, since every byte in that sequence matches
		* a byte range in a row of the table (the last row).
		*
		*
		* Table 3.1B. Legal UTF-8 Byte Sequences
		* Code Points			  1st Byte	2nd Byte	3rd Byte	4th Byte
		* =========================================================================
		* U+0000..U+007F			00..7F
		* -------------------------------------------------------------------------
		* U+0080..U+07FF			C2..DF	  80..BF
		*
		* -------------------------------------------------------------------------
		* U+0800..U+0FFF			E0		  A0..BF	 80..BF
		*									   --
		*
		* U+1000..U+FFFF			E1..EF	  80..BF	 80..BF
		*
		* --------------------------------------------------------------------------
		* U+10000..U+3FFFF		  F0		  90..BF	 80..BF	   80..BF
		*									   --
		* U+40000..U+FFFFF		  F1..F3	  80..BF	 80..BF	   80..BF
		* U+100000..U+10FFFF		F4		  80..8F	 80..BF	   80..BF
		*										   --
		* ==========================================================================
		*
		*  Cases where a trailing byte range is not 80..BF are underlined in the table to
		*  draw attention to them. These occur only in the second byte of a sequence.
		*
		***/

		unsigned int tmpVal = 0;

		switch (trailingBytes)
		{
		case 1 :
			// UTF-8:   [110y yyyy] [10xx xxxx]
			// Unicode: [0000 0yyy] [yyxx xxxx]
			//
			// 0xC0, 0xC1 has been filtered out			 
			if (!checkTrailingBytes(*(srcPtr+1)))
			{
				return -1;
			}

			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		case 2 :
			// UTF-8:   [1110 zzzz] [10yy yyyy] [10xx xxxx]
			// Unicode: [zzzz yyyy] [yyxx xxxx]
			//
			if (( *srcPtr == 0xE0) && ( *(srcPtr+1) < 0xA0)) 
			{
				return -1;
			}

			if (!checkTrailingBytes(*(srcPtr+1)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr+2)))
			{
				return -1;
			}

			//
			// D36 (a) UTF-8 is the Unicode Transformation Format that serializes 
			//		 a Unicode code point as a sequence of one to four bytes, 
			//		 as specified in Table 3.1, UTF-8 Bit Distribution.
			//	 (b) An illegal UTF-8 code unit sequence is any byte sequence that 
			//		 does not match the patterns listed in Table 3.1B, Legal UTF-8 
			//		 Byte Sequences.
			//	 (c) An irregular UTF-8 code unit sequence is a six-byte sequence 
			//		 where the first three bytes correspond to a high surrogate, 
			//		 and the next three bytes correspond to a low surrogate. 
			//		 As a consequence of C12, these irregular UTF-8 sequences shall 
			//		 not be generated by a conformant process. 
			//
			//irregular three bytes sequence
			// that is zzzzyy matches leading surrogate tag 110110 or 
			//					   trailing surrogate tag 110111
			// *srcPtr=1110 1101 
			// *(srcPtr+1)=1010 yyyy or 
			// *(srcPtr+1)=1011 yyyy
			//
			// 0xED 1110 1101
			// 0xA0 1010 0000

			if ((*srcPtr == 0xED) && (*(srcPtr+1) >= 0xA0))
			{
				return -1;
			}

			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		case 3 : 
			// UTF-8:   [1111 0uuu] [10uu zzzz] [10yy yyyy] [10xx xxxx]*
			// Unicode: [1101 10ww] [wwzz zzyy] (high surrogate)
			//		  [1101 11yy] [yyxx xxxx] (low surrogate)
			//		  * uuuuu = wwww + 1
			//
			if (((*srcPtr == 0xF0) && (*(srcPtr+1) < 0x90)) ||
				((*srcPtr == 0xF4) && (*(srcPtr+1) > 0x8F))  )
			{
				return -1;
			}

			if (!checkTrailingBytes(*(srcPtr+1)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr+2)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr+3)))
			{
				return -1;
			}
				
			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		default: // trailingBytes > 3

			/***
				* The definition of UTF-8 in Annex D of ISO/IEC 10646-1:2000 also allows 
				* for the use of five- and six-byte sequences to encode characters that 
				* are outside the range of the Unicode character set; those five- and 
				* six-byte sequences are illegal for the use of UTF-8 as a transformation 
				* of Unicode characters. ISO/IEC 10646 does not allow mapping of unpaired 
				* surrogates, nor U+FFFE and U+FFFF (but it does allow other noncharacters).
				***/
			return -1;
		}

		tmpVal -= gUTFOffsets[trailingBytes];

		//
		//  If it will fit into a single char, then put it in. Otherwise
		//  encode it as a surrogate pair. If its not valid, use the
		//  replacement char.
		//
		if (!(tmpVal & 0xFFFF0000))
		{
			if (outPtr)
				*outPtr++ = UnicodeChar(tmpVal);

			if (offsetPtr && (offsetPtr < offsetEnd))
				*offsetPtr++ = srcRelPos;
			numConverted++;
		}
		else if (tmpVal > 0x10FFFF)
		{
			return -1;
		}
		else
		{
			//
			//  If we have enough room to store the leading and trailing
			//  chars, then lets do it. Else, pretend this one never
			//  happened, and leave it for the next time. Since we don't
			//  update the bytes read until the bottom of the loop, by
			//  breaking out here its like it never happened.
			//
			if (outPtr)
			{
				if ((numConverted + 1) >= maxChars)
					break;

				// Store the leading surrogate char
				tmpVal -= 0x10000;
				*outPtr++ = UnicodeChar((tmpVal >> 10) + 0xD800);

				//
				//  And then the trailing char. This one accounts for no
				//  bytes eaten from the source, so set the char size for this
				//  one to be zero.
				//
				*outPtr++ = UnicodeChar((tmpVal & 0x3FF) + 0xDC00);
			}

			if (offsetPtr && (offsetPtr < offsetEnd))
			{
				*offsetPtr++ = srcRelPos;
				if (offsetPtr < offsetEnd)
					*offsetPtr++ = -1;
			}
			numConverted += 2;
		}
	}

	int srcRelPos = (int)(srcPtr - srcStart);
	// Update the bytes eaten
	if (bytesEaten)
		*bytesEaten = (unsigned int)srcRelPos;
	if (offsetPtr && (offsetPtr < offsetEnd))
		*offsetPtr = srcRelPos;

	// Return the characters read
	return (int)numConverted; //outPtr - toFill;
}

/*Converts a UTF-8 sequence to UTF-32. The function returns the
  number of converted characters.  If the
  function returns -1 then there was an encoding error in srcData.
  The offset array maps by its index, the location in the target to the location in the source*/
int utf_conv::UTF8ToUTF32(const  char* const			srcData
								, const unsigned int	srcCount
								, unsigned int* const	bytesEaten
								, UnicodeChar32* const	toFill
								, const unsigned int	maxChars
								, int* const			offsetArray
								, const unsigned int	maxOffsets)
{
	// Watch for pathological scenario. Shouldn't happen, but...
	if (!srcCount)
		return 0;

	//
	//  Get pointers to our start and end points of the input and output
	//  buffers.
	//
	const unsigned char*  srcStart = (const unsigned char*)srcData;
	const unsigned char*  srcPtr = srcStart;
	const unsigned char*  srcEnd = srcPtr + srcCount;
	UnicodeChar32*		  outPtr = (maxChars != 0) ? toFill : NULL;
	int*				  offsetPtr = offsetArray;
	int*				  offsetEnd = offsetPtr + maxOffsets;

	unsigned int numConverted = 0;

	//
	//  We now loop until we either run out of input data, or room to store
	//  output chars.
	//
	while ((srcPtr < srcEnd) && ((maxChars == 0) || (numConverted < maxChars)))
	{
		int srcRelPos = (int)(srcPtr - srcStart);
		// Special-case ASCII, which is a leading byte value of <= 127
		if (*srcPtr <= 127)
		{
			// Handle ASCII in groups instead of single character at a time.
			do
			{
				if (outPtr)
					*outPtr++ = UnicodeChar32(*srcPtr);
				if (offsetPtr && (offsetPtr < offsetEnd))
					*offsetPtr++ = srcRelPos;
				numConverted++;
				srcPtr++;
				srcRelPos++;
			} while ((srcPtr != srcEnd) && (*srcPtr <= 127) &&
					  ((maxChars == 0) || (numConverted < maxChars) ) );
			if ((srcPtr == srcEnd) || ((maxChars != 0) && (numConverted >= maxChars)))
				break;
		}

		// See how many trailing src bytes this sequence is going to require
		const unsigned int trailingBytes = gUTFBytes[*srcPtr];

		//
		//  If there are not enough source bytes to do this one, then we
		//  are done. Note that we done >= here because we are implicitly
		//  counting the 1 byte we get no matter what.
		//
		//  If we break out here, then there is nothing to undo since we
		//  haven't updated any pointers yet.
		//
		if (srcPtr + trailingBytes >= srcEnd)
		{
			return -1;
		}

		// Looks ok, so lets build up the value
		// or at least let's try to do so--remembering that
		// we cannot assume the encoding to be valid:

		// first, test first byte
		if ((gUTFByteIndicatorTest[trailingBytes] & *srcPtr) != gUTFByteIndicator[trailingBytes])
		{
			return -1;
		}

		/***
		* http://www.unicode.org/reports/tr27/
		*
		* Table 3.1B. lists all of the byte sequences that are legal in UTF-8.
		* A range of byte values such as A0..BF indicates that any byte from A0 to BF (inclusive)
		* is legal in that position.
		* Any byte value outside of the ranges listed is illegal.
		* For example,
		* the byte sequence <C0 AF> is illegal  since C0 is not legal in the 1st Byte column.
		* The byte sequence <E0 9F 80> is illegal since in the row
		*	where E0 is legal as a first byte,
		*	9F is not legal as a second byte.
		* The byte sequence <F4 80 83 92> is legal, since every byte in that sequence matches
		* a byte range in a row of the table (the last row).
		*
		*
		* Table 3.1B. Legal UTF-8 Byte Sequences
		* Code Points			  1st Byte	2nd Byte	3rd Byte	4th Byte
		* =========================================================================
		* U+0000..U+007F			00..7F
		* -------------------------------------------------------------------------
		* U+0080..U+07FF			C2..DF	  80..BF
		*
		* -------------------------------------------------------------------------
		* U+0800..U+0FFF			E0		  A0..BF	 80..BF
		*									   --
		*
		* U+1000..U+FFFF			E1..EF	  80..BF	 80..BF
		*
		* --------------------------------------------------------------------------
		* U+10000..U+3FFFF		  F0		  90..BF	 80..BF	   80..BF
		*									   --
		* U+40000..U+FFFFF		  F1..F3	  80..BF	 80..BF	   80..BF
		* U+100000..U+10FFFF		F4		  80..8F	 80..BF	   80..BF
		*										   --
		* ==========================================================================
		*
		*  Cases where a trailing byte range is not 80..BF are underlined in the table to
		*  draw attention to them. These occur only in the second byte of a sequence.
		*
		***/

		unsigned int tmpVal = 0;

		switch (trailingBytes)
		{
		case 1:
			// UTF-8:   [110y yyyy] [10xx xxxx]
			// Unicode: [0000 0yyy] [yyxx xxxx]
			//
			// 0xC0, 0xC1 has been filtered out			 
			if (!checkTrailingBytes(*(srcPtr + 1)))
			{
				return -1;
			}

			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		case 2:
			// UTF-8:   [1110 zzzz] [10yy yyyy] [10xx xxxx]
			// Unicode: [zzzz yyyy] [yyxx xxxx]
			//
			if ((*srcPtr == 0xE0) && (*(srcPtr + 1) < 0xA0))
			{
				return -1;
			}

			if (!checkTrailingBytes(*(srcPtr + 1)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr + 2)))
			{
				return -1;
			}

			//
			// D36 (a) UTF-8 is the Unicode Transformation Format that serializes 
			//		 a Unicode code point as a sequence of one to four bytes, 
			//		 as specified in Table 3.1, UTF-8 Bit Distribution.
			//	 (b) An illegal UTF-8 code unit sequence is any byte sequence that 
			//		 does not match the patterns listed in Table 3.1B, Legal UTF-8 
			//		 Byte Sequences.
			//	 (c) An irregular UTF-8 code unit sequence is a six-byte sequence 
			//		 where the first three bytes correspond to a high surrogate, 
			//		 and the next three bytes correspond to a low surrogate. 
			//		 As a consequence of C12, these irregular UTF-8 sequences shall 
			//		 not be generated by a conformant process. 
			//
			//irregular three bytes sequence
			// that is zzzzyy matches leading surrogate tag 110110 or 
			//					   trailing surrogate tag 110111
			// *srcPtr=1110 1101 
			// *(srcPtr+1)=1010 yyyy or 
			// *(srcPtr+1)=1011 yyyy
			//
			// 0xED 1110 1101
			// 0xA0 1010 0000

			if ((*srcPtr == 0xED) && (*(srcPtr + 1) >= 0xA0))
			{
				return -1;
			}

			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		case 3:
			// UTF-8:   [1111 0uuu] [10uu zzzz] [10yy yyyy] [10xx xxxx]*
			// Unicode: [1101 10ww] [wwzz zzyy] (high surrogate)
			//		  [1101 11yy] [yyxx xxxx] (low surrogate)
			//		  * uuuuu = wwww + 1
			//
			if (((*srcPtr == 0xF0) && (*(srcPtr + 1) < 0x90)) ||
				((*srcPtr == 0xF4) && (*(srcPtr + 1) > 0x8F)))
			{
				return -1;
			}

			if (!checkTrailingBytes(*(srcPtr + 1)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr + 2)))
			{
				return -1;
			}
			if (!checkTrailingBytes(*(srcPtr + 3)))
			{
				return -1;
			}

			tmpVal = *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;
			tmpVal <<= 6;
			tmpVal += *srcPtr++;

			break;
		default: // trailingBytes > 3

			/***
			* The definition of UTF-8 in Annex D of ISO/IEC 10646-1:2000 also allows
			* for the use of five- and six-byte sequences to encode characters that
			* are outside the range of the Unicode character set; those five- and
			* six-byte sequences are illegal for the use of UTF-8 as a transformation
			* of Unicode characters. ISO/IEC 10646 does not allow mapping of unpaired
			* surrogates, nor U+FFFE and U+FFFF (but it does allow other noncharacters).
			***/
			return -1;
		}

		tmpVal -= gUTFOffsets[trailingBytes];

		//
		//  If its not valid, use the
		//  replacement char.
		//
		if (tmpVal > 0x10FFFF)
		{
			return -1;
		}
		else
		{
			if (outPtr)
				*outPtr++ = UnicodeChar32(tmpVal);

			if (offsetPtr && (offsetPtr < offsetEnd))
				*offsetPtr++ = srcRelPos;
			numConverted++;
		}
	}

	int srcRelPos = (int)(srcPtr - srcStart);
	// Update the bytes eaten
	if (bytesEaten)
		*bytesEaten = (unsigned int)srcRelPos;
	if (offsetPtr && (offsetPtr < offsetEnd))
		*offsetPtr = srcRelPos;

	// Return the characters read
	return (int)numConverted; //outPtr - toFill;
}

int utf_conv::UTF32ToUTF8(UnicodeChar32 c, char* buffer, int buff_size)
{
	if (c < 0x80 && buff_size > 0)
	{
		buffer[0] = (char)c;
		return 1;
	}

	unsigned int charsEaten = 0;
	int len = UTF32ToUTF8(&c, 1, &charsEaten, buffer, buff_size, NULL, 0);
	if (len <= 0 || charsEaten != 1)
	{
		return 0;
	}
	return len;
}

void utf_conv::UTF32ToUTF8(UnicodeChar32 c, string& strRet)
{
	strRet = UTF32ToUTF8(c);
}

string utf_conv::UTF32ToUTF8(UnicodeChar32 c)
{
	string strRet;
	if (c < 0x80)
	{
		strRet.resize(1);
		strRet[0] = (char)c;
	}
	else
	{
		const int buff_size = 5;
		char szBuff[buff_size];
		unsigned int charsEaten = 0;
		int len = UTF32ToUTF8(&c, 1, &charsEaten, szBuff, buff_size, NULL, 0);
		if (!((len <= 0) || (charsEaten != 1)))
			strRet.assign(szBuff, len);
		else
			strRet.clear();
	}

	return strRet;
}

void utf_conv::UTF32ToUTF8(const basic_string<UnicodeChar32>& ustrSrc, string& strRet, vector<int>* pvcOffsets)
{
	strRet = UTF32ToUTF8(ustrSrc, pvcOffsets);
}

string utf_conv::UTF32ToUTF8(const basic_string<UnicodeChar32>& ustrSrc, vector<int>* pvcOffsets)
{
	string strRet;
	if (ustrSrc.length() == 1) {
		strRet = UTF32ToUTF8(ustrSrc[0]);
		if (pvcOffsets)
			pvcOffsets->resize(1, 0);
		return strRet;
	}

	unsigned int srcCount = (unsigned int)ustrSrc.length();
	unsigned int charsEaten = 0;
	const UnicodeChar32* srcData = ustrSrc.c_str();

	// get buffer size
	int buff_size = UTF32ToUTF8(srcData, srcCount, &charsEaten, NULL, 0, NULL, 0);
	if (!((buff_size <= 0) || (charsEaten != srcCount))) {
		strRet.resize(buff_size);
		bool offsets = (pvcOffsets && srcCount > 0);
		if (pvcOffsets)
			pvcOffsets->resize(srcCount);
		if ((UTF32ToUTF8(srcData, srcCount, &charsEaten, &*strRet.begin(), buff_size, (offsets ? &*(pvcOffsets->begin()) : NULL), offsets ? srcCount : 0) <= 0) || (charsEaten != srcCount)) {
			strRet.clear();
			if (pvcOffsets)
				pvcOffsets->clear();
		}
	}

	return strRet;
}

std::basic_string<UnicodeChar> utf_conv::UTF32ToUTF16(UnicodeChar32 c)
{
	std::basic_string<UnicodeChar> ustrRet;
	if ((c >= 0x110000) || (c < 0)) {
		// error
		return ustrRet;
	}

	if (c <= 0xFFFF) {
		ustrRet.append(1, (UnicodeChar)c);
	}
	else {
		// split into 2
		// Store the leading surrogate char
		c -= 0x10000;
		ustrRet.append(1, UnicodeChar((c >> 10) + 0xD800));

		//  And then the trailing char.
		ustrRet.append(1, UnicodeChar((c & 0x3FF) + 0xDC00));
	}

	return ustrRet;
}

void utf_conv::UTF32ToUTF16(const basic_string<UnicodeChar32>& ustrSrc, basic_string<UnicodeChar>& ustrRet, vector<int>* pvcOffsets)
{
	ustrRet = UTF32ToUTF16(ustrSrc, pvcOffsets);
}

basic_string<UnicodeChar> utf_conv::UTF32ToUTF16(const basic_string<UnicodeChar32>& ustrSrc, vector<int>* pvcOffsets)
{
	basic_string<UnicodeChar> ustrRet;
	ustrRet.reserve(ustrSrc.size());
	if (pvcOffsets)
		(*pvcOffsets).reserve(ustrSrc.size());
	for (size_t i = 0; i < ustrSrc.size(); ++i) {
		UnicodeChar32 c = ustrSrc[i];
		if ((c >= 0x110000) || (c < 0)) {
			// error
			ustrRet.clear();
			if (pvcOffsets)
				(*pvcOffsets).clear();
			return ustrRet;
		}

		if (pvcOffsets)
			(*pvcOffsets).push_back((int)ustrRet.length());
		if (c <= 0xFFFF) {
			ustrRet.append(1, (UnicodeChar)c);
		}
		else {
			// split into 2
			// Store the leading surrogate char
			c -= 0x10000;
			ustrRet.append(1, UnicodeChar((c >> 10) + 0xD800));

			//  And then the trailing char.
			ustrRet.append(1, UnicodeChar((c & 0x3FF) + 0xDC00));
		}
	}

	return ustrRet;
}

int utf_conv::UTF16ToUTF8(UnicodeChar c, char* buffer, int buff_size)
{
	if (c < 0x80 && buff_size > 0)
	{
		buffer[0] = (char)c;
		return 1;
	}

	unsigned int charsEaten = 0;
	int len = UTF16ToUTF8(&c, 1, &charsEaten, buffer, buff_size, NULL, 0);
	if (len <= 0 || charsEaten != 1)
	{
		return 0;
	}
	return len;
}

void utf_conv::UTF16ToUTF8(UnicodeChar c, string& strRet)
{
	strRet = UTF16ToUTF8(c);
}

string utf_conv::UTF16ToUTF8(UnicodeChar c)
{
	string strRet;
	if (c < 0x80)
	{
		strRet.resize(1);
		strRet[0] = (char)c;
	}
	else
	{
		const int buff_size = 4;
		char szBuff[buff_size];
		unsigned int charsEaten = 0;
		int len = UTF16ToUTF8(&c, 1, &charsEaten, szBuff, buff_size, NULL, 0);
		if (!((len <= 0) || (charsEaten != 1)))
			strRet.assign(szBuff, len);
		else
			strRet.clear();
	}

	return strRet;
}

void utf_conv::UTF16ToUTF8(const basic_string<UnicodeChar>& ustrSrc, string& strRet)
{
	strRet = UTF16ToUTF8(ustrSrc);
}

string utf_conv::UTF16ToUTF8(const basic_string<UnicodeChar>& ustrSrc)
{
	string strRet;
	unsigned int srcCount = (unsigned int)ustrSrc.length();
	unsigned int charsEaten = 0;
	const UnicodeChar* srcData = ustrSrc.c_str();

	// get buffer size
	int buff_size = UTF16ToUTF8(srcData, srcCount, &charsEaten, NULL, 0, NULL, 0, false);
	if (!((buff_size <= 0) || (charsEaten != srcCount))) {
		strRet.resize(buff_size);
		if ((UTF16ToUTF8(srcData, srcCount, &charsEaten, &*strRet.begin(), buff_size, NULL, 0, false) <= 0) || (charsEaten != srcCount))
			strRet.clear();
	}

	return strRet;
}

void utf_conv::UTF8ToUTF16(const string& strSrc, basic_string<UnicodeChar>& ustrRet)
{
	ustrRet = UTF8ToUTF16(strSrc);
}

basic_string<UnicodeChar> utf_conv::UTF8ToUTF16(const string& strSrc)
{
	basic_string<UnicodeChar> ustrRet;
	unsigned int srcCount =  (unsigned int)strSrc.length();
	unsigned int bytesEaten = 0;
	const char* srcData = strSrc.c_str();

	// get buffer size
	int buff_size = UTF8ToUTF16(srcData, srcCount, &bytesEaten, NULL, 0, NULL, 0);
	if (!((buff_size <= 0) || (bytesEaten != srcCount))) {
		ustrRet.resize(buff_size);
		if ((UTF8ToUTF16(srcData, srcCount, &bytesEaten, &*ustrRet.begin(), buff_size, NULL, 0) != buff_size) || (bytesEaten != srcCount))
			ustrRet.clear();
	}

	return ustrRet;
}

void utf_conv::UTF8ToUTF16(const char* pszSrc, basic_string<UnicodeChar>& ustrRet)
{
	ustrRet = UTF8ToUTF16(pszSrc);
}

basic_string<UnicodeChar> utf_conv::UTF8ToUTF16(const char* pszSrc)
{
	basic_string<UnicodeChar> ustrRet;
	unsigned int srcCount = (unsigned int)strlen(pszSrc);
	unsigned int bytesEaten = 0;

	// get buffer size
	int buff_size = UTF8ToUTF16(pszSrc, srcCount, &bytesEaten, NULL, 0, NULL, 0);
	if (!((buff_size <= 0) || (bytesEaten != srcCount))) {
		ustrRet.resize(buff_size);
		if ((UTF8ToUTF16(pszSrc, srcCount, &bytesEaten, &*ustrRet.begin(), buff_size, NULL, 0) != buff_size) || (bytesEaten != srcCount))
			ustrRet.clear();
	}

	return ustrRet;
}

void utf_conv::UTF8ToUTF32(const string& strSrc, basic_string<UnicodeChar32>& ustrRet)
{
	ustrRet = UTF8ToUTF32(strSrc);
}

basic_string<UnicodeChar32> utf_conv::UTF8ToUTF32(const string& strSrc)
{
	basic_string<UnicodeChar32> ustrRet;
	unsigned int srcCount = (unsigned int)strSrc.length();
	unsigned int bytesEaten = 0;
	const char* srcData = strSrc.c_str();

	// get buffer size
	int buff_size = UTF8ToUTF32(srcData, srcCount, &bytesEaten, NULL, 0, NULL, 0);
	if (!((buff_size <= 0) || (bytesEaten != srcCount))) {
		ustrRet.resize(buff_size);
		if ((UTF8ToUTF32(srcData, srcCount, &bytesEaten, &*ustrRet.begin(), buff_size, NULL, 0) != buff_size) || (bytesEaten != srcCount))
			ustrRet.clear();
	}

	return ustrRet;
}

void utf_conv::UTF8ToUTF32(const char* pszSrc, basic_string<UnicodeChar32>& ustrRet)
{
	ustrRet = UTF8ToUTF32(pszSrc);
}

basic_string<UnicodeChar32> utf_conv::UTF8ToUTF32(const char* pszSrc)
{
	basic_string<UnicodeChar32> ustrRet;
	unsigned int srcCount = (unsigned int)strlen(pszSrc);
	unsigned int bytesEaten = 0;

	// get buffer size
	int buff_size = UTF8ToUTF32(pszSrc, srcCount, &bytesEaten, NULL, 0, NULL, 0);
	if (!((buff_size <= 0) || (bytesEaten != srcCount))) {
		ustrRet.resize(buff_size);
		if ((UTF8ToUTF32(pszSrc, srcCount, &bytesEaten, &*ustrRet.begin(), buff_size, NULL, 0) != buff_size) || (bytesEaten != srcCount))
			ustrRet.clear();
	}

	return ustrRet;
}

int utf_conv::UTF16ToUTF32(const basic_string<UnicodeChar>& ustrSrc, UnicodeChar32* pszRet, int buff_size)
{
	int outPos = 0;
	for (basic_string<UnicodeChar>::const_iterator iter = ustrSrc.begin(); iter != ustrSrc.end(); ++iter)
	{
		UnicodeChar32 ch = *iter;
		if (UD_IS_TRAIL_SURROGATE(ch))
		{
			ch = ' '; // error
		}
		else if (UD_IS_LEAD_SURROGATE(ch))
		{
			if (((iter + 1) == ustrSrc.end()) || (!UD_IS_TRAIL_SURROGATE(*(iter + 1))))
			{
				ch = ' '; // error
			}
			else
			{
				// Create the composite surrogate pair
				++iter;
				ch = ((ch - 0xD800) << 10) + ((*iter - 0xDC00) + 0x10000);
			}
		}

		if (outPos < buff_size)
			pszRet[outPos] = ch;

		++outPos;
	}

	return outPos;
}

void utf_conv::UTF16ToUTF32(const basic_string<UnicodeChar>& ustrSrc, basic_string<UnicodeChar32>& ustrRet)
{
	ustrRet = UTF16ToUTF32(ustrSrc);
}

basic_string<UnicodeChar32> utf_conv::UTF16ToUTF32(const basic_string<UnicodeChar>& ustrSrc)
{
	basic_string<UnicodeChar32> ustrRet;
	if (ustrSrc.empty())
		return ustrRet;

	ustrRet.resize(ustrSrc.length()); // note that the size of this string is guaranteed to be less or equal to the size of the input string

	int nSize = UTF16ToUTF32(ustrSrc, &*ustrRet.begin(), (int)ustrRet.size());
	ustrRet.resize(nSize);
	return ustrRet;
}
