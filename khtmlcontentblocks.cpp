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
void KHTMLContentBlocks::BlockContent::FlushText()
//-----------------------------------------------------------------------------
{
	if (!m_sContent.empty())
	{
		m_Content.push_back(std::make_unique<KHTMLText>(m_sContent));
		m_sContent.clear();
	}
}

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::BlockContent::Text(char ch)
//-----------------------------------------------------------------------------
{
	if (!KASCII::kIsSpace(ch))
	{
		m_bHadTextContent = true;
	}
	m_sContent += ch;
}

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::BlockContent::InlineTag(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	FlushText();
	m_Content.push_back(std::make_unique<KHTMLTag>(Tag));
}

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::BlockContent::clear()
//-----------------------------------------------------------------------------
{
	m_Content.clear();
	m_sContent.clear();
	m_bHadTextContent = false;
}

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::BlockContent::Serialize(KString& sOut) const
//-----------------------------------------------------------------------------
{
	for (const auto& it : m_Content)
	{
		it->Serialize(sOut);
	}
}

//-----------------------------------------------------------------------------
KString KHTMLContentBlocks::BlockContent::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut);
	return sOut;
}

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::ContentBlock(BlockContent& Block)
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
	m_BlockContent.Completed();

	if (!m_BlockContent.empty())
	{
		if (m_BlockContent.HadTextContent())
		{
			ContentBlock(m_BlockContent);
		}
		else
		{
			KString sSerialized;
			m_BlockContent.Serialize(sSerialized);
			Skeleton(sSerialized);
		}

		m_BlockContent.clear();
	}

} // FlushContentBlock

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Object(KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	switch (Object.Type())
	{
		case KHTMLTag::TYPE:
		{
			auto& Tag = reinterpret_cast<KHTMLTag&>(Object);

			if (!Tag.IsInline())
			{
				FlushContentBlock();
				// and push the tag into the skeleton
				Skeleton(Object);
			}
			else
			{
				// push the inline tag into the content block
				m_BlockContent.InlineTag(Tag);
			}
			break;
		}

		case KHTMLComment::TYPE:
			// the effect of this check is that we throw away comments inside of
			// content blocks with real content
			if (!m_BlockContent.HadTextContent())
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
	m_BlockContent.Text(ch);

} // Content

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Script(char ch)
//-----------------------------------------------------------------------------
{
	m_BlockContent.NoText(ch);

} // Script

//-----------------------------------------------------------------------------
void KHTMLContentBlocks::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	Skeleton(ch);

} // Invalid

} // end of namespace dekaf2

