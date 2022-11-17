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

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(const KHTMLElement& other)
//-----------------------------------------------------------------------------
: KHTMLObject(other)
, m_Name(other.m_Name)
, m_Attributes(other.m_Attributes)
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
	m_Name = Tag.Name;
	m_Attributes = Tag.Attributes;
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KHTMLTag&& Tag)
//-----------------------------------------------------------------------------
{
	m_Name = std::move(Tag.Name);
	m_Attributes = std::move(Tag.Attributes);
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KString sName, KStringView sID, KStringView sClass)
//-----------------------------------------------------------------------------
: m_Name(std::move(sName))
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
bool KHTMLElement::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	m_Name = sInput;
	return true;
}

//-----------------------------------------------------------------------------
bool KHTMLElement::Print(KOutStream& OutStream, char chIndent, uint16_t iIndent, bool bIsFirstAfterLinefeed) const
//-----------------------------------------------------------------------------
{
	// bIsFirstAfterLinefeed means: last output was a linefeed
	// returns bIsFirstAfterLinefeed

	auto bIsStandalone = IsStandalone();
	auto bIsInline     = IsInline();
	bool bIsRoot       = m_Name.empty();

	kDebug(3, "up: <{}>, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}, IsFirst={}", m_Name, iIndent, bIsStandalone, bIsInline, bIsRoot, bIsFirstAfterLinefeed);

	auto PrintIndent   = [&bIsFirstAfterLinefeed,chIndent,&OutStream](uint16_t iIndent)
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
		}
	};

	auto WriteLinefeed = [&bIsFirstAfterLinefeed,&OutStream]()
	{
		static constexpr KStringView sLineFeed { "\r\n" };

		OutStream.Write(sLineFeed);
		bIsFirstAfterLinefeed = true;
	};

	if (!m_Name.empty())
	{
		PrintIndent(iIndent);
		OutStream.Write('<');
		OutStream.Write(m_Name);

		m_Attributes.Serialize(OutStream);

		if (bIsStandalone)
		{
			kDebug(3, "down: <{}/>", m_Name);
			OutStream.Write("/>");

			if (!bIsInline || m_Name.ToLowerASCII().In("br,hr"))
			{
				WriteLinefeed();
				return true;
			}

			return false;
		}

		OutStream.Write('>');

		if (!bIsInline)
		{
			if (!m_Children.empty())
			{
				WriteLinefeed();
			}
		}
	}

	for (const auto& it : m_Children)
	{
		kDebug(3, "child: {}", it->TypeName());

		auto iType { it->Type() };

		switch (iType)
		{
			case KHTMLElement::TYPE:
			{
				auto* Element = static_cast<KHTMLElement*>(it.get());
				// print KHTMLElements here, instead of calling Serialize() below
				bIsFirstAfterLinefeed = Element->Print(OutStream, chIndent, iIndent + (bIsRoot ? 0 : 1), bIsFirstAfterLinefeed);
			}
			continue;

			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
				PrintIndent(iIndent + (bIsRoot ? 0 : 1));
				break;

			default:
				break;
		}

		PrintIndent(iIndent+1);

		it->Serialize(OutStream);

		switch (iType)
		{
			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
				WriteLinefeed();
				break;

			case KHTMLText::TYPE:
			{
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
			if (!bIsInline)
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

		if (!bIsInline)
		{
			WriteLinefeed();
		}
	}

	kDebug(3, "down: </{}>", m_Name);
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
		// merge with last text node if possible
		if (m_Children.empty() || m_Children.back()->Type() != KHTMLText::TYPE)
		{
			Add(KHTMLText(KHTMLEntity::EncodeMandatory(sContent)));
		}
		else
		{
			auto* Text = reinterpret_cast<KHTMLText*>(m_Children.back().get());
			Text->sText += KHTMLEntity::EncodeMandatory(sContent);
		}
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
		Add(KHTMLText(sContent));
	}
	return *this;
	
} // AddText

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
	m_sContent.clear();
	m_sError.clear();
	m_bLastWasSpace = false;
	m_bDoNotEscape = false;

} // clear

//-----------------------------------------------------------------------------
void KHTML::FlushText()
//-----------------------------------------------------------------------------
{
	if (!m_sContent.empty())
	{
		if (!(m_bLastWasSpace && m_sContent.size() == 1))
		{
			if (m_bDoNotEscape)
			{
				m_Hierarchy.back()->AddRawText(m_sContent);
			}
			else
			{
				m_Hierarchy.back()->AddText(m_sContent);
			}
		}
		m_sContent.clear();
		m_bLastWasSpace = false;
		m_bDoNotEscape = false;
	}

} // FlushText

//-----------------------------------------------------------------------------
bool KHTML::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	kDebug(1, sError);
	m_sError = std::move(sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
void KHTML::Object(KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	FlushText();

	switch (Object.Type())
	{
		case KHTMLTag::TYPE:
		{
			auto& Tag = static_cast<KHTMLTag&>(Object);

			if (Tag.IsClosing())
			{
				if (m_Hierarchy.size() > 1)
				{
					if (m_Hierarchy.back()->GetName() != Tag.Name)
					{
						SetError(kFormat("invalid html - start and end tag differ: {} <> {}", m_Hierarchy.back()->GetName(), Tag.Name));
					}
					m_Hierarchy.pop_back();
				}
				else
				{
					SetError("invalid html - unbalanced");
					return;
				}
			}
			else
			{
				auto& Element = m_Hierarchy.back()->Add(KHTMLElement(Tag));

				// we check both the standalone flag from parsing, and the preset list
				// of standalone tags - HTML often has missing standalone flags: <link/> vs <link>
				if (!Tag.IsStandalone() && !KHTMLObject::IsStandaloneTag(Tag.Name))
				{
					// get one level deeper
					m_Hierarchy.push_back(&Element);
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

	if (m_Hierarchy.size() != 1)
	{
		m_Hierarchy.clear();
		m_Hierarchy.push_back(&m_Root);
		SetError("invalid html - unbalanced");
	}

} // Finished

//-----------------------------------------------------------------------------
void KHTML::Content(char ch)
//-----------------------------------------------------------------------------
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
	m_sContent += ch;

} // Content

//-----------------------------------------------------------------------------
void KHTML::Script(char ch)
//-----------------------------------------------------------------------------
{
	m_bDoNotEscape = true;
	// do not collapse white space
	m_sContent += ch;

} // Script

//-----------------------------------------------------------------------------
void KHTML::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	m_bDoNotEscape = true;
	// do not collapse white space
	m_sContent += ch;

} // Invalid

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView KHTMLElement::s_sObjectName;
#endif

} // end of namespace dekaf2

