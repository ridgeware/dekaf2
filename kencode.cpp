/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kencode.h"


namespace dekaf2 {



KString KDec::HTML(KStringView sIn)
{
	KString sRet;
	sRet.reserve(sIn.size());

	for (KStringView::const_iterator it = sIn.cbegin(), ie = sIn.cend(); it != ie; ++it)
	{
		if (*it != '&')
		{
			sRet += *it;
		}
		else
		{
			// decode one char
			if (++it != ie)
			{
				if (*it == '#')
				{
					if (++it != ie)
					{
						uint32_t iChar{0};

						if (*it == 'x' || *it == 'X')
						{
							// hex
							while (std::isxdigit(*it))
							{
								iChar *= 16;

								switch (*it)
								{
									case '0': case '1': case '2': case '3': case '4':
									case '5': case '6': case '7': case '8': case '9':
										iChar += *it - '0';
										break;
									case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
										iChar += *it - 'A';
										break;
									case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
										iChar += *it - 'a';
										break;
								}

								if (++it == ie)
								{
									break;
								}
							}
						}
						else
						{
							// decimal
							while (std::isdigit(*it))
							{
								iChar *= 10;
								iChar += *it - '0';

								if (++it == ie)
								{
									break;
								}
							}
						}
						// TODO write UTF8 converter
						sRet += iChar & 0x0ff;
//						sRet += ToUTF8(iChar);

						if (it != ie && *++it  == static_cast<KStringView::value_type>(';'))
						{
							++it;
						}
					}
				}
			}
		}
	}

	return sRet;
}

void KDec::HTMLInPlace(KString& sBuffer)
{
	KString sRet = HTML(sBuffer);
	sBuffer.swap(sRet);
}


} // of namespace dekaf2

