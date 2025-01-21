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

#include "khtmldom.h"
#include "kctype.h"
#include "klog.h"
#include "koutstringstream.h"
#include "khtmlentities.h"
#include "kexception.h"

DEKAF2_NAMESPACE_BEGIN

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
KHTMLElement::KHTMLElement(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	m_Name       = Tag.Name;
	m_Attributes = Tag.Attributes;
	m_Property   = GetTagProperty(m_Name);
	// we should check here if the attribute values are entity encoded, in
	// which case we should decode them - but we use this constructor only
	// during parsing of a DOM where the entities decode is active already
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KHTMLTag&& Tag)
//-----------------------------------------------------------------------------
{
	m_Name       = std::move(Tag.Name);
	m_Attributes = std::move(Tag.Attributes);
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
bool KHTMLElement::Print(KOutStream& OutStream, char chIndent, uint16_t iIndent, bool bIsFirstAfterLinefeed, bool bIsInsideHead) const
//-----------------------------------------------------------------------------
{
	// bIsFirstAfterLinefeed means: last output was a linefeed
	// returns bIsFirstAfterLinefeed

	auto bIsStandalone  = IsStandalone();
	auto bIsInline      = IsInline();
	auto bIsInlineBlock = IsInlineBlock();
	bool bIsRoot        = m_Name.empty();
	bool bLastWasSpace  = bIsFirstAfterLinefeed;

#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
	kDebug(4, "up: <{}{}, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}, IsFirst={}", m_Name, '>', iIndent, bIsStandalone, bIsInline, bIsRoot, bIsFirstAfterLinefeed);
#else
	kDebug(4, "up: <{}>, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}, IsFirst={}", m_Name, iIndent, bIsStandalone, bIsInline, bIsRoot, bIsFirstAfterLinefeed);
#endif

	auto PrintIndent   = [&bIsFirstAfterLinefeed,&bLastWasSpace,chIndent,&OutStream](uint16_t iIndent)
	{
		if (bIsFirstAfterLinefeed)
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
			kDebug(4, "down: <{}/>", m_Name);

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

			if (!m_Children.empty())
			{
				WriteLinefeed();
			}
		}
	}

	for (const auto& it : m_Children)
	{
		kDebug(4, "child: {}", it->TypeName());

		auto iType { it->Type() };

		switch (iType)
		{
			case KHTMLElement::TYPE:
			{
				auto* Element = static_cast<KHTMLElement*>(it.get());
				// print KHTMLElements here, instead of calling Serialize() below
				bIsFirstAfterLinefeed = Element->Print(OutStream, chIndent, iIndent + (bIsRoot ? 0 : 1), bIsFirstAfterLinefeed, bIsInsideHead);
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
			else if (bLastWasSpace)
			{
				// do not print a whitespace-only text element in normal output mode
				// if preceded by whitespace
				const auto* TextElement = static_cast<const KHTMLText*>(it.get());

				if (TextElement->sText != " ")
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
				if (!Element->sText.empty() && Element->sText.back() == '\n')
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
			if (!bIsInline || bIsInlineBlock)
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

#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
	kDebug(4, "down: </{}{}", m_Name, '>');
#else
	kDebug(4, "down: </{}>", m_Name);
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
KHTMLText& KHTMLElement::AppendTextObject(KHTMLText& left, KHTMLText& right)
//-----------------------------------------------------------------------------
{
	// adapt entity encoding
	if (left.bIsEntityEncoded == right.bIsEntityEncoded)
	{
		left.sText += right.sText;
	}
	else if (left.bIsEntityEncoded)
	{
		left.sText += KHTMLEntity::EncodeMandatory(right.sText);
	}
	else
	{
		left.sText += KHTMLEntity::Decode(right.sText);
	}

	return left;

} // AppendTextObject

//-----------------------------------------------------------------------------
KHTMLText& KHTMLElement::PrependTextObject(KHTMLText& left, KHTMLText& right)
//-----------------------------------------------------------------------------
{
	// adapt entity encoding
	if (left.bIsEntityEncoded == right.bIsEntityEncoded)
	{
		left.sText.insert(0, right.sText);
	}
	else if (left.bIsEntityEncoded)
	{
		left.sText.append(" ");
		left.sText.insert(0, KHTMLEntity::EncodeMandatory(right.sText));
	}
	else
	{
		left.sText.insert(0, KHTMLEntity::Decode(right.sText));
	}

	return left;

} // PrependTextObject

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
	if (Object.sText.empty())
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
	if (Object.sText.empty())
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

				Text->sText.TrimLeft();

				if (Text->sText.empty())
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

				Text->sText.TrimRight();

				if (Text->sText.empty())
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
KHTMLElement::self& KHTMLElement::SetAttribute(KString sName, KString sValue, bool bRemoveIfEmptyValue)
//-----------------------------------------------------------------------------
{
	if (bRemoveIfEmptyValue && sValue.empty())
	{
		m_Attributes.Remove(sName);
	}
	else
	{
		m_Attributes.Set(std::move(sName), std::move(sValue));
	}
	return *this;

} // SetAttribute

//-----------------------------------------------------------------------------
KHTML::KHTML()
//-----------------------------------------------------------------------------
{
	EmitEntitiesAsUTF8();

} // ctor

//-----------------------------------------------------------------------------
KHTML::~KHTML()
//-----------------------------------------------------------------------------
{
} // dtor

//-----------------------------------------------------------------------------
void KHTML::Serialize(KOutStream& Stream, char chIndent) const
//-----------------------------------------------------------------------------
{
	m_Root.Print(Stream, chIndent);
}

//-----------------------------------------------------------------------------
void KHTML::Serialize(KStringRef& sOut, char chIndent) const
//-----------------------------------------------------------------------------
{
	m_Root.Print(sOut, chIndent);
}

//-----------------------------------------------------------------------------
KString KHTML::Serialize(char chIndent) const
//-----------------------------------------------------------------------------
{
	return m_Root.Print(chIndent);
}

//-----------------------------------------------------------------------------
void KHTML::clear()
//-----------------------------------------------------------------------------
{
	m_Root.clear();
	m_Hierarchy.clear();
	m_Hierarchy.push_back(&m_Root);
	m_sContent.clear();
	ClearError();
	m_Issues.clear();
	m_bLastWasSpace = true;
	m_bDoNotEscape  = false;
	m_bInsideStyle  = false;

} // clear

//-----------------------------------------------------------------------------
void KHTML::FlushText()
//-----------------------------------------------------------------------------
{
	if (!m_sContent.empty())
	{
		if (m_bDoNotEscape)
		{
			m_Hierarchy.back()->AddRawText(m_sContent);
		}
		else
		{
			m_Hierarchy.back()->AddText(m_sContent);
		}

		m_sContent.clear();
		m_bDoNotEscape = false;
		// do not reset m_bLastWasSpace - we only clear it when descending from a block element
	}

} // FlushText

//-----------------------------------------------------------------------------
void KHTML::SetIssue(KString sIssue)
//-----------------------------------------------------------------------------
{
	kDebug(1, sIssue);
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
			auto  TagProps         = KHTMLObject::GetTagProperty (Tag.Name);
			auto  bIsStandalone    = KHTMLObject::IsStandalone   (TagProps);
			auto  bIsInline        = KHTMLObject::IsInline       (TagProps);
			auto  bIsInlineBlock   = KHTMLObject::IsInlineBlock  (TagProps);
			auto  iHierarchyLevels = m_Hierarchy.size();

			if (Tag.IsClosing())
			{
				if (!Tag.Attributes.empty())
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

					const auto& Children = m_Hierarchy.back()->GetChildren();

					if (   Children.empty()
						|| Children.back()->Type() != KHTMLElement::TYPE
						|| static_cast<KHTMLElement*>(Children.back().get())->GetName() != Tag.Name)
					{
						SetIssue(kFormat("invalid html - standalone tag closed without immediately opening it - treating it as a new standalone: {}", Tag.ToString()));
						m_Hierarchy.back()->Add(KHTMLElement(Tag));

						// is this content ("phrasing context")?
						if (bIsInline && Tag.Name != "br") // we treat <br> like a space
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
					if (m_Hierarchy.back()->GetName() == Tag.Name)
					{
						if (bIsInlineBlock)
						{
							// as in a block context, remove inner whitespaces
							m_Hierarchy.back()->RemoveTrailingWhitespace();
							// otherwise behave like inline
						}
						else if (bIsInline)
						{
							// this is content ("phrasing context")
						}
						else
						{
							m_Hierarchy.back()->RemoveTrailingWhitespace();
							m_bLastWasSpace = true;

							if (Tag.Name == "style")
							{
								m_bInsideStyle = false;
							}
						}

						m_Hierarchy.pop_back();
					}
					else
					{
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
						SetIssue(kFormat("invalid html - start and end tag differ: <{}{} -> </{}{}", m_Hierarchy.back()->GetName(), '>', Tag.Name, '>'));
#else
						SetIssue(kFormat("invalid html - start and end tag differ: <{}> -> </{}>", m_Hierarchy.back()->GetName(), Tag.Name));
#endif

						// now try to resync
						auto iMaxAutoClose = m_iMaxAutoCloseLevels;
						auto iCurrentLevel = iHierarchyLevels - 1;

						for(;;)
						{
							if (!iMaxAutoClose--)
							{
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
								kDebug(2, "could not resync, max auto close levels = {}, will drop </{}{}", m_iMaxAutoCloseLevels, Tag.Name, '>');
#else
								kDebug(2, "could not resync, max auto close levels = {}, will drop </{}>", m_iMaxAutoCloseLevels, Tag.Name);
#endif
								break;
							}

							if (iCurrentLevel-- < 2)
							{
#if	DEKAF2_FORMAT_HAS_BROKEN_FILL_DETECTION
								kDebug(2, "could not resync, reached root during descent, will drop </{}{}", Tag.Name, '>');
#else
								kDebug(2, "could not resync, reached root during descent, will drop </{}>", Tag.Name);
#endif
								break;
							}

							if (m_Hierarchy[iCurrentLevel]->GetName() == Tag.Name)
							{
								kDebug(2, "resync after {} descents", m_Hierarchy.size() - 1 - iCurrentLevel);
								while (m_Hierarchy.size() > iCurrentLevel)
								{

									if (!m_Hierarchy.back()->IsInline())
									{
										// this is a block element
										m_Hierarchy.back()->RemoveTrailingWhitespace();
										m_bLastWasSpace = true;

										if (m_Hierarchy.back()->GetName() == "style")
										{
											m_bInsideStyle = false;
										}
									}

									m_Hierarchy.pop_back();
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
					m_Hierarchy.back()->RemoveTrailingWhitespace();
				}

				auto& Element = m_Hierarchy.back()->Add(KHTMLElement(Tag));

				// we check both the standalone flag from parsing, and the preset list
				// of standalone tags - HTML when not XHTML has missing standalone flags: <link/> vs <link>
				// and we also want to repair erroneous standalone tags like <video/> into <video></video>
				if (!bIsStandalone && !Tag.IsStandalone())
				{
					// get one level deeper
					m_Hierarchy.push_back(&Element);

					if (!Element.IsInline())
					{
						m_bLastWasSpace = true;

						if (Element.GetName() == "style")
						{
							// we format the contents of style elements differently
							m_bInsideStyle = true;
						}
					}
				}
				else
				{
					if (Tag.Name == "br")
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
			m_Hierarchy.back()->Add(static_cast<KHTMLComment&>(Object));
		}
		break;

		case KHTMLCData::TYPE:
		{
			m_Hierarchy.back()->Add(static_cast<KHTMLCData&>(Object));
		}
		break;

		case KHTMLProcessingInstruction::TYPE:
		{
			m_Hierarchy.back()->Add(static_cast<KHTMLProcessingInstruction&>(Object));
		}
		break;

		case KHTMLDocumentType::TYPE:
		{
			m_Hierarchy.back()->Add(static_cast<KHTMLDocumentType&>(Object));
		}
		break;
	}

} // Object

//-----------------------------------------------------------------------------
void KHTML::Finished()
//-----------------------------------------------------------------------------
{
	FlushText();

	if (m_Hierarchy.size() > 1)
	{
		SetIssue(kFormat("unbalanced HTML - still {} nodes open at end", m_Hierarchy.size() - 1));

		// close all open nodes..
		while (m_Hierarchy.size() > 1)
		{
			if (!m_Hierarchy.back()->IsInline())
			{
				// this is a block element
				m_Hierarchy.back()->RemoveTrailingWhitespace();
			}
			m_Hierarchy.erase(m_Hierarchy.end() - 1);
		}
	}

	if (m_Hierarchy.size() == 1)
	{
		m_Hierarchy.back()->RemoveTrailingWhitespace();
	}

} // Finished

//-----------------------------------------------------------------------------
void KHTML::Content(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_bInsideStyle)
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
