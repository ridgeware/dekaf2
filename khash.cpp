/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2020, Ridgeware, Inc.
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
 //
 */

#include "khash.h"
#include "kcasestring.h"
#include <array>

namespace dekaf2 {

//---------------------------------------------------------------------------
bool KHash::Update(KStringView::value_type chInput)
//---------------------------------------------------------------------------
{
    m_iHash = kHash(chInput, m_iHash);
    m_bUpdated = true;
    return true;
}

//---------------------------------------------------------------------------
bool KHash::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
    if (!sInput.empty())
    {
        m_iHash = kHash(sInput.data(), sInput.size(), m_iHash);
        m_bUpdated = true;
    }
    return true;
}

//---------------------------------------------------------------------------
bool KHash::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
    enum { BLOCKSIZE = 4096 };
    std::array<char, BLOCKSIZE> Buffer;

    for (;;)
    {
        auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());
        Update(KStringView(Buffer.data(), iReadChunk));
        if (iReadChunk < BLOCKSIZE)
        {
            return true;
        }
    }

} // Update

//---------------------------------------------------------------------------
bool KCaseHash::Update(KStringView::value_type chInput)
//---------------------------------------------------------------------------
{
    m_iHash = kHash(KASCII::kToLower(chInput), m_iHash);
    m_bUpdated = true;
    return true;
}

//---------------------------------------------------------------------------
bool KCaseHash::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
    if (!sInput.empty())
    {
        for (auto ch : sInput)
        {
            m_iHash = kHash(KASCII::kToLower(ch), m_iHash);
        }
        m_bUpdated = true;
    }
    return true;
}

//---------------------------------------------------------------------------
bool KCaseHash::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
    enum { BLOCKSIZE = 4096 };
    std::array<char, BLOCKSIZE> Buffer;

    for (;;)
    {
        auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());
        Update(KStringView(Buffer.data(), iReadChunk));
        if (iReadChunk < BLOCKSIZE)
        {
            return true;
        }
    }

} // Update

} // end of namespace dekaf2



