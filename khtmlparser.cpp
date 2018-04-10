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

#include "khtmlparser.h"
#include "kstringstream.h"
#include "klog.h"
#include <unordered_set>

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KHTMLAttribute::clear()
//-----------------------------------------------------------------------------
{
	Name.clear();
	Value.clear();
	Quote = 0;
}

//-----------------------------------------------------------------------------
void KHTMLAttribute::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KInStringStream iss(sInput);
	Parse(iss);
}

//-----------------------------------------------------------------------------
void KHTMLAttribute::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	clear();

	enum pstate { START, KEY, BEFORE_EQUAL, AFTER_EQUAL, VALUE };
	pstate state { START };

	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		switch (state)
		{
			case START:
				if (ch == '>')
				{
					// normal exit (no attribute)
					InStream.UnRead();
					return;
				}
				else if (!std::isspace(ch))
				{
					Name.assign(1, std::tolower(ch));
					state = KEY;
				}
				break;

			case KEY:
				if (std::isspace(ch))
				{
					state = BEFORE_EQUAL;
				}
				else if (ch == '=')
				{
					state = AFTER_EQUAL;
				}
				else
				{
					Name += std::tolower(ch);
				}
				break;

			case BEFORE_EQUAL:
				if (ch == '=')
				{
					state = AFTER_EQUAL;
				}
				else if (!std::isspace(ch))
				{
					// probably the next attribute - this one had no value
					InStream.UnRead();
					return;
				}
				break;

			case AFTER_EQUAL:
				if (!std::isspace(ch))
				{
					if (ch == '\'' || ch == '"')
					{
						Quote = ch;
					}
					else
					{
						// no quotes around attribute
						Value.assign(1, ch);
					}
					state = VALUE;
				}
				break;

			case VALUE:
				if (Quote)
				{
					if (ch != Quote)
					{
						Value += ch;
					}
					else
					{
						// normal exit
						return;
					}
				}
				else
				{
					// the no - quotes case
					if (std::isspace(ch))
					{
						// normal exit
						return;
					}

					if (ch == '>' || ch == '/')
					{
						// normal exit
						InStream.UnRead();
						return;
					}

					Value += ch;
				}
				break;
		}
	}

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttribute::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	if (!empty())
	{
		OutStream.Write(Name);

		if (!Value.empty())
		{
			OutStream.Write('=');

			if (!Quote)
			{
				// lazy check if we need a quote (maybe the value was changed)
				if (Value.find_first_of(" \t") != KString::npos)
				{
					Quote = '"';
				}
			}

			if (Quote)
			{
				OutStream.Write(Quote);
			}

			OutStream.Write(Value);

			if (Quote)
			{
				OutStream.Write(Quote);
			}
		}
	}

} // Serialize


//-----------------------------------------------------------------------------
KStringView KHTMLAttributes::Get(KStringView sAttributeName) const
//-----------------------------------------------------------------------------
{
	auto it = m_Attributes.find(sAttributeName);
	if (it != m_Attributes.end())
	{
		return it->Value;
	}
	else
	{
		return KStringView{};
	}

} // Get

//-----------------------------------------------------------------------------
void KHTMLAttributes::Add(const KHTMLAttribute& Attribute)
//-----------------------------------------------------------------------------
{
	KHTMLAttribute attribute(Attribute);
	Add(std::move(attribute));

} // Add

//-----------------------------------------------------------------------------
void KHTMLAttributes::Add(KHTMLAttribute&& Attribute)
//-----------------------------------------------------------------------------
{
	m_Attributes.insert(std::move(Attribute));

} // Add


//-----------------------------------------------------------------------------
void KHTMLAttributes::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	clear();

	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (!std::isspace(ch))
		{
			if (ch == '>' || ch == '/')
			{
				// we're done
				InStream.UnRead();
				return;
			}
			else
			{
				InStream.UnRead();
				// parse a new attribute
				KHTMLAttribute attribute(InStream);
				if (!attribute.empty())
				{
					Add(std::move(attribute));
				}
			}
		}
	}

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttributes::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KInStringStream iss(sInput);
	Parse(iss);

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttributes::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	for (auto& attribute : m_Attributes)
	{
		OutStream.Write(' ');
		attribute.Serialize(OutStream);
	}

} // Serialize


//-----------------------------------------------------------------------------
bool KHTMLTag::IsInline() const
//-----------------------------------------------------------------------------
{
	// https://en.wikipedia.org/wiki/HTML_element#Inline_elements

	static std::unordered_set<KStringView> s_InlineTags {
		KStringView {"a"},
		KStringView {"abbr"},
		KStringView {"acronym"},
		KStringView {"dfn"},
		KStringView {"em"},
		KStringView {"strong"},
		KStringView {"code"},
		KStringView {"kbd"},
		KStringView {"samp"},
		KStringView {"var"},
		KStringView {"b"},
		KStringView {"i"},
		KStringView {"u"},
		KStringView {"small"},
		KStringView {"s"},
		KStringView {"big"},
		KStringView {"strike"},
		KStringView {"tt"},
		KStringView {"font"},
		KStringView {"span"},
		KStringView {"br"},
		KStringView {"bdi"},
		KStringView {"bdo"},
		KStringView {"cite"},
		KStringView {"data"},
		KStringView {"del"},
		KStringView {"ins"},
		KStringView {"mark"},
		KStringView {"q"},
		KStringView {"rb"},
		KStringView {"rp"},
		KStringView {"rt"},
		KStringView {"rtc"},
		KStringView {"ruby"},
		KStringView {"script"},
		KStringView {"sub"},
		KStringView {"template"},
		KStringView {"time"},
		KStringView {"wbr"}
	};

	return s_InlineTags.find(Name) != s_InlineTags.end();

} // IsInline

//-----------------------------------------------------------------------------
void KHTMLTag::clear()
//-----------------------------------------------------------------------------
{
	Name.clear();
	Attributes.clear();
	bSelfClosing = false;
	bClosing = false;

} // clear

//-----------------------------------------------------------------------------
void KHTMLTag::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KInStringStream iss(sInput);
	Parse(iss);

} // Parse

//-----------------------------------------------------------------------------
void KHTMLTag::Parse(KInStream& InStream, bool bHadOpenAngleBracket)
//-----------------------------------------------------------------------------
{
	clear();

	enum pstate { START, OPEN, NAME, CLOSE };
	pstate state { bHadOpenAngleBracket ? OPEN : START };
	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		switch (state)
		{
			case START:
				if (!std::isspace(ch))
				{
					if (ch == '<')
					{
						state = OPEN;
						break;
					}
					// this is no tag
					return;
				}
				break;


			case OPEN:
				if (ch == '>')
				{
					// error (no tag, probably a comment)
					return;
				}
				else if (ch == '/' && !bClosing)
				{
					bClosing = true;
				}
				else if (!std::isspace(ch))
				{
					Name.assign(1, std::tolower(ch));
					state = NAME;
				}
				break;

			case NAME:
				if (std::isspace(ch))
				{
					Attributes.Parse(InStream);
					state = CLOSE;
				}
				else if (ch == '>')
				{
					// we're done
					return;
				}
				else
				{
					Name += std::tolower(ch);
				}
				break;

			case CLOSE:
				if (!std::isspace(ch))
				{
					if (ch == '>')
					{
						// we're done
						return;
					}
					else if (ch == '/' && !bClosing)
					{
						bSelfClosing = true;
					}
				}
				break;

		}
	}

} // Parse

//-----------------------------------------------------------------------------
void KHTMLTag::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	if (!empty())
	{
		OutStream.Write('<');

		if (bClosing)
		{
			OutStream.Write('/');
		}

		OutStream.Write(Name);

		Attributes.Serialize(OutStream);

		if (bSelfClosing)
		{
			OutStream.Write('/');
		}

		OutStream.Write('>');
	}

} // Serialize


//-----------------------------------------------------------------------------
KHTMLParser::~KHTMLParser()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KHTMLParser::ParseComment(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	ch = InStream.Read();

	if (ch != '-')
	{
		PushToInvalid("<!-");
		PushToInvalid(ch);
		return false;
	}

	// announce output of comment chars
	SwitchOutput(COMMENT);

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (ch == '-')
		{
			ch = InStream.Read();
			while (ch == '-')
			{
				ch = InStream.Read();
				if (ch == '>')
				{
					return true;
				}
				Comment('-');
			}
			Comment('-');
		}
		Comment(ch);
	}

	return false;

} // ParseComment

//-----------------------------------------------------------------------------
bool KHTMLParser::ParseDocumentType(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	ch = InStream.Read();

	if (ch == '-')
	{
		// this is most probably a comment
		return ParseComment(InStream);
	}

	// start capturing the document type
	SwitchOutput(DOCUMENTTYPE);
	DocumentType(ch);

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (ch == '>')
		{
			return true;
		}
		DocumentType(ch);
	}

	return false;

} // ParseDocumentType

//-----------------------------------------------------------------------------
bool KHTMLParser::ParseProcessingInstruction(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	// announce output of a processing instruction
	SwitchOutput(PROCESSINGINSTRUCTION);

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		while (ch == '?')
		{
			ch = InStream.Read();
			if (ch == '>')
			{
				return true;
			}
			ProcessingInstruction('?');
		}
		ProcessingInstruction(ch);
	}

	return false;

} // ParseProcessingInstruction

//-----------------------------------------------------------------------------
void KHTMLParser::PushToInvalid(KStringView sInvalid)
//-----------------------------------------------------------------------------
{
	if (!sInvalid.empty())
	{
		SwitchOutput(INVALID);
		for (auto ch : sInvalid)
		{
			Invalid(ch);
		}
	}

} // PushToInvalid

//-----------------------------------------------------------------------------
void KHTMLParser::PushToInvalid(std::iostream::int_type ch)
//-----------------------------------------------------------------------------
{
	SwitchOutput(INVALID);
	Invalid(ch);

} // PushToInvalid

//-----------------------------------------------------------------------------
bool KHTMLParser::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (m_Output == INVALID)
		{
			Invalid(ch);
			if (ch == '>')
			{
				m_Output = NONE;
			}
		}
		else if (ch == '<')
		{
			// check if this starts a comment or other command construct
			ch = InStream.Read();

			if (ch == std::iostream::traits_type::eof())
			{
				return false;
			}
			else if (ch == '!')
			{
				ParseDocumentType(InStream);
				// if this went wrong we have the output set to INVALID, and will
				// parse into that until we reach a '>'
				continue;
			}
			else if (ch == '?')
			{
				ParseProcessingInstruction(InStream);
				// if this went wrong we have the output set to INVALID, and will
				// parse into that until we reach a '>'
				continue;
			}

			InStream.UnRead();

			// no, this is most probably a tag
			KHTMLTag tag(InStream, true);
			if (!tag.empty())
			{
				SwitchOutput(TAG);
				Tag(tag);
			}
			else
			{
				// trouble ..
				kDebug(1, "parser error: invalid tag");
			}
		}
		else
		{
			SwitchOutput(CONTENT);
			Content(ch);
		}
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLParser::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KInStringStream iss(sInput);
	return Parse(iss);

} // Parse

//-----------------------------------------------------------------------------
void KHTMLParser::Tag(KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Element

//-----------------------------------------------------------------------------
void KHTMLParser::Content(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Content

//-----------------------------------------------------------------------------
void KHTMLParser::Comment(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Comment

//-----------------------------------------------------------------------------
void KHTMLParser::DocumentType(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // DTD

//-----------------------------------------------------------------------------
void KHTMLParser::ProcessingInstruction(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // ProcessingInstruction

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Invalid

//-----------------------------------------------------------------------------
void KHTMLParser::Output(OutputType)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Output

} // end of namespace dekaf2

