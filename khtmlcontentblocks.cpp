/*
 //-----------------------------------------------------------------------------//
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

#include "khtmlcontentblocks.h"
#include "kctype.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::ContentBlock(KStringView sContentBlock)
//-----------------------------------------------------------------------------
{
	// base version does nothing

} // ContentBlock

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Skeleton(char ch)
//-----------------------------------------------------------------------------
{
	// base version does nothing

} // Skeleton

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Skeleton(KStringView sSkeleton)
//-----------------------------------------------------------------------------
{
	// base version outputs to Skeleton(char)
	for (auto ch : sSkeleton)
	{
		Skeleton(ch);
	}

} // Skeleton

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Skeleton(const KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	KString sOutput;
	Object.Serialize(sOutput);
	Skeleton(sOutput);

} // Skeleton

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::FlushContentBlock()
//-----------------------------------------------------------------------------
{
	if (!m_sContentBlock.empty())
	{
		if (m_bHadTextContent)
		{
			ContentBlock(m_sContentBlock);
		}
		else
		{
			for (auto ch : m_sContentBlock)
			{
				Skeleton(ch);
			}
		}
		m_sContentBlock.clear();
		m_bHadTextContent = false;
	}

} // FlushContentBlock

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Object(KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	switch (Object.Type())
	{
		case KHTMLObject::TAG:
		{
			KHTMLTag& Tag = reinterpret_cast<KHTMLTag&>(Object);
			if (!Tag.IsInline())
			{
				FlushContentBlock();
				// and push the tag into the skeleton
				Skeleton(Object);
			}
			else
			{
				// push the inline tag into the content block
				Tag.Serialize(m_sContentBlock);
			}
			break;
		}

		case KHTMLObject::COMMENT:
			// the effect of this check is that we throw away comments inside of
			// content blocks with real content
			if (!m_bHadTextContent)
			{
				FlushContentBlock();
				Skeleton(Object);
			}
			break;

		default:
			FlushContentBlock();
			Skeleton(Object);
			break;
	}

} // Object

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Finished()
//-----------------------------------------------------------------------------
{
	FlushContentBlock();

} // Finished

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Content(char ch)
//-----------------------------------------------------------------------------
{
	if (!KASCII::kIsSpace(ch))
	{
		m_bHadTextContent = true;
	}
	
	// push the char into the content block
	m_sContentBlock += ch;

} // Content

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Script(char ch)
//-----------------------------------------------------------------------------
{
	m_sContentBlock += ch;

} // Script

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	Skeleton(ch);

} // Invalid

} // end of namespace dekaf2

