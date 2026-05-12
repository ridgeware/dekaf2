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
//-----------------------------------------------------------------------------

namespace {

// quote-required chars — matches the heap-DOM era KHTMLAttribute::s_NeedsQuotes.
constexpr KStringView s_PodNeedsQuoteChars { " \f\v\t\r\n\b\"'=<>`" };

//-----------------------------------------------------------------------------
template <typename Output>
void PodEncodeAttrValue(Output& Out, KStringView sValue, char chQuote)
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
template <typename Output>
void PodEncodeHTMLContent(Output& Out, KStringView sContent)
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
inline char PodDecideQuote(const khtml::AttrPOD* a) noexcept
//-----------------------------------------------------------------------------
{
	if (a->Quote() != 0)
	{
		return a->Quote();
	}

	// TODO switch to KFindSetOfChars
	if (a->Value().find_first_of(s_PodNeedsQuoteChars) != KStringView::npos)
	{
		return '"';
	}

	return 0;
}

//-----------------------------------------------------------------------------
inline void PodSerializeAttr(KOutStream& Out, const khtml::AttrPOD* a)
//-----------------------------------------------------------------------------
{
	if (a->Name().empty())
	{
		return;
	}

	Out.Write(a->Name());

	if (!a->Value().empty())
	{
		Out.Write('=');

		const char chQuote = PodDecideQuote(a);

		if (chQuote)
		{
			Out.Write(chQuote);
		}

		if (!a->DoEscape())
		{
			Out.Write(a->Value());
		}
		else
		{
			PodEncodeAttrValue(Out, a->Value(), chQuote);
		}

		if (chQuote)
		{
			Out.Write(chQuote);
		}
	}
}

//-----------------------------------------------------------------------------
// Emit all attributes of pNode, sorted alphabetically by name. The
// alphabetical order matches the heap-DOM era's std::set-based container,
// keeping byte-identical output between parsed and built documents (the
// parser's KHTMLAttributes already came out alphabetical too).
//
// Buffer is stack-allocated; elements with more than s_iMaxAttrs
// attributes serialise in insertion order (extremely rare for real HTML —
// we don't allocate a fallback heap buffer for that).
//-----------------------------------------------------------------------------
inline void PodSerializeAttrs(KOutStream& Out, const khtml::NodePOD* pNode)
//-----------------------------------------------------------------------------
{
	static constexpr std::size_t s_iMaxAttrs = 32;
	std::array<const khtml::AttrPOD*, s_iMaxAttrs> buf {};
	std::size_t n         = 0;
	bool        bOverflow = false;

	for (const khtml::AttrPOD* a = pNode->FirstAttr(); a != nullptr; a = a->Next())
	{
		if (n < buf.size())
		{
			buf[n++] = a;
		}
		else
		{
			bOverflow = true;
			break;
		}
	}

	if (!bOverflow)
	{
		const auto itEnd = buf.begin() + n;
		std::sort(buf.begin(), itEnd,
			[](const khtml::AttrPOD* x, const khtml::AttrPOD* y)
			{
				return x->Name() < y->Name();
			});
		std::for_each(buf.begin(), itEnd,
			[&Out](const khtml::AttrPOD* a)
			{
				Out.Write(' ');
				PodSerializeAttr(Out, a);
			});
	}
	else
	{
		// rare path — preserve insertion order
		for (const khtml::AttrPOD* a = pNode->FirstAttr(); a != nullptr; a = a->Next())
		{
			Out.Write(' ');
			PodSerializeAttr(Out, a);
		}
	}
}

//-----------------------------------------------------------------------------
// Recursive serializer. Returns the value of bIsFirstAfterLinefeed at the
// end of emission for the outer recursion to chain properly.
bool PodSerializeNodeImpl(KOutStream& Out,
                          const khtml::NodePOD* pNode,
                          char chIndent,
                          uint16_t iIndent,
                          bool bIsFirstAfterLinefeed,
                          bool bIsInsideHead,
                          uint16_t iIsPreformatted)
//-----------------------------------------------------------------------------
{
	using TP = KHTMLObject::TagProperty;

	const bool bIsElement      = (pNode->Kind() == khtml::NodeKind::Element);
	const TP   Props           = pNode->TagProps();
	const bool bIsStandalone   = bIsElement && KHTMLObject::IsStandalone   (Props);
	const bool bIsInline       = bIsElement && KHTMLObject::IsInline       (Props);
	const bool bIsInlineBlock  = bIsElement && KHTMLObject::IsInlineBlock  (Props);
	const bool bIsRootLikeDoc  = (pNode->Kind() == khtml::NodeKind::Document)
	                          || (bIsElement && pNode->Name().empty());
	const bool bHasOpening     = bIsElement && !pNode->Name().empty();
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
		Out.Write(pNode->Name());

		PodSerializeAttrs(Out, pNode);

		if (bIsStandalone)
		{
			// pure HTML emits <tag> for standalone (not the XHTML <tag />)
			Out.Write('>');

			if (!bIsInline || pNode->Name() == "br" || pNode->Name() == "hr"
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
			if (pNode->Name() == "head")
			{
				bIsInsideHead = true;
			}

			if (!iIsPreformatted && pNode->FirstChild() != nullptr)
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

	for (const khtml::NodePOD* c = pNode->FirstChild(); c != nullptr; c = c->NextSibling())
	{
		// Element children recurse and continue the for-loop directly
		if (c->Kind() == khtml::NodeKind::Element)
		{
			bIsFirstAfterLinefeed = PodSerializeNodeImpl(Out, c, chIndent,
				iIndent + (bIsRootLikeDoc ? 0 : 1), bIsFirstAfterLinefeed, bIsInsideHead, iIsPreformatted);
			bLastWasSpace = bIsFirstAfterLinefeed;
			continue;
		}

		// PrintIndent before non-Element-leaves
		switch (c->Kind())
		{
			case khtml::NodeKind::Comment:
			case khtml::NodeKind::ProcessingInstruction:
			case khtml::NodeKind::DocumentType:
			case khtml::NodeKind::Text:
				PrintIndent(iIndent + (bIsRootLikeDoc ? 0 : 1));
				break;

			case khtml::NodeKind::CData:
			case khtml::NodeKind::Element:
			case khtml::NodeKind::Document:
				break;
		}

		// Emit the leaf body
		switch (c->Kind())
		{
			case khtml::NodeKind::Text:
			{
				const bool bDoNotEscape   = (c->Flags() & khtml::NodeFlag::TextDoNotEscape) != 0;
				const bool bSkipLoneSpace = !iIsPreformatted && bLastWasSpace && c->Name() == " ";

				if (!bSkipLoneSpace)
				{
					if (bDoNotEscape) Out.Write(c->Name());
					else              PodEncodeHTMLContent(Out, c->Name());
				}
				break;
			}

			case khtml::NodeKind::Comment:
				Out.Write(KHTMLComment::s_sLeadIn);
				Out.Write(c->Name());
				Out.Write(KHTMLComment::s_sLeadOut);
				break;

			case khtml::NodeKind::CData:
				Out.Write(KHTMLCData::s_sLeadIn);
				Out.Write(c->Name());
				Out.Write(KHTMLCData::s_sLeadOut);
				break;

			case khtml::NodeKind::ProcessingInstruction:
				Out.Write(KHTMLProcessingInstruction::s_sLeadIn);
				Out.Write(c->Name());
				Out.Write(KHTMLProcessingInstruction::s_sLeadOut);
				break;

			case khtml::NodeKind::DocumentType:
				Out.Write(KHTMLDocumentType::s_sLeadIn);
				Out.Write(c->Name());
				Out.Write(KHTMLDocumentType::s_sLeadOut);
				break;

			case khtml::NodeKind::Document:
			case khtml::NodeKind::Element:
				break;
		}

		switch (c->Kind())
		{
			case khtml::NodeKind::Comment:
			case khtml::NodeKind::ProcessingInstruction:
			case khtml::NodeKind::DocumentType:
				WriteLinefeed();
				break;

			case khtml::NodeKind::Text:
				if (!iIsPreformatted && !c->Name().empty() && c->Name().back() == '\n')
				{
					bIsFirstAfterLinefeed = true;
				}
				break;

			case khtml::NodeKind::CData:
			case khtml::NodeKind::Element:
			case khtml::NodeKind::Document:
				break;
		}
	}

	if (bHasOpening)
	{
		if (pNode->FirstChild() != nullptr)
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
		Out.Write(pNode->Name());
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

	for (NodePOD* c = pNode->LastChild(); c != nullptr;)
	{
		switch (c->Kind())
		{
			case NodeKind::Text:
			{
				KStringView text = c->Name();
				std::size_t i    = text.size();

				while (i > 0 && KASCII::kIsSpace(text[i - 1]))
				{
					--i;
				}

				if (i == 0)
				{
					NodePOD* prev = c->PrevSibling();
					c->Detach();
					c = prev;
					continue;
				}

				c->Name(text.substr(0, i));
				return true;
			}

			case NodeKind::Element:
			{
				const TP Props = c->TagProps();

				if (bStopAtBlockElement && KHTMLObject::IsBlock(Props))
				{
					return true;
				}

				if (KHTMLObject::IsStandalone(Props) && c->Name() != "br" && c->Name() != "hr")
				{
					return true;
				}

				if (!KHTMLObject::IsStandalone(Props) && c->Name() != "script")
				{
					if (PodRemoveTrailingWhitespace(c, bStopAtBlockElement))
					{
						return true;
					}
				}

				c = c->PrevSibling();
				break;
			}

			case NodeKind::Comment:
			case NodeKind::CData:
			case NodeKind::ProcessingInstruction:
			case NodeKind::DocumentType:
			case NodeKind::Document:
				c = c->PrevSibling();
				break;
		}
	}

	return false;

} // PodRemoveTrailingWhitespace

} // namespace khtml

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KHTML
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//-----------------------------------------------------------------------------
KHTML::KHTML()
//-----------------------------------------------------------------------------
{
	EmitEntitiesAsUTF8();
	// inject our arena so the parser can (in a future step) write tag names,
	// attribute names/values, and text content directly into arena memory
	// rather than through per-tag KString allocations. The storage layout
	// is now view-first (m_sName/m_sValue/...) so subsequent steps can flip
	// individual parse paths over to KArenaAllocator::AllocateString() or a
	// streaming KArenaStringBuilder without further changes to the parser
	// callbacks.
	SetArena(&m_Document);
	PodResetTree();

} // ctor

//-----------------------------------------------------------------------------
KHTML::KHTML(KString sSource)
//-----------------------------------------------------------------------------
{
	EmitEntitiesAsUTF8();
	PodResetTree();
	Parse(std::move(sSource));

} // ctor (KString)

//-----------------------------------------------------------------------------
KHTML::~KHTML()
//-----------------------------------------------------------------------------
{
} // dtor

//-----------------------------------------------------------------------------
bool KHTML::ParseStable(KStringView sView)
//-----------------------------------------------------------------------------
{
	// Caller promises that sView outlives this document. We register
	// the bytes as an arena stable region (no copy) and run the
	// streaming parse path. No `m_SourceBuffer` is touched.
	clear();
	if (!sView.empty())
	{
		m_Document.RegisterStableRegion(sView.data(), sView.size());
	}
	return KHTMLParser::ParseImpl(sView);

} // ParseStable

//-----------------------------------------------------------------------------
bool KHTML::Parse(KString sSource)
// (no `override` — see khtmlparser.h: the base has only a constrained
// template `Parse(T&&)` for memory input, not a concrete `Parse(KString)`.)
//-----------------------------------------------------------------------------
{
	// Take ownership of the source. If the caller passed by move
	// (KHTML(std::move(buf)) or Parse(std::move(buf))), this is zero
	// copies. If by value, one copy was made at the call site.
	m_SourceBuffer = std::move(sSource);

	// Register the source range with the arena so arena-string-allocations
	// of views pointing into it pass through as-is (no copy). Phase-5
	// in-situ parsing will rely on this; today's parser still produces
	// fresh KString accumulators internally so the registration is a
	// no-op except for any future views the caller hands in.
	if (!m_SourceBuffer.empty())
	{
		m_Document.RegisterStableRegion(m_SourceBuffer.data(), m_SourceBuffer.size());
	}

	// Hand the parser a view into our owned buffer. Call ParseImpl
	// QUALIFIED (no virtual dispatch) so we don't re-enter our own
	// `KHTML::ParseImpl` override — that would loop back into us.
	return KHTMLParser::ParseImpl(KStringView(m_SourceBuffer));

} // Parse(KString)

//-----------------------------------------------------------------------------
bool KHTML::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	// Stream input: we can't point views at a contiguous source range,
	// so we don't even try. Drop any previously-registered stable region
	// from a memory parse and dispatch straight to the streaming path.
	m_SourceBuffer.clear();
	m_Document.ClearStableRegions();
	return KHTMLParser::Parse(InStream);

} // Parse(KInStream&)

//-----------------------------------------------------------------------------
void KHTML::PodResetTree()
//-----------------------------------------------------------------------------
{
	// reset() recycles the previously-allocated blocks instead of freeing
	// them, so a hot reparse loop pays at most one std::malloc() per
	// growth (and zero on subsequent rounds of the same size).
	m_Document.reset();
	// reset() preserves stable regions (they're caller-managed metadata,
	// not arena content). For us they always point at the source buffer
	// of the *previous* parse — drop them so a stale-pointer scenario
	// can't arise when m_SourceBuffer is replaced or cleared.
	m_Document.ClearStableRegions();
	m_Document.AddNode(khtml::NodeKind::Element);
	m_PodHierarchy.clear();
	m_PodHierarchy.push_back(m_Document.FirstChild());

} // PodResetTree

//-----------------------------------------------------------------------------
khtml::NodePOD* KHTML::PodAddElement(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	auto* pNode = m_PodHierarchy.back()->AddNode(khtml::NodeKind::Element, Tag.Name());

	if (pNode)
	{
		for (const auto& Attr : Tag.Attributes())
		{
			pNode->AddAttribute(
					Attr.Name(),
					Attr.Value(),
					Attr.Quote(),
					Attr.IsEntityEncoded() == false
			);
		}
	}

	return pNode;

} // PodAddElement

//-----------------------------------------------------------------------------
void KHTML::PodAddStringNode(khtml::NodeKind kind, const KHTMLStringObject& Object)
//-----------------------------------------------------------------------------
{
	m_PodHierarchy.back()->AddNode(kind, Object.sValue);

} // PodAddStringNode

//-----------------------------------------------------------------------------
void KHTML::PodAddTextLeaf(KStringView sContent, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	m_PodHierarchy.back()->AddText(sContent, bDoNotEscape);

} // PodAddTextLeaf

//-----------------------------------------------------------------------------
void KHTML::Serialize(KOutStream& Stream, char chIndent) const
//-----------------------------------------------------------------------------
{
	khtml::SerializeNode(Stream, PodRoot(), chIndent);
}

//-----------------------------------------------------------------------------
void KHTML::Serialize(KStringRef& sOut, char chIndent) const
//-----------------------------------------------------------------------------
{
	KOutStringStream oss(sOut);
	khtml::SerializeNode(oss, PodRoot(), chIndent);
}

//-----------------------------------------------------------------------------
KString KHTML::Serialize(char chIndent) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	KOutStringStream oss(sOut);
	khtml::SerializeNode(oss, PodRoot(), chIndent);
	return sOut;
}

//-----------------------------------------------------------------------------
void KHTML::clear()
//-----------------------------------------------------------------------------
{
	// Discard any open builder BEFORE we reset the tree (the builder
	// holds a lease on m_Document).
	if (m_ContentBuilder.IsActive())
	{
		(void) m_ContentBuilder.Finalize();
	}

	PodResetTree();
	m_SourceBuffer.clear();          // also unregisters via Document::clear path
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
	if (!m_ContentBuilder.IsActive()) return;

	// Finalize the open builder. Its view is stable inside the arena —
	// use the adopt path so we do not waste an AllocateString copy.
	auto sView = m_ContentBuilder.Finalize();
	const bool bDoNotEscape = m_bDoNotEscape;
	m_bDoNotEscape = false;

	if (!sView.empty())
	{
		m_PodHierarchy.back()->AddTextAdopt(sView, bDoNotEscape);
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
	FlushText();

	switch (Object.Type())
	{
		case KHTMLTag::TYPE:
		{
			auto& Tag              = static_cast<KHTMLTag&>(Object);
			auto  TagProps         = KHTMLObject::GetTagProperty (Tag.Name());
			auto  bIsStandalone    = KHTMLObject::IsStandalone   (TagProps);
			auto  bIsInline        = KHTMLObject::IsInline       (TagProps);
			auto  bIsInlineBlock   = KHTMLObject::IsInlineBlock  (TagProps);
			auto  bIsPreformatted  = KHTMLObject::IsPreformatted (TagProps);
			auto  iHierarchyLevels = m_PodHierarchy.size();

			if (Tag.IsClosing())
			{
				if (!Tag.Attributes().empty())
				{
					SetIssue(kFormat("invalid html - closing tag has attributes: {}", Tag.ToString()));
				}

				if (iHierarchyLevels > 0 && bIsStandalone)
				{
					const auto* pLastChild = m_PodHierarchy.back()->LastChild();

					if (   pLastChild == nullptr
						|| pLastChild->Kind() != khtml::NodeKind::Element
						|| pLastChild->Name() != Tag.Name())
					{
						SetIssue(kFormat("invalid html - standalone tag closed without immediately opening it - treating it as a new standalone: {}", Tag.ToString()));
						PodAddElement(Tag);

						if (bIsInline && Tag.Name() != "br")
						{
							m_bLastWasSpace = false;
						}
						else
						{
							m_bLastWasSpace = true;
						}
					}
				}
				else if (iHierarchyLevels > 1)
				{
					if (m_PodHierarchy.back()->Name() == Tag.Name())
					{
						if (bIsInlineBlock)
						{
							khtml::PodRemoveTrailingWhitespace(m_PodHierarchy.back());
						}
						else if (bIsInline)
						{
							// phrasing context
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
						SetIssue(kFormat("invalid html - start and end tag differ: <{}> -> </{}>", m_PodHierarchy.back()->Name(), Tag.Name()));

						auto iMaxAutoClose = m_iMaxAutoCloseLevels;
						auto iCurrentLevel = iHierarchyLevels - 1;

						for(;;)
						{
							if (!iMaxAutoClose--) break;
							if (iCurrentLevel-- < 2) break;

							if (m_PodHierarchy[iCurrentLevel]->Name() == Tag.Name())
							{
								while (m_PodHierarchy.size() > iCurrentLevel)
								{
									auto* pBack = m_PodHierarchy.back();

									if (!KHTMLObject::IsInline(pBack->TagProps()))
									{
										khtml::PodRemoveTrailingWhitespace(pBack);
										m_bLastWasSpace = true;

										if (KHTMLObject::IsPreformattedTag(pBack->Name()) && m_bPreformatted)
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

				if (!bIsStandalone && !Tag.IsStandalone())
				{
					m_PodHierarchy.push_back(pPod);

					if (!KHTMLObject::IsInline(pPod->TagProps()))
					{
						m_bLastWasSpace = true;

						if (KHTMLObject::IsPreformattedTag(pPod->Name()))
						{
							++m_bPreformatted;
						}
					}
				}
				else
				{
					if (Tag.Name() == "br")
					{
						m_bLastWasSpace = true;
					}
					else if (bIsStandalone && bIsInline)
					{
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

		while (m_PodHierarchy.size() > 1)
		{
			auto* pBack = m_PodHierarchy.back();

			if (!KHTMLObject::IsInline(pBack->TagProps()))
			{
				khtml::PodRemoveTrailingWhitespace(pBack);
			}
			m_PodHierarchy.pop_back();
		}
	}

	if (m_PodHierarchy.size() == 1)
	{
		khtml::PodRemoveTrailingWhitespace(m_Document.FirstChild());
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

	if (!m_ContentBuilder.IsActive())
	{
		// Lazy-arm: takes the arena lease, blocked until FlushText().
		m_ContentBuilder.Start(m_Document);
	}
	m_ContentBuilder.Append(ch);

} // Content

//-----------------------------------------------------------------------------
void KHTML::Script(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bDoNotEscape)
	{
		FlushText();
	}

	m_bDoNotEscape = true;

	if (!m_ContentBuilder.IsActive())
	{
		m_ContentBuilder.Start(m_Document);
	}
	m_ContentBuilder.Append(ch);

} // Script

//-----------------------------------------------------------------------------
void KHTML::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bDoNotEscape)
	{
		FlushText();
	}

	m_bDoNotEscape = true;

	if (!m_ContentBuilder.IsActive())
	{
		m_ContentBuilder.Start(m_Document);
	}
	m_ContentBuilder.Append(ch);

} // Invalid

DEKAF2_NAMESPACE_END
