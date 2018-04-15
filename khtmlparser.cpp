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
KHTMLObject::~KHTMLObject()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void KHTMLObject::clear()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLObject::Type() const
//-----------------------------------------------------------------------------
{
	return NONE;
}

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KInStream& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	// it would not make sense to call the string parser, as
	// we woud not know in advance how many characters of the
	// stream we would have to consume
	return false;

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	// call the stream parser
	KInStringStream iss(sInput);
	return Parse(iss);

} // Parse

//-----------------------------------------------------------------------------
void KHTMLObject::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	// call the string serializer - make sure always at least one of those
	// two serializers are implemented in derived classes!
	KString sSerialized;
	Serialize(sSerialized);
	OutStream.Write(sSerialized);

} // Serialize

//-----------------------------------------------------------------------------
void KHTMLObject::Serialize(KString& sOut) const
//-----------------------------------------------------------------------------
{
	// call the stream serializer - make sure always at least one of those
	// two serializers are implemented in derived classes!
	KOutStringStream oss(sOut);
	Serialize(oss);

} // Serialize


//-----------------------------------------------------------------------------
void KHTMLStringObject::clear()
//-----------------------------------------------------------------------------
{
	Value.clear();
}

//-----------------------------------------------------------------------------
bool KHTMLStringObject::empty() const
//-----------------------------------------------------------------------------
{
	return Value.empty();
}

//-----------------------------------------------------------------------------
bool KHTMLStringObject::Parse(KInStream& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	// <!-- opens a comment until -->
	// <! opens a DTD until >
	// <? opens a processing instruction until ?>

	auto iStart = sOpening.size();
	auto iLeadIn = m_sLeadIn.size();

	if (iStart >= iLeadIn)
	{
		sOpening.remove_prefix(iLeadIn);
		Value = sOpening;
	}
	else
	{
		while (iStart < iLeadIn)
		{
			auto ch = InStream.Read();
			if (ch == m_sLeadIn[iStart])
			{
				++iStart;
			}
			else
			{
				return false;
			}
		}
	}

	// A performant stream search for a pattern is not trivial to do,
	// the algorithm of choice would actually be a Knuth-Morris-Pratt
	// search. However, for the three patterns we are searching for
	// in this parser we can implement really fast and simple
	// hard wired code. But the consequence is that we cannot accept
	// arbitrary lead-out strings, but only "-->", "?>" and ">".
	// In fact we delegate the search for the lead-out to the child
	// class to implement the corresponding algorithm there.

	return SearchForLeadOut(InStream);

} // Parse

//-----------------------------------------------------------------------------
void KHTMLStringObject::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	OutStream.Write(m_sLeadIn);
	OutStream.Write(Value);
	OutStream.Write(m_sLeadOut);

} // Serialize


//-----------------------------------------------------------------------------
void KHTMLAttribute::clear()
//-----------------------------------------------------------------------------
{
	Name.clear();
	Value.clear();
	Quote = 0;
}

//-----------------------------------------------------------------------------
bool KHTMLAttribute::empty() const
//-----------------------------------------------------------------------------
{
	return Name.empty();
}

//-----------------------------------------------------------------------------
bool KHTMLAttribute::Parse(KInStream& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	clear();

	enum pstate { START, KEY, BEFORE_EQUAL, AFTER_EQUAL, VALUE };
	pstate state { START };

	if (!sOpening.empty())
	{
		Name = sOpening;
		state = KEY;
	}

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
					return true;
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
					return true;
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
						return true;
					}
				}
				else
				{
					// the no - quotes case
					if (std::isspace(ch))
					{
						// normal exit
						return true;
					}

					if (ch == '>' || ch == '/')
					{
						// normal exit
						InStream.UnRead();
						return true;
					}

					Value += ch;
				}
				break;
		}
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttribute::Serialize(KString& sOut) const
//-----------------------------------------------------------------------------
{
	if (!empty())
	{
		sOut += Name;

		if (!Value.empty())
		{
			sOut += '=';

			if (!Quote)
			{
				// lazy check if we need a quote (maybe the value was changed)
				if (Value.find_first_of(" \t\r\n\b\"'=<>`") != KString::npos)
				{
					Quote = '"';
				}
			}

			if (Quote)
			{
				sOut += Quote;
			}

			sOut += Value;

			if (Quote)
			{
				sOut += Quote;
			}
		}
	}

} // Serialize

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
void KHTMLAttributes::clear()
//-----------------------------------------------------------------------------
{
	m_Attributes.clear();
}

//-----------------------------------------------------------------------------
bool KHTMLAttributes::empty() const
//-----------------------------------------------------------------------------
{
	return m_Attributes.empty();
}

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
bool KHTMLAttributes::Parse(KInStream& InStream, KStringView sOpening)
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
				return true;
			}
			else
			{
				InStream.UnRead();
				// parse a new attribute
				KHTMLAttribute attribute;
				attribute.Parse(InStream);
				if (!attribute.empty())
				{
					Add(std::move(attribute));
				}
			}
		}
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttributes::Serialize(KString& sOut) const
//-----------------------------------------------------------------------------
{
	for (auto& attribute : m_Attributes)
	{
		sOut += ' ';
		attribute.Serialize(sOut);
	}

} // Serialize

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

	// TODO fix possible race in MT
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
bool KHTMLTag::empty() const
//-----------------------------------------------------------------------------
{
	return Name.empty();

} // empty

//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLTag::Type() const
//-----------------------------------------------------------------------------
{
	return TAG;
}

//-----------------------------------------------------------------------------
bool KHTMLTag::Parse(KInStream& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	clear();

	enum pstate { START, OPEN, NAME, CLOSE };
	pstate state { START };
	std::iostream::int_type ch;

	auto iOSize = sOpening.size();
	if (iOSize)
	{
		if (iOSize > 1)
		{
			Name = sOpening.substr(1, KStringView::npos);
		}
		state = OPEN;
	}

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
					InStream.UnRead();
					return false;
				}
				break;


			case OPEN:
				if (ch == '>')
				{
					// error (no tag, probably a comment)
					// make sure the INVALID destination will find this closing bracket
					InStream.UnRead();
					return false;
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
				else
				{
					// no spaces are allowed in front of tag names
					InStream.UnRead();
					return false;
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
					return true;
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
						return true;
					}
					else if (ch == '/')
					{
						// it would be good to check for !bClosing here as well,
						// but - garbage in, garbage out
						bSelfClosing = true;
					}
				}
				break;

		}
	}

	return false;
	
} // Parse

//-----------------------------------------------------------------------------
void KHTMLTag::Serialize(KString& sOut) const
//-----------------------------------------------------------------------------
{
	if (!empty())
	{
		sOut += '<';

		if (bClosing)
		{
			sOut += '/';
		}

		sOut += Name;

		Attributes.Serialize(sOut);

		if (bSelfClosing)
		{
			if (!Attributes.empty())
			{
				sOut += ' ';
			}
			sOut += '/';
		}

		sOut += '>';
	}

} // Serialize

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
			if (!Attributes.empty())
			{
				OutStream.Write(' ');
			}
			OutStream.Write('/');
		}

		OutStream.Write('>');
	}

} // Serialize


//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLComment::Type() const
//-----------------------------------------------------------------------------
{
	return COMMENT;
}

//-----------------------------------------------------------------------------
bool KHTMLComment::SearchForLeadOut(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

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
				Value += '-';
			}
			Value += '-';
		}
		Value += ch;
	}

	return false;

} // SearchForLeadOut

//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLDocumentType::Type() const
//-----------------------------------------------------------------------------
{
	return DOCUMENTTYPE;
}

//-----------------------------------------------------------------------------
bool KHTMLDocumentType::SearchForLeadOut(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (ch == '>')
		{
			return true;
		}
		Value += ch;
	}

	return false;

} // SearchForLeadOut

//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLProcessingInstruction::Type() const
//-----------------------------------------------------------------------------
{
	return PROCESSINGINSTRUCTION;
}

//-----------------------------------------------------------------------------
bool KHTMLProcessingInstruction::SearchForLeadOut(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		while (ch == '?')
		{
			ch = InStream.Read();
			if (ch == '>')
			{
				return true;
			}
			Value += '?';
		}
		Value += ch;
	}

	return false;

} // SearchForLeadOut


//-----------------------------------------------------------------------------
KHTMLObjectType KHTMLCData::Type() const
//-----------------------------------------------------------------------------
{
	return CDATA;
}

//-----------------------------------------------------------------------------
bool KHTMLCData::SearchForLeadOut(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::iostream::int_type ch;

	while ((ch = InStream.Read()) != std::iostream::traits_type::eof())
	{
		if (ch == ']')
		{
			ch = InStream.Read();
			while (ch == ']')
			{
				ch = InStream.Read();
				if (ch == '>')
				{
					return true;
				}
				Value += ']';
			}
			Value += ']';
		}
		Value += ch;
	}

	return false;

} // SearchForLeadOut


//-----------------------------------------------------------------------------
KHTMLParser::~KHTMLParser()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(KStringView sInvalid)
//-----------------------------------------------------------------------------
{
	if (!sInvalid.empty())
	{
		for (auto ch : sInvalid)
		{
			Invalid(ch);
		}
	}

} // Invalid

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(const KHTMLStringObject& Object)
//-----------------------------------------------------------------------------
{
	Invalid(Object.LeadIn());
	Invalid(Object.Value);

} // PushToInvalid

//-----------------------------------------------------------------------------
bool KHTMLParser::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	std::istream::int_type ch;
	bool bInvalid { false };

	while ((ch = InStream.Read()) != std::istream::traits_type::eof())
	{
		if (bInvalid)
		{
			Invalid(ch);
			if (ch == '>')
			{
				bInvalid = false;
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
				ch = InStream.Read();
				if (ch == '-')
				{
					KHTMLComment Comment;
					if (Comment.Parse(InStream, "<!-"))
					{
						Object(Comment);
					}
					else
					{
						// if this went wrong we set the output to INVALID, and will
						// parse into that until we reach a '>'
						Invalid(Comment);
						bInvalid = true;
					}
				}
				else if (ch == '[')
				{
					KHTMLCData CData;
					if (CData.Parse(InStream, "<!["))
					{
						Object(CData);
					}
					else
					{
						// if this went wrong we set the output to INVALID, and will
						// parse into that until we reach a '>'
						Invalid(CData);
						bInvalid = true;
					}
				}
				else
				{
					InStream.UnRead();
					KHTMLDocumentType DTD;
					if (DTD.Parse(InStream, "<!"))
					{
						Object(DTD);
					}
					else
					{
						// if this went wrong we set the output to INVALID, and will
						// parse into that until we reach a '>'
						Invalid(DTD);
						bInvalid = true;
					}
				}
				continue;
			}
			else if (ch == '?')
			{
				KHTMLProcessingInstruction PI;
				if (PI.Parse(InStream, "<?"))
				{
					Object(PI);
				}
				else
				{
					// if this went wrong we set the output to INVALID, and will
					// parse into that until we reach a '>'
					Invalid(PI);
					bInvalid = true;
				}
				continue;
			}

			InStream.UnRead();

			// no, this is most probably a tag
			KHTMLTag tag;
			if (tag.Parse(InStream, "<"))
			{
				if (!tag.empty())
				{
					Object(tag);
				}
			}
			else
			{
				// print to Invalid() until next '>'
				Invalid('<');
				bInvalid = true;
			}
		}
		else
		{
			Content(ch);
		}
	}

	// force a flush in children
	Finished();

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
void KHTMLParser::Content(char ch)
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Content

//-----------------------------------------------------------------------------
void KHTMLParser::Object(KHTMLObject& Object)
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
void KHTMLParser::Finished()
//-----------------------------------------------------------------------------
{
	// does nothing in base class

} // Finished

} // end of namespace dekaf2

