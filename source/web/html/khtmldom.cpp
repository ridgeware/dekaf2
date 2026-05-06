/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/web/html/khtmlentities.h>
#include <dekaf2/core/errors/kexception.h>

// set to 1 for debug output
#define DEKAF2_HTMLDOM_DEBUG 0

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
// File-private helpers used by khtml::SerializeNode().
// These mirror the encoding/quoting logic that today lives in the anonymous
// namespace of khtmlparser.cpp (EncodeMandatoryHTMLContent /
// EncodeMandatoryAttributeValue) and the lazy-quoting logic in
// KHTMLAttribute::Serialize. Replicated here on purpose to operate on POD
// values (KStringView) without going through the KHTMLAttribute / KHTMLText
// heap types — keeping the POD-serializer free of heap allocations.
//-----------------------------------------------------------------------------

namespace {

// quote-required chars — verbatim copy of KHTMLAttribute::s_NeedsQuotes.
constexpr KStringView s_PodNeedsQuoteChars { " \f\v\t\r\n\b\"'=<>`" };

template <typename Output>
void PodEncodeAttrValue(Output& Out, KStringView sValue, char chQuote)
{
	for (auto ch : sValue)
	{
		switch (ch)
		{
			case '"':  if (chQuote == '\'') Out += ch;  else Out += "&quot;"; break;
			case '&':                                        Out += "&amp;";  break;
			case '\'': if (chQuote == '"' ) Out += ch;  else Out += "&apos;"; break;
			case '<':                                        Out += "&lt;";   break;
			case '>':                                        Out += "&gt;";   break;
			default:                                         Out += ch;       break;
		}
	}
}

template <typename Output>
void PodEncodeHTMLContent(Output& Out, KStringView sContent)
{
	for (auto ch : sContent)
	{
		switch (ch)
		{
			case '&': Out += "&amp;"; break;
			case '<': Out += "&lt;";  break;
			case '>': Out += "&gt;";  break;
			default:  Out += ch;      break;
		}
	}
}

inline char PodDecideQuote(const khtml::AttrPOD* a) noexcept
{
	if (a->Quote != 0)
	{
		return a->Quote;
	}
	if (a->Value.find_first_of(s_PodNeedsQuoteChars) != KStringView::npos)
	{
		return '"';
	}
	return 0;
}

inline void PodSerializeAttr(KOutStream& Out, const khtml::AttrPOD* a)
{
	if (a->Name.empty())
	{
		return;
	}

	Out.Write(a->Name);

	if (!a->Value.empty())
	{
		Out.Write('=');

		const char chQuote = PodDecideQuote(a);

		if (chQuote)
		{
			Out.Write(chQuote);
		}

		if (!a->DoEscape)
		{
			Out.Write(a->Value);
		}
		else
		{
			PodEncodeAttrValue(Out, a->Value, chQuote);
		}

		if (chQuote)
		{
			Out.Write(chQuote);
		}
	}
}

inline void PodSerializeAttrs(KOutStream& Out, const khtml::NodePOD* pNode)
{
	for (const khtml::AttrPOD* a = pNode->FirstAttr; a != nullptr; a = a->Next)
	{
		Out.Write(' ');
		PodSerializeAttr(Out, a);
	}
}

// Recursive serializer; mirrors KHTMLElement::Print() one-to-one.
// Returns the value of bIsFirstAfterLinefeed at the end of emission for the
// outer recursion to chain properly.
bool PodSerializeNodeImpl(KOutStream& Out,
                          const khtml::NodePOD* pNode,
                          char chIndent,
                          uint16_t iIndent,
                          bool bIsFirstAfterLinefeed,
                          bool bIsInsideHead,
                          uint16_t iIsPreformatted)
{
	using TP = KHTMLObject::TagProperty;

	const bool bIsElement      = (pNode->Kind == khtml::NodeKind::Element);
	const TP   Props           = pNode->TagProps;
	const bool bIsStandalone   = bIsElement && KHTMLObject::IsStandalone   (Props);
	const bool bIsInline       = bIsElement && KHTMLObject::IsInline       (Props);
	const bool bIsInlineBlock  = bIsElement && KHTMLObject::IsInlineBlock  (Props);
	const bool bIsRoot         = bIsElement && pNode->Name.empty();
	const bool bHasOpening     = bIsElement && !pNode->Name.empty();
	bool       bLastWasSpace   = bIsFirstAfterLinefeed;

	auto PrintIndent = [&](uint16_t lvl)
	{
		if (!iIsPreformatted && bIsFirstAfterLinefeed)
		{
			if (chIndent)
			{
				for (; lvl-- > 0;)
				{
					Out.Write(chIndent);
				}
			}
			bIsFirstAfterLinefeed = false;
			bLastWasSpace         = KASCII::kIsSpace(chIndent);
		}
	};

	auto WriteLinefeed = [&]()
	{
		static constexpr KStringView sLineFeed { "\r\n" };
		Out.Write(sLineFeed);
		bIsFirstAfterLinefeed = true;
		bLastWasSpace         = true;
	};

	if (bHasOpening)
	{
		if (!bIsInline && !bIsFirstAfterLinefeed)
		{
			WriteLinefeed();
		}

		PrintIndent(iIndent);
		Out.Write('<');
		Out.Write(pNode->Name);
		PodSerializeAttrs(Out, pNode);

		if (bIsStandalone)
		{
			// pure HTML emits <tag> for standalone (not the XHTML <tag />)
			Out.Write('>');

			if (!bIsInline || pNode->Name == "br" || pNode->Name == "hr"
				|| (bIsInsideHead && KHTMLObject::IsNotInlineInHead(Props)))
			{
				WriteLinefeed();
				return true;
			}
			return false;
		}

		Out.Write('>');
		bLastWasSpace = false;

		if (!bIsInline || bIsInlineBlock)
		{
			if (pNode->Name == "head")
			{
				bIsInsideHead = true;
			}

			if (!iIsPreformatted && pNode->FirstChild != nullptr)
			{
				WriteLinefeed();
			}
		}

		if (KHTMLObject::IsPreformatted(Props)
			&& iIsPreformatted < std::numeric_limits<decltype(iIsPreformatted)>::max())
		{
			++iIsPreformatted;
		}
	}

	for (const khtml::NodePOD* c = pNode->FirstChild; c != nullptr; c = c->NextSibling)
	{
		// Element children recurse and continue the for-loop directly
		if (c->Kind == khtml::NodeKind::Element)
		{
			bIsFirstAfterLinefeed = PodSerializeNodeImpl(Out, c, chIndent,
				iIndent + (bIsRoot ? 0 : 1), bIsFirstAfterLinefeed, bIsInsideHead, iIsPreformatted);
			bLastWasSpace = bIsFirstAfterLinefeed;
			continue;
		}

		// PrintIndent before non-Element-leaves (mirror of KHTMLElement::Print)
		switch (c->Kind)
		{
			case khtml::NodeKind::Comment:
			case khtml::NodeKind::ProcessingInstruction:
			case khtml::NodeKind::DocumentType:
			case khtml::NodeKind::Text:
				PrintIndent(iIndent + (bIsRoot ? 0 : 1));
				break;
			case khtml::NodeKind::CData:
			case khtml::NodeKind::Element:
				// CData has no PrintIndent (in heap path: falls through 'default'
				// branch of the switch in KHTMLElement::Print).
				break;
		}

		// Emit the leaf body
		switch (c->Kind)
		{
			case khtml::NodeKind::Text:
			{
				const bool bDoNotEscape = (c->Flags & khtml::NodeFlag::TextDoNotEscape) != 0;
				const bool bSkipLoneSpace = !iIsPreformatted && bLastWasSpace && c->Name == " ";

				if (!bSkipLoneSpace)
				{
					if (bDoNotEscape) Out.Write(c->Name);
					else              PodEncodeHTMLContent(Out, c->Name);
				}
				break;
			}

			case khtml::NodeKind::Comment:
				Out.Write(KHTMLComment::s_sLeadIn);
				Out.Write(c->Name);
				Out.Write(KHTMLComment::s_sLeadOut);
				break;

			case khtml::NodeKind::CData:
				Out.Write(KHTMLCData::s_sLeadIn);
				Out.Write(c->Name);
				Out.Write(KHTMLCData::s_sLeadOut);
				break;

			case khtml::NodeKind::ProcessingInstruction:
				Out.Write(KHTMLProcessingInstruction::s_sLeadIn);
				Out.Write(c->Name);
				Out.Write(KHTMLProcessingInstruction::s_sLeadOut);
				break;

			case khtml::NodeKind::DocumentType:
				Out.Write(KHTMLDocumentType::s_sLeadIn);
				Out.Write(c->Name);
				Out.Write(KHTMLDocumentType::s_sLeadOut);
				break;

			case khtml::NodeKind::Element:
				// already handled above
				break;
		}

		// Post-emission housekeeping (linefeeds, last-char tracking)
		switch (c->Kind)
		{
			case khtml::NodeKind::Comment:
			case khtml::NodeKind::ProcessingInstruction:
			case khtml::NodeKind::DocumentType:
				WriteLinefeed();
				break;

			case khtml::NodeKind::Text:
				if (!iIsPreformatted && !c->Name.empty() && c->Name.back() == '\n')
				{
					bIsFirstAfterLinefeed = true;
				}
				break;

			case khtml::NodeKind::CData:
			case khtml::NodeKind::Element:
				break;
		}
	}

	if (bHasOpening)
	{
		if (pNode->FirstChild != nullptr)
		{
			if ((!bIsInline || bIsInlineBlock) && !iIsPreformatted)
			{
				if (!bIsFirstAfterLinefeed)
				{
					WriteLinefeed();
				}
			}

			if (bIsFirstAfterLinefeed)
			{
				PrintIndent(iIndent);
				bIsFirstAfterLinefeed = false;
			}
		}

		Out.Write(KStringView{"</"});
		Out.Write(pNode->Name);
		Out.Write('>');

		if (!bIsInline)
		{
			WriteLinefeed();
		}
	}

	return bIsFirstAfterLinefeed;
}

} // anonymous namespace

namespace khtml {

//-----------------------------------------------------------------------------
void SerializeNode(KOutStream& OutStream, const NodePOD* pNode, char chIndent)
//-----------------------------------------------------------------------------
{
	if (pNode == nullptr)
	{
		return;
	}

	PodSerializeNodeImpl(OutStream, pNode, chIndent,
		/*iIndent=*/             0,
		/*bIsFirstAfterLinefeed*/ true,
		/*bIsInsideHead=*/       false,
		/*iIsPreformatted=*/     0);

} // SerializeNode

//-----------------------------------------------------------------------------
bool PodRemoveTrailingWhitespace(NodePOD* pNode, bool bStopAtBlockElement)
//-----------------------------------------------------------------------------
{
	if (pNode == nullptr)
	{
		return false;
	}

	using TP = KHTMLObject::TagProperty;

	for (NodePOD* c = pNode->LastChild; c != nullptr;)
	{
		switch (c->Kind)
		{
			case NodeKind::Text:
			{
				// trim trailing whitespace from this text leaf
				KStringView text = c->Name;
				std::size_t i    = text.size();

				while (i > 0 && KASCII::kIsSpace(text[i - 1]))
				{
					--i;
				}

				if (i == 0)
				{
					// fully whitespace — drop the text node
					NodePOD* prev = c->PrevSibling;
					DetachChild(c);
					c = prev;
					continue;
				}

				c->Name = text.substr(0, i);
				return true;
			}

			case NodeKind::Element:
			{
				const TP Props = c->TagProps;

				if (bStopAtBlockElement && KHTMLObject::IsBlock(Props))
				{
					return true;
				}

				if (KHTMLObject::IsStandalone(Props) && c->Name != "br" && c->Name != "hr")
				{
					// hit non-whitespace embedded content (e.g. <img>)
					return true;
				}

				if (!KHTMLObject::IsStandalone(Props) && c->Name != "script")
				{
					if (PodRemoveTrailingWhitespace(c, bStopAtBlockElement))
					{
						return true;
					}
				}

				c = c->PrevSibling;
				break;
			}

			case NodeKind::Comment:
			case NodeKind::CData:
			case NodeKind::ProcessingInstruction:
			case NodeKind::DocumentType:
				// skip non-text, non-element nodes
				c = c->PrevSibling;
				break;
		}
	}

	return false;

} // PodRemoveTrailingWhitespace

} // namespace khtml

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(const KHTMLElement& other)
//-----------------------------------------------------------------------------
: KHTMLObject(other)
, m_Name(other.m_Name)
, m_Attributes(other.m_Attributes)
, m_Property(other.m_Property)
{
	for (auto& Child : other.m_Children)
	{
		m_Children.push_back(Child->Clone());
	}

} // copy ctor

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KHTMLTag Tag)
//-----------------------------------------------------------------------------
{
	m_Name       = std::move(Tag.MoveName());
	m_Attributes = std::move(Tag.MoveAttributes());
	m_Property   = GetTagProperty(m_Name);
	// we should check here if the attribute values are entity encoded, in
	// which case we should decode them - but we use this constructor only
	// during parsing of a DOM where the entity decode is active already
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KString sName, KHTMLAttributes Attributes)
//-----------------------------------------------------------------------------
: m_Name(std::move(sName))
, m_Attributes(std::move(Attributes))
, m_Property(GetTagProperty(m_Name))
{
} // ctor

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KString sName, KStringView sID, KStringView sClass)
//-----------------------------------------------------------------------------
: m_Name(std::move(sName))
, m_Property(GetTagProperty(m_Name))
{
	if (!sID.empty())
	{
		SetID(sID);
	}

	if (!sClass.empty())
	{
		SetClass(sClass);
	}

} // ctor

//-----------------------------------------------------------------------------
bool KHTMLElement::Parse(KStringView sInput, bool bDummyParam)
//-----------------------------------------------------------------------------
{
	// we always store unencoded content (no entities)
	m_Name = sInput;
	return true;
}

//-----------------------------------------------------------------------------
bool KHTMLElement::Print(KOutStream& OutStream, char chIndent, uint16_t iIndent, bool bIsFirstAfterLinefeed, bool bIsInsideHead, uint16_t iIsPreformatted) const
//-----------------------------------------------------------------------------
{
	// bIsFirstAfterLinefeed means: last output was a linefeed
	// returns bIsFirstAfterLinefeed

	auto bIsStandalone   = IsStandalone();
	auto bIsInline       = IsInline();
	auto bIsInlineBlock  = IsInlineBlock();
	bool bIsRoot         = m_Name.empty();
	bool bLastWasSpace   = bIsFirstAfterLinefeed;

#if DEKAF2_HTMLDOM_DEBUG
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
	kDebug(4, "up: <{}{}, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}, IsFirst={}", m_Name, '>', iIndent, bIsStandalone, bIsInline, bIsRoot, bIsFirstAfterLinefeed);
#else
	kDebug(4, "up: <{}>, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}, IsFirst={}", m_Name, iIndent, bIsStandalone, bIsInline, bIsRoot, bIsFirstAfterLinefeed);
#endif
#endif

	auto PrintIndent   = [&bIsFirstAfterLinefeed,&bLastWasSpace,chIndent,&OutStream,&iIsPreformatted](uint16_t iIndent)
	{
		if (!iIsPreformatted && bIsFirstAfterLinefeed)
		{
			if (chIndent)
			{
				for (;iIndent-- > 0;)
				{
					OutStream.Write(chIndent);
				}
			}
			bIsFirstAfterLinefeed = false;
			bLastWasSpace         = KASCII::kIsSpace(chIndent);
		}
	};

	auto WriteLinefeed = [&bIsFirstAfterLinefeed,&bLastWasSpace,&OutStream]()
	{
		static constexpr KStringView sLineFeed { "\r\n" };

		OutStream.Write(sLineFeed);
		bIsFirstAfterLinefeed = true;
		bLastWasSpace         = true;
	};

	if (!m_Name.empty())
	{
		if (!bIsInline && !bIsFirstAfterLinefeed)
		{
			WriteLinefeed();
		}

		PrintIndent(iIndent);
		OutStream.Write('<');
		OutStream.Write(m_Name);

		m_Attributes.Serialize(OutStream);

		if (bIsStandalone)
		{
#if DEKAF2_HTMLDOM_DEBUG
			kDebug(4, "down: <{}/>", m_Name);
#endif
			static constexpr bool bXHTML = false;

			if (bXHTML)
			{
				// XHTML requests (but not requires) a space before the closing slash
				// if used for standalone tags
				OutStream.Write(" />");
			}
			else
			{
				// pure HTML does neither want nor require a closing slash for
				// standalone tags
				OutStream.Write('>');
			}

			if (!bIsInline || m_Name.In("br,hr") || (bIsInsideHead && IsNotInlineInHead()))
			{
				WriteLinefeed();
				return true;
			}

			return false;
		}

		OutStream.Write('>');

		bLastWasSpace = false;

		if (!bIsInline || bIsInlineBlock)
		{
			if (m_Name == "head")
			{
				bIsInsideHead = true;
			}

			if (!iIsPreformatted && !m_Children.empty())
			{
				WriteLinefeed();
			}
		}

		// preformatted sections only start _after_ start tag emission, right here..
		// we also still write a line break after the tag because browsers remove the
		// first line break - this way a leading line break of the real preformatted
		// content gets honoured
		if (IsPreformatted() && iIsPreformatted < std::numeric_limits<decltype(iIsPreformatted)>::max())
		{
			// the above test for limits is only meant to avoid UB - if it overflows the DOM
			// has quite another problem, therefore we don't report it
			++iIsPreformatted;
		}
	}

	for (const auto& it : m_Children)
	{
#if DEKAF2_HTMLDOM_DEBUG
		kDebug(4, "child: {}", it->TypeName());
#endif
		auto iType { it->Type() };

		switch (iType)
		{
			case KHTMLElement::TYPE:
			{
				auto* Element = static_cast<KHTMLElement*>(it.get());
				// print KHTMLElements here, instead of calling Serialize() below
				bIsFirstAfterLinefeed = Element->Print(OutStream, chIndent, iIndent + (bIsRoot ? 0 : 1), bIsFirstAfterLinefeed, bIsInsideHead, iIsPreformatted);
				bLastWasSpace = bIsFirstAfterLinefeed;
			}
			continue;

			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
			case KHTMLText::TYPE:
				PrintIndent(iIndent + (bIsRoot ? 0 : 1));
				break;

			default:
				break;
		}

		// to visualize text elements in the output (->debug), set bFrameText to true
		static constexpr bool bFrameText = false;

		if (iType == KHTMLText::TYPE)
		{
			if (bFrameText)
			{
				// with framed output always print the full text element
				OutStream.Write('[');
				it->Serialize(OutStream);
			}
			else if (!iIsPreformatted && bLastWasSpace)
			{
				// do not print a whitespace-only text element in normal output mode
				// if preceded by whitespace
				const auto* TextElement = static_cast<const KHTMLText*>(it.get());

				if (TextElement->GetText() != " ")
				{
					it->Serialize(OutStream);
				}
			}
			else
			{
				it->Serialize(OutStream);
			}
		}
		else
		{
			it->Serialize(OutStream);
		}

		switch (iType)
		{
			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
				WriteLinefeed();
				break;

			case KHTMLText::TYPE:
			{
				if (bFrameText)
				{
					OutStream.Write(']');
				}

				auto* Element = static_cast<KHTMLText*>(it.get());

				if (!iIsPreformatted && !Element->GetText().empty() && Element->GetText().back() == '\n')
				{
					bIsFirstAfterLinefeed = true;
				}
			}
			break;
		}
	}

	if (!m_Name.empty())
	{
		if (!m_Children.empty())
		{
			if ((!bIsInline || bIsInlineBlock) && !iIsPreformatted)
			{
				if (!bIsFirstAfterLinefeed)
				{
					WriteLinefeed();
				}
			}

			if (bIsFirstAfterLinefeed)
			{
				PrintIndent(iIndent);
				bIsFirstAfterLinefeed = false;
			}
		}

		OutStream.Write("</");
		OutStream.Write(m_Name);
		OutStream.Write('>');

		if (!bIsInline) // do not test for bIsInlineBlock here, outside the InlineBlock it behaves like an Inline
		{
			WriteLinefeed();
		}
	}

#if DEKAF2_HTMLDOM_DEBUG
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
	kDebug(4, "down: </{}{}", m_Name, '>');
#else
	kDebug(4, "down: </{}>", m_Name);
#endif
#endif
	return bIsFirstAfterLinefeed;

} // Print

//-----------------------------------------------------------------------------
void KHTMLElement::Print(KStringRef& sOut, char chIndent, uint16_t iIndent) const
//-----------------------------------------------------------------------------
{
	KOutStringStream oss(sOut);
	Print(oss, chIndent, iIndent);
}

//-----------------------------------------------------------------------------
KString KHTMLElement::Print(char chIndent, uint16_t iIndent) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Print(sOut, chIndent, iIndent);
	return sOut;
}

//-----------------------------------------------------------------------------
void KHTMLElement::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	Print(OutStream);
}

//-----------------------------------------------------------------------------
void KHTMLElement::clear()
//-----------------------------------------------------------------------------
{
	m_Name.clear();
	m_Attributes.clear();
	m_Children.clear();
}

//-----------------------------------------------------------------------------
bool KHTMLElement::empty() const
//-----------------------------------------------------------------------------
{
	return m_Name.empty() && m_Children.empty();
}

//-----------------------------------------------------------------------------
KHTMLElement::self& KHTMLElement::AddText(KStringView sContent)
//-----------------------------------------------------------------------------
{
	if (!sContent.empty())
	{
		Add(KHTMLText(sContent, false));
	}
	return *this;

} // AddText

//-----------------------------------------------------------------------------
KHTMLElement::self& KHTMLElement::AddRawText(KStringView sContent)
//-----------------------------------------------------------------------------
{
	if (!sContent.empty())
	{
		// do not escape and do not merge with last text node - this is for scripts and bad code
		Add(KHTMLText(sContent, true));
	}
	return *this;
	
} // AddRawText

//-----------------------------------------------------------------------------
KHTMLText& KHTMLElement::Insert(iterator it, KHTMLText Object, Merge merge)
//-----------------------------------------------------------------------------
{
	if (!m_Children.empty() && merge != Merge::Not)
	{
		if ((merge & Merge::Before) && it != m_Children.begin())
		{
			// check for left-merge
			auto Prev = (it - 1)->get();

			if (Prev->Type() == KHTMLText::TYPE)
			{
				return AppendTextObject(static_cast<KHTMLText&>(*Prev), Object);
			}
		}

		if (merge & Merge::After && it != m_Children.end())
		{
			// check the right side as well
			auto Next = it->get();

			if (Next->Type() == KHTMLText::TYPE)
			{
				return PrependTextObject(static_cast<KHTMLText&>(*Next), Object);
			}
		}
	}

	// if we end up here we could not merge the text, and will though create
	// a new child
	if (Object.empty())
	{
		kDebug(1, "adding an empty KHTMLText object is considered harmful");
	}

	auto up = std::make_unique<KHTMLText>(std::move(Object));
	auto* p = up.get();
	m_Children.insert(it, std::move(up));
	return *p;

} // Insert (KHTMLText)

//-----------------------------------------------------------------------------
KHTMLText& KHTMLElement::Add(KHTMLText Object, Merge merge)
//-----------------------------------------------------------------------------
{
	if (!m_Children.empty() && merge & Merge::Before)
	{
		// check for left-merge
		auto Prev = m_Children.back().get();

		if (Prev->Type() == KHTMLText::TYPE)
		{
			return AppendTextObject(static_cast<KHTMLText&>(*Prev), Object);
		}
	}

	// if we end up here we could not merge the text, and will though create
	// a new child
	if (Object.empty())
	{
		kDebug(1, "adding an empty KHTMLText object is considered harmful");
	}

	auto up = std::make_unique<KHTMLText>(std::move(Object));
	auto* p = up.get();
	m_Children.push_back(std::move(up));
	return *p;

} // Add (KHTMLText)

//-----------------------------------------------------------------------------
KHTMLElement& KHTMLElement::MergeTextElements(bool bHierarchically)
//-----------------------------------------------------------------------------
{
	KHTMLText* LastTextElement { nullptr };

	for (auto it = m_Children.begin(); it != m_Children.end();)
	{
		auto Child = it->get();

		switch (Child->Type())
		{
			case KHTMLText::TYPE:
				if (LastTextElement)
				{
					AppendTextObject(*LastTextElement, static_cast<KHTMLText&>(*Child));
					// remove the merged element
					it = m_Children.erase(it);
				}
				else
				{
					LastTextElement = static_cast<KHTMLText*>(Child);
					++it;
				}
				break;

			case KHTMLElement::TYPE:
				if (bHierarchically)
				{
					static_cast<KHTMLElement*>(Child)->MergeTextElements(bHierarchically);
				}
				DEKAF2_FALLTHROUGH;
			default:
				LastTextElement = nullptr;
				++it;
				break;
		}
	}

	return *this;

} // MergeTextElements

//-----------------------------------------------------------------------------
bool KHTMLElement::RemoveLeadingWhitespace(bool bStopAtBlockElement)
//-----------------------------------------------------------------------------
{
	for (auto it = m_Children.begin(); it != m_Children.end();)
	{
		auto Child = it->get();

		switch (Child->Type())
		{
			case KHTMLText::TYPE:
			{
				auto* Text = static_cast<KHTMLText*>(Child);

				Text->TrimLeft();

				if (Text->empty())
				{
					it = m_Children.erase(it);
				}
				else
				{
					// hit non-whitespace text
					return true;
				}
			}
			break;

			case KHTMLElement::TYPE:
			{
				auto* Element = static_cast<KHTMLElement*>(Child);

				if (bStopAtBlockElement && Element->IsBlock())
				{
					return true;
				}

				if (Element->IsStandalone() && !Element->GetName().In("br,hr"))
				{
					// hit non-whitespace content (an img or input element e.g.)
					return true;
				}

				if (!Element->IsStandalone() && Element->GetName() != "script")
				{
					if (Element->RemoveLeadingWhitespace(bStopAtBlockElement))
					{
						return true;
					}
				}
				++it;
			}
			break;

			default:
				// skip comment nodes etc.
				++it;
				break;
		}
	}

	return false;

} // RemoveLeadingWhitespace

//-----------------------------------------------------------------------------
bool KHTMLElement::RemoveTrailingWhitespace(bool bStopAtBlockElement)
//-----------------------------------------------------------------------------
{
	for (auto it = m_Children.end(); it != m_Children.begin();)
	{
		auto Child = (--it)->get();

		switch (Child->Type())
		{
			case KHTMLText::TYPE:
			{
				auto* Text = static_cast<KHTMLText*>(Child);

				Text->TrimRight();

				if (Text->empty())
				{
					it = m_Children.erase(it);
				}
				else
				{
					// hit non-whitespace text
					return true;
				}
			}
				break;

			case KHTMLElement::TYPE:
			{
				auto* Element = static_cast<KHTMLElement*>(Child);

				if (bStopAtBlockElement && Element->IsBlock())
				{
					return true;
				}

				if (Element->IsStandalone() && !Element->GetName().In("br,hr"))
				{
					// hit non-whitespace content (an img or input element e.g.)
					return true;
				}

				if (!Element->IsStandalone() && Element->GetName() != "script")
				{
					if (Element->RemoveTrailingWhitespace(bStopAtBlockElement))
					{
						return true;
					}
				}
			}
				break;

			default:
				// skip comment nodes etc.
				break;
		}
	}

	return false;

} // RemoveTrailingWhitespace

//-----------------------------------------------------------------------------
KHTMLElement::self& KHTMLElement::SetBoolAttribute(KString sName, bool bYesNo)
//-----------------------------------------------------------------------------
{
	if (IsBooleanAttribute(sName))
	{
		if (bYesNo)
		{
			m_Attributes.Set(std::move(sName), "");
		}
		else
		{
			RemoveAttribute(sName);
		}
	}
	else
	{
		m_Attributes.Set(std::move(sName), bYesNo ? "true" : "false");
	}
	return *this;

} // SetBoolAttribute

//-----------------------------------------------------------------------------
KHTMLElement::self& KHTMLElement::SetAttribute(KString sName, KString sValue, bool bRemoveIfEmptyValue, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	if (bRemoveIfEmptyValue && sValue.empty())
	{
		m_Attributes.Remove(sName);
	}
	else
	{
		m_Attributes.Set(std::move(sName), std::move(sValue), '"', bDoNotEscape);
	}
	return *this;

} // SetAttribute

//-----------------------------------------------------------------------------
KHTML::KHTML()
//-----------------------------------------------------------------------------
{
	EmitEntitiesAsUTF8();
	PodResetTree();

} // ctor

//-----------------------------------------------------------------------------
KHTML::~KHTML()
//-----------------------------------------------------------------------------
{
} // dtor

//-----------------------------------------------------------------------------
void KHTML::PodResetTree()
//-----------------------------------------------------------------------------
{
	m_Arena.clear();
	m_pPodRoot = khtml::CreateNode(m_Arena, khtml::NodeKind::Element);
	m_PodHierarchy.clear();
	m_PodHierarchy.push_back(m_pPodRoot);

} // PodResetTree

//-----------------------------------------------------------------------------
khtml::NodePOD* KHTML::PodAddElement(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	auto* pNode     = khtml::CreateNode(m_Arena, khtml::NodeKind::Element);
	pNode->Name     = m_Arena.AllocateString(Tag.GetName());
	pNode->TagProps = KHTMLObject::GetTagProperty(Tag.GetName());

	for (const auto& Attr : Tag.GetAttributes())
	{
		khtml::AppendAttr(pNode,
			khtml::CreateAttr(m_Arena,
				Attr.GetName(),
				Attr.GetValue(),
				Attr.GetQuote(),
				!Attr.IsEntityEncoded()));
	}

	khtml::AppendChild(m_PodHierarchy.back(), pNode);
	return pNode;

} // PodAddElement

//-----------------------------------------------------------------------------
void KHTML::PodAddStringNode(khtml::NodeKind kind, const KHTMLStringObject& Object)
//-----------------------------------------------------------------------------
{
	auto* pNode = khtml::CreateNode(m_Arena, kind);
	pNode->Name = m_Arena.AllocateString(Object.sValue);
	khtml::AppendChild(m_PodHierarchy.back(), pNode);

} // PodAddStringNode

//-----------------------------------------------------------------------------
void KHTML::PodAddTextLeaf(KStringView sContent, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	auto* pNode = khtml::CreateNode(m_Arena, khtml::NodeKind::Text);
	pNode->Name = m_Arena.AllocateString(sContent);

	if (bDoNotEscape)
	{
		pNode->Flags |= khtml::NodeFlag::TextDoNotEscape;
	}

	khtml::AppendChild(m_PodHierarchy.back(), pNode);

} // PodAddTextLeaf

//-----------------------------------------------------------------------------
// prefer the arena-backed POD shadow tree when it has been built
// by the parser (parse-path). For DOM-build path (KHTML::Add(), DOM().Add()
// etc.), the POD tree stays empty and we transparently fall back to the
// heap DOM serializer. Equivalence of both paths for parser inputs is
// guaranteed by the "KHTML POD-vs-heap serialization equivalence" test.
//-----------------------------------------------------------------------------
bool KHTML::PodHasContent() const
//-----------------------------------------------------------------------------
{
	return m_pPodRoot != nullptr && m_pPodRoot->FirstChild != nullptr;
}

namespace {

//-----------------------------------------------------------------------------
// Build a heap-DOM subtree under 'parent' that mirrors the children of POD
// node 'pPod'. Used by KHTML::MaterializePodToHeap() to reconstruct the
// heap DOM lazily on first DOM() access. Walks pPod->FirstChild ... NextSibling
// and dispatches by NodeKind, calling the same KHTMLElement::Add* methods
// that the parser would have called in Object()/FlushText() before
//-----------------------------------------------------------------------------
void MaterializePodChildren(KHTMLElement& parent, const khtml::NodePOD* pPod)
//-----------------------------------------------------------------------------
{
	if (pPod == nullptr) return;

	for (const auto* c = pPod->FirstChild; c != nullptr; c = c->NextSibling)
	{
		switch (c->Kind)
		{
			case khtml::NodeKind::Element:
			{
				KHTMLAttributes Attributes;
				for (const auto* pAttr = c->FirstAttr; pAttr != nullptr; pAttr = pAttr->Next)
				{
					Attributes.Set(KHTMLAttribute(KString(pAttr->Name),
					                              KString(pAttr->Value),
					                              pAttr->Quote ? pAttr->Quote : '"',
					                              !pAttr->DoEscape));
				}
				auto& Child = parent.Add(KHTMLElement(KString(c->Name), std::move(Attributes)));
				MaterializePodChildren(Child, c);
				break;
			}

			case khtml::NodeKind::Text:
			{
				const bool bDoNotEscape =
					(c->Flags & khtml::NodeFlag::TextDoNotEscape) != 0;
				if (bDoNotEscape) parent.AddRawText(c->Name);
				else              parent.AddText   (c->Name);
				break;
			}

			case khtml::NodeKind::Comment:
				parent.Add(KHTMLComment(KString(c->Name)));
				break;

			case khtml::NodeKind::CData:
				parent.Add(KHTMLCData(KString(c->Name)));
				break;

			case khtml::NodeKind::ProcessingInstruction:
				parent.Add(KHTMLProcessingInstruction(KString(c->Name)));
				break;

			case khtml::NodeKind::DocumentType:
				parent.Add(KHTMLDocumentType(KString(c->Name)));
				break;
		}
	}

} // MaterializePodChildren

} // anonymous namespace

//-----------------------------------------------------------------------------
// Build the heap DOM (m_Root) from the POD shadow tree on demand. Called
// transparently by DOM() when a caller requests the heap DOM and m_Root is
// still empty.
//-----------------------------------------------------------------------------
void KHTML::MaterializePodToHeap() const
//-----------------------------------------------------------------------------
{
	if (m_pPodRoot == nullptr)               return;
	if (m_pPodRoot->FirstChild == nullptr)   return;
	if (!m_Root.GetChildren().empty())       return;

	MaterializePodChildren(m_Root, m_pPodRoot);

} // MaterializePodToHeap

//-----------------------------------------------------------------------------
void KHTML::Serialize(KOutStream& Stream, char chIndent) const
//-----------------------------------------------------------------------------
{
	if (PodHasContent())
	{
		khtml::SerializeNode(Stream, m_pPodRoot, chIndent);
	}
	else
	{
		m_Root.Print(Stream, chIndent);
	}
}

//-----------------------------------------------------------------------------
void KHTML::Serialize(KStringRef& sOut, char chIndent) const
//-----------------------------------------------------------------------------
{
	if (PodHasContent())
	{
		KOutStringStream oss(sOut);
		khtml::SerializeNode(oss, m_pPodRoot, chIndent);
	}
	else
	{
		m_Root.Print(sOut, chIndent);
	}
}

//-----------------------------------------------------------------------------
KString KHTML::Serialize(char chIndent) const
//-----------------------------------------------------------------------------
{
	if (PodHasContent())
	{
		KString sOut;
		KOutStringStream oss(sOut);
		khtml::SerializeNode(oss, m_pPodRoot, chIndent);
		return sOut;
	}
	return m_Root.Print(chIndent);
}

//-----------------------------------------------------------------------------
void KHTML::clear()
//-----------------------------------------------------------------------------
{
	m_Root.clear();
	PodResetTree();
	m_sContent.clear();
	ClearError();
	m_Issues.clear();
	m_bLastWasSpace = true;
	m_bDoNotEscape  = false;
	m_bPreformatted = 0;

} // clear

//-----------------------------------------------------------------------------
void KHTML::FlushText()
//-----------------------------------------------------------------------------
{
	if (!m_sContent.empty())
	{
		const bool bDoNotEscape = m_bDoNotEscape;

		// phase 2c-i: write text only into the arena-backed POD shadow tree.
		// the heap DOM is no longer populated during parse — it is materialized
		// lazily on first DOM() access.
		PodAddTextLeaf(m_sContent, bDoNotEscape);

		m_sContent.clear();
		m_bDoNotEscape = false;
		// do not reset m_bLastWasSpace - we only clear it when descending from a block element
	}

} // FlushText

//-----------------------------------------------------------------------------
void KHTML::SetIssue(KString sIssue)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HTMLDOM_DEBUG
	kDebug(1, sIssue);
#endif
	m_Issues.push_back(std::move(sIssue));

} // SetIssue

//-----------------------------------------------------------------------------
void KHTML::Object(KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	// the parser writes only into the arena-backed POD shadow
	// tree. m_Root / heap DOM stay empty during parse and are materialized
	// lazily on first DOM() access. All hierarchy bookkeeping below operates
	// on m_PodHierarchy; reads (Name, IsInline, etc.) come from the POD
	// node's cached TagProps and Name fields.

	FlushText();

	switch (Object.Type())
	{
		case KHTMLTag::TYPE:
		{
			auto& Tag              = static_cast<KHTMLTag&>(Object);
			auto  TagProps         = KHTMLObject::GetTagProperty (Tag.GetName());
			auto  bIsStandalone    = KHTMLObject::IsStandalone   (TagProps);
			auto  bIsInline        = KHTMLObject::IsInline       (TagProps);
			auto  bIsInlineBlock   = KHTMLObject::IsInlineBlock  (TagProps);
			auto  bIsPreformatted  = KHTMLObject::IsPreformatted (TagProps);
			auto  iHierarchyLevels = m_PodHierarchy.size();

			if (Tag.IsClosing())
			{
				if (!Tag.GetAttributes().empty())
				{
					SetIssue(kFormat("invalid html - closing tag has attributes: {}", Tag.ToString()));
				}
				
				if (iHierarchyLevels > 0 && bIsStandalone)
				{
					// A standalone tag with a closer mark (</img>)
					// This is already invalid html, but because xhtml would allow an <img></img>
					// sequence, we check if we had that tag opened before and silently ignore
					// the closer. If the tag was not opened before, we treat it as a new standalone
					// (the author most probably had mistaken <tag/> with </tag>).

					const auto* pLastChild = m_PodHierarchy.back()->LastChild;

					if (   pLastChild == nullptr
						|| pLastChild->Kind != khtml::NodeKind::Element
						|| pLastChild->Name != Tag.GetName())
					{
						SetIssue(kFormat("invalid html - standalone tag closed without immediately opening it - treating it as a new standalone: {}", Tag.ToString()));
						PodAddElement(Tag);  // standalone — no push into POD hierarchy

						// is this content ("phrasing context")?
						if (bIsInline && Tag.GetName() != "br") // we treat <br> like a space
						{
							m_bLastWasSpace = false;
						}
						else
						{
							m_bLastWasSpace = true;
						}
					}
					else
					{
						// this is a standalone that was closed in XHTML manners.. do not err out,
						// instead maybe notify for sanity checks
					}
				}
				else if (iHierarchyLevels > 1) // normal closing tag
				{
					if (m_PodHierarchy.back()->Name == Tag.GetName())
					{
						if (bIsInlineBlock)
						{
							// as in a block context, remove inner whitespaces
							khtml::PodRemoveTrailingWhitespace(m_PodHierarchy.back());
							// otherwise behave like inline
						}
						else if (bIsInline)
						{
							// this is content ("phrasing context")
						}
						else
						{
							khtml::PodRemoveTrailingWhitespace(m_PodHierarchy.back());
							m_bLastWasSpace = true;

							if (bIsPreformatted && m_bPreformatted)
							{
								--m_bPreformatted;
							}
						}

						m_PodHierarchy.pop_back();
					}
					else
					{
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
						SetIssue(kFormat("invalid html - start and end tag differ: <{}{} -> </{}{}", m_PodHierarchy.back()->Name, '>', Tag.GetName(), '>'));
#else
						SetIssue(kFormat("invalid html - start and end tag differ: <{}> -> </{}>", m_PodHierarchy.back()->Name, Tag.GetName()));
#endif

						// now try to resync
						auto iMaxAutoClose = m_iMaxAutoCloseLevels;
						auto iCurrentLevel = iHierarchyLevels - 1;

						for(;;)
						{
							if (!iMaxAutoClose--)
							{
#if DEKAF2_HTMLDOM_DEBUG
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
								kDebug(2, "could not resync, max auto close levels = {}, will drop </{}{}", m_iMaxAutoCloseLevels, Tag.Name, '>');
#else
								kDebug(2, "could not resync, max auto close levels = {}, will drop </{}>", m_iMaxAutoCloseLevels, Tag.Name);
#endif
#endif
								break;
							}

							if (iCurrentLevel-- < 2)
							{
#if DEKAF2_HTMLDOM_DEBUG
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
								kDebug(2, "could not resync, reached root during descent, will drop </{}{}", Tag.Name, '>');
#else
								kDebug(2, "could not resync, reached root during descent, will drop </{}>", Tag.Name);
#endif
#endif
								break;
							}

							if (m_PodHierarchy[iCurrentLevel]->Name == Tag.GetName())
							{
#if DEKAF2_HTMLDOM_DEBUG
								kDebug(2, "resync after {} descents", m_PodHierarchy.size() - 1 - iCurrentLevel);
#endif
								while (m_PodHierarchy.size() > iCurrentLevel)
								{
									auto* pBack = m_PodHierarchy.back();

									if (!KHTMLObject::IsInline(pBack->TagProps))
									{
										// this is a block element
										khtml::PodRemoveTrailingWhitespace(pBack);
										m_bLastWasSpace = true;

										if (KHTMLObject::IsPreformattedTag(pBack->Name) && m_bPreformatted)
										{
											--m_bPreformatted;
										}
									}

									m_PodHierarchy.pop_back();
								}

								break;
							}
						}

					}
				}
				else
				{
					SetError(kFormat("invalid html - underflow at: {}", Tag.ToString()));
					return;
				}
			}
			else // opening tag or standalone
			{
				if (!bIsInline)
				{
					khtml::PodRemoveTrailingWhitespace(m_PodHierarchy.back());
				}

				auto* pPod = PodAddElement(Tag);

				// we check both the standalone flag from parsing, and the preset list
				// of standalone tags - HTML when not XHTML has missing standalone flags: <link/> vs <link>
				// and we also want to repair erroneous standalone tags like <video/> into <video></video>
				if (!bIsStandalone && !Tag.IsStandalone())
				{
					// get one level deeper
					m_PodHierarchy.push_back(pPod);

					if (!KHTMLObject::IsInline(pPod->TagProps))
					{
						m_bLastWasSpace = true;

						if (KHTMLObject::IsPreformattedTag(pPod->Name))
						{
							++m_bPreformatted;
						}
					}
				}
				else
				{
					if (Tag.GetName() == "br")
					{
						// we treat <br> like a space
						m_bLastWasSpace = true;
					}
					else if (bIsStandalone && bIsInline)
					{
						// a standalone inline tag is normally treated like phrasing content
						m_bLastWasSpace = false;
					}
				}
			}
		}
		break;

		case KHTMLComment::TYPE:
		{
			auto& Obj = static_cast<KHTMLComment&>(Object);
			PodAddStringNode(khtml::NodeKind::Comment, Obj);
		}
		break;

		case KHTMLCData::TYPE:
		{
			auto& Obj = static_cast<KHTMLCData&>(Object);
			PodAddStringNode(khtml::NodeKind::CData, Obj);
		}
		break;

		case KHTMLProcessingInstruction::TYPE:
		{
			auto& Obj = static_cast<KHTMLProcessingInstruction&>(Object);
			PodAddStringNode(khtml::NodeKind::ProcessingInstruction, Obj);
		}
		break;

		case KHTMLDocumentType::TYPE:
		{
			auto& Obj = static_cast<KHTMLDocumentType&>(Object);
			PodAddStringNode(khtml::NodeKind::DocumentType, Obj);
		}
		break;
	}

} // Object

//-----------------------------------------------------------------------------
void KHTML::Finished()
//-----------------------------------------------------------------------------
{
	FlushText();

	if (m_PodHierarchy.size() > 1)
	{
		SetIssue(kFormat("unbalanced HTML - still {} nodes open at end", m_PodHierarchy.size() - 1));

		// close all open nodes..
		while (m_PodHierarchy.size() > 1)
		{
			auto* pBack = m_PodHierarchy.back();

			if (!KHTMLObject::IsInline(pBack->TagProps))
			{
				// this is a block element
				khtml::PodRemoveTrailingWhitespace(pBack);
			}
			m_PodHierarchy.pop_back();
		}
	}

	if (m_PodHierarchy.size() == 1)
	{
		khtml::PodRemoveTrailingWhitespace(m_pPodRoot);
	}

} // Finished

//-----------------------------------------------------------------------------
void KHTML::Content(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bPreformatted)
	{
		if (KASCII::kIsSpace(ch))
		{
			if (m_bLastWasSpace)
			{
				return;
			}
			m_bLastWasSpace = true;
			ch = ' ';
		}
		else
		{
			m_bLastWasSpace = false;
		}
	}

	m_sContent += ch;

} // Content

//-----------------------------------------------------------------------------
void KHTML::Script(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bDoNotEscape)
	{
		// if we had already valid text, flush it now
		FlushText();
	}

	m_bDoNotEscape = true;
	// do not collapse white space
	m_sContent += ch;

} // Script

//-----------------------------------------------------------------------------
void KHTML::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bDoNotEscape)
	{
		// if we had already valid text, flush it now
		FlushText();
	}

	m_bDoNotEscape = true;
	// do not collapse white space
	m_sContent += ch;

} // Invalid

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::size_t KHTMLElement::TYPE;
constexpr KStringView KHTMLElement::s_sObjectName;
#endif

DEKAF2_NAMESPACE_END
