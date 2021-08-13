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
, Name(other.Name)
, Attributes(other.Attributes)
{
	for (auto& Child : other.Children)
	{
		Children.push_back(Child->Clone());
	}

} // copy ctor

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	Name = Tag.Name;
	Attributes = Tag.Attributes;
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KHTMLTag&& Tag)
//-----------------------------------------------------------------------------
{
	Name = std::move(Tag.Name);
	Attributes = std::move(Tag.Attributes);
}

//-----------------------------------------------------------------------------
bool KHTMLElement::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	Name = sInput;
	return true;
}

//-----------------------------------------------------------------------------
void KHTMLElement::Print(KOutStream& OutStream, char chIndent, uint16_t iIndent) const
//-----------------------------------------------------------------------------
{
	auto bIsStandalone = IsStandalone();
	auto bIsInline     = IsInline();
	bool bIsRoot       = Name.empty();

	kDebug(3, "up: <{}>, Indent={}, IsStandalone={}, IsInline={}, IsRoot={}", Name, iIndent, bIsStandalone, bIsInline, bIsRoot);

	auto PrintIndent   = [bIsInline,&chIndent,&OutStream](uint16_t iIndent)
	{
		if (chIndent && !bIsInline)
		{
			for (;iIndent-- > 0;)
			{
				OutStream.Write(chIndent);
			}
		}
	};

	if (!Name.empty())
	{
		PrintIndent(iIndent);
		OutStream.Write('<');
		OutStream.Write(Name);

		Attributes.Serialize(OutStream);

		if (bIsStandalone)
		{
			OutStream.Write(" />");
			kDebug(3, "down: <{}/>", Name);
			return;
		}

		if (bIsInline)
		{
			chIndent = 0;
		}

		OutStream.Write('>');

		if (!bIsInline)
		{
			if (!Children.empty())
			{
				switch (Children.front()->Type())
				{
					case KHTMLElement::TYPE:
					{
						auto* Element = reinterpret_cast<KHTMLElement*>(Children.front().get());

						if (!Element->IsInline() || !Element->IsStandalone())
						{
							OutStream.Write("\r\n");
						}
						else
						{
							chIndent = 0;
						}
					}
					break;

					case KHTMLText::TYPE:
						chIndent = 0;
						break;

					default:
						OutStream.Write("\r\n");
						break;
				}
			}
		}
	}

	for (const auto& it : Children)
	{
		kDebug(3, "child: {}", it->TypeName());

		auto iType = it->Type();

		switch (iType)
		{
			case KHTMLElement::TYPE:
			{
				auto* Element = reinterpret_cast<KHTMLElement*>(it.get());
				if (Element->IsInline())
				{
					chIndent = 0;
				}
				// print here, instead of calling Serialize() below
				Element->Print(OutStream, chIndent, iIndent + (bIsRoot ? 0 : 1));
			}
			continue;

			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
				PrintIndent(iIndent + (bIsRoot ? 0 : 1));
				break;

			case KHTMLText::TYPE:
				chIndent = 0;
				break;

			default:
				break;
		}

		it->Serialize(OutStream);

		switch (iType)
		{
			case KHTMLComment::TYPE:
			case KHTMLProcessingInstruction::TYPE:
			case KHTMLDocumentType::TYPE:
				OutStream.Write("\r\n");
				break;
		}
	}

	if (!Name.empty())
	{
		if (!Children.empty())
		{
			PrintIndent(iIndent);
		}

		OutStream.Write("</");
		OutStream.Write(Name);
		OutStream.Write('>');

		if (!bIsInline)
		{
			OutStream.Write("\r\n");
		}
	}

	kDebug(3, "down: </{}>", Name);

} // Print

//-----------------------------------------------------------------------------
void KHTMLElement::Print(KString& sOut, char chIndent, uint16_t iIndent) const
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
	Name.clear();
	Attributes.clear();
	Children.clear();
}

//-----------------------------------------------------------------------------
bool KHTMLElement::empty() const
//-----------------------------------------------------------------------------
{
	return Name.empty() && Children.empty();
}

//-----------------------------------------------------------------------------
KHTMLElement::KHTMLElement(KString sName, KStringView sID, KStringView sClass)
//-----------------------------------------------------------------------------
: Name(std::move(sName))
{
	SetID(sID);
	SetClass(sClass);
}

//-----------------------------------------------------------------------------
void KHTMLElement::AddText(KStringView sContent)
//-----------------------------------------------------------------------------
{
	// merge with last text node if possible
	if (Children.empty() || Children.back()->Type() != KHTMLText::TYPE)
	{
		Add(KHTMLText(KHTMLEntity::EncodeMandatory(sContent)));
	}
	else
	{
		auto* Text = reinterpret_cast<KHTMLText*>(Children.back().get());
		Text->sText += KHTMLEntity::EncodeMandatory(sContent);
	}

} // AddText

//-----------------------------------------------------------------------------
void KHTMLElement::AddRawText(KStringView sContent)
//-----------------------------------------------------------------------------
{
	// do not escape and do not merge with last text node - this is for scripts and bad code
	Add(KHTMLText(sContent));

} // AddText

//-----------------------------------------------------------------------------
void KHTML::Serialize(KOutStream& Stream, char chIndent) const
//-----------------------------------------------------------------------------
{
	m_Root.Print(Stream, chIndent);
}

//-----------------------------------------------------------------------------
void KHTML::Serialize(KString& sOut, char chIndent) const
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
	m_bOpenElement = false;
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
			auto& Tag = reinterpret_cast<KHTMLTag&>(Object);

			if (Tag.IsClosing())
			{
				if (m_Hierarchy.size() > 1)
				{
					if (m_Hierarchy.back()->Name != Tag.Name)
					{
						SetError(kFormat("invalid html - start and end tag differ: {} <> {}", m_Hierarchy.back()->Name, Tag.Name));
					}
					m_Hierarchy.pop_back();
					m_bOpenElement = false;
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

				if (!Tag.IsStandalone())
				{
					// get one level deeper
					m_Hierarchy.push_back(&Element);
					m_bOpenElement = true;
				}
			}
		}
		break;

		case KHTMLComment::TYPE:
		{
			m_Hierarchy.back()->Add(reinterpret_cast<KHTMLComment&>(Object));
		}
		break;

		case KHTMLCData::TYPE:
		{
			m_Hierarchy.back()->Add(reinterpret_cast<KHTMLCData&>(Object));
		}
		break;

		case KHTMLProcessingInstruction::TYPE:
		{
			m_Hierarchy.back()->Add(reinterpret_cast<KHTMLProcessingInstruction&>(Object));
		}
		break;

		case KHTMLDocumentType::TYPE:
		{
			m_Hierarchy.back()->Add(reinterpret_cast<KHTMLDocumentType&>(Object));
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
	if (m_bOpenElement)
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
	}

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

} // end of namespace dekaf2

