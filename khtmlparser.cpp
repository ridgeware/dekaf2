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
#include "khtmlentities.h"
#include "kstringstream.h"
#include "klog.h"
#include "kfrozen.h"
#include "kctype.h"

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
KHTMLObject::ObjectType KHTMLObject::Type() const
//-----------------------------------------------------------------------------
{
	return NONE;
}

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KInStream& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	KBufferedStreamReader kbr(InStream);
	return Parse(kbr, sOpening);

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KBufferedReader& InStream, KStringView sOpening)
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
	KBufferedStringReader kbr(sInput);
	return Parse(kbr);

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
bool KHTMLStringObject::Parse(KBufferedReader& InStream, KStringView sOpening)
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
bool KHTMLAttribute::Parse(KBufferedReader& InStream, KStringView sOpening)
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

	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::iostream::traits_type::eof()))
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
				else if (!KASCII::kIsSpace(ch))
				{
					Name.assign(1, KASCII::kToLower(ch));
					state = KEY;
				}
				break;

			case KEY:
				if (KASCII::kIsSpace(ch))
				{
					state = BEFORE_EQUAL;
				}
				else if (ch == '=')
				{
					state = AFTER_EQUAL;
				}
				else if (ch == '>')
				{
					// an attribute without value
					InStream.UnRead();
					return true;
				}
				else
				{
					Name += KASCII::kToLower(ch);
				}
				break;

			case BEFORE_EQUAL:
				if (ch == '=')
				{
					state = AFTER_EQUAL;
				}
				else if (!KASCII::kIsSpace(ch))
				{
					// probably the next attribute - this one had no value
					InStream.UnRead();
					return true;
				}
				break;

			case AFTER_EQUAL:
				if (!KASCII::kIsSpace(ch))
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
					if (KASCII::kIsSpace(ch))
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

	return false;

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
				if (Value.find_first_of(" \t\r\n\b\"'=<>`") != KString::npos)
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
bool KHTMLAttributes::Add(KHTMLAttribute&& Attribute)
//-----------------------------------------------------------------------------
{
	return m_Attributes.insert(std::move(Attribute)).second;

} // Add

//-----------------------------------------------------------------------------
void KHTMLAttributes::Replace(KHTMLAttribute&& Attribute)
//-----------------------------------------------------------------------------
{
	m_Attributes.erase(Attribute);
	m_Attributes.insert(std::move(Attribute));

} // Replace

//-----------------------------------------------------------------------------
bool KHTMLAttributes::Parse(KBufferedReader& InStream, KStringView sOpening)
//-----------------------------------------------------------------------------
{
	clear();

	std::iostream::int_type ch;

	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::iostream::traits_type::eof()))
	{
		if (!KASCII::kIsSpace(ch))
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

	return false;

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

#ifdef DEKAF2_HAS_FROZEN
	// this set is created at compile time
	static constexpr auto s_InlineTags {frozen::make_unordered_set(	{
#else
	// this set is created at run time
	static const std::unordered_set<KStringView> s_InlineTags {
#endif
		"a"_ksv,
		"abbr"_ksv,
		"acronym"_ksv,
		"dfn"_ksv,
		"em"_ksv,
		"strong"_ksv,
		"code"_ksv,
		"kbd"_ksv,
		"samp"_ksv,
		"var"_ksv,
		"b"_ksv,
		"i"_ksv,
		"u"_ksv,
		"small"_ksv,
		"s"_ksv,
		"big"_ksv,
		"strike"_ksv,
		"tt"_ksv,
		"font"_ksv,
		"span"_ksv,
		"br"_ksv,
		"bdi"_ksv,
		"bdo"_ksv,
		"cite"_ksv,
		"data"_ksv,
		"del"_ksv,
		"ins"_ksv,
		"mark"_ksv,
		"q"_ksv,
		"rb"_ksv,
		"rp"_ksv,
		"rt"_ksv,
		"rtc"_ksv,
		"ruby"_ksv,
		"script"_ksv,
		"sub"_ksv,
		"template"_ksv,
		"time"_ksv,
		"wbr"_ksv
#ifdef DEKAF2_HAS_FROZEN
	})};
#else
	};
#endif

	return s_InlineTags.find(Name) != s_InlineTags.end();

} // IsInline

//-----------------------------------------------------------------------------
void KHTMLTag::clear()
//-----------------------------------------------------------------------------
{
	Name.clear();
	Attributes.clear();
	TagType = NONE;

} // clear

//-----------------------------------------------------------------------------
bool KHTMLTag::empty() const
//-----------------------------------------------------------------------------
{
	return Name.empty();

} // empty

//-----------------------------------------------------------------------------
KHTMLObject::ObjectType KHTMLTag::Type() const
//-----------------------------------------------------------------------------
{
	return TAG;
}

//-----------------------------------------------------------------------------
bool KHTMLTag::Parse(KBufferedReader& InStream, KStringView sOpening)
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

	TagType = TagType::OPEN;

	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::iostream::traits_type::eof()))
	{
		switch (state)
		{
			case START:
				if (!KASCII::kIsSpace(ch))
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
				else if (ch == '/' && !IsClosing())
				{
					TagType = TagType::CLOSE;
				}
				else if (KASCII::kIsAlNum(ch))
				{
					Name.assign(1, KASCII::kToLower(ch));
					state = NAME;
				}
				else
				{
					// no spaces are allowed in front of tag names,
					// and only ASCII alnums are permitted
					InStream.UnRead();
					return false;
				}
				break;

			case NAME:
				if (KASCII::kIsSpace(ch))
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
					Name += KASCII::kToLower(ch);
				}
				break;

			case CLOSE:
				if (!KASCII::kIsSpace(ch))
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
						TagType = TagType::STANDALONE;
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

		if (IsClosing())
		{
			sOut += '/';
		}

		sOut += Name;

		Attributes.Serialize(sOut);

		if (DEKAF2_UNLIKELY(IsStandalone()))
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

		if (IsClosing())
		{
			OutStream.Write('/');
		}

		OutStream.Write(Name);

		Attributes.Serialize(OutStream);

		if (DEKAF2_UNLIKELY(IsStandalone()))
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
KHTMLObject::ObjectType KHTMLComment::Type() const
//-----------------------------------------------------------------------------
{
	return COMMENT;
}

//-----------------------------------------------------------------------------
bool KHTMLComment::SearchForLeadOut(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		std::iostream::int_type ch = InStream.Read();

		if (DEKAF2_UNLIKELY(ch == '-'))
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

		if (DEKAF2_UNLIKELY(ch == std::iostream::traits_type::eof()))
		{
			return false;
		}

		Value += ch;
	}

} // SearchForLeadOut

//-----------------------------------------------------------------------------
KHTMLObject::ObjectType KHTMLDocumentType::Type() const
//-----------------------------------------------------------------------------
{
	return DOCUMENTTYPE;
}

//-----------------------------------------------------------------------------
bool KHTMLDocumentType::SearchForLeadOut(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		std::iostream::int_type ch = InStream.Read();

		if (DEKAF2_UNLIKELY(ch == '>'))
		{
			return true;
		}

		if (DEKAF2_UNLIKELY(ch == std::iostream::traits_type::eof()))
		{
			return false;
		}

		Value += ch;
	}

} // SearchForLeadOut

//-----------------------------------------------------------------------------
KHTMLObject::ObjectType KHTMLProcessingInstruction::Type() const
//-----------------------------------------------------------------------------
{
	return PROCESSINGINSTRUCTION;
}

//-----------------------------------------------------------------------------
bool KHTMLProcessingInstruction::SearchForLeadOut(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		std::iostream::int_type ch = InStream.Read();

		while (DEKAF2_UNLIKELY(ch == '?'))
		{
			ch = InStream.Read();
			if (ch == '>')
			{
				return true;
			}
			Value += '?';
		}

		if (DEKAF2_UNLIKELY(ch == std::iostream::traits_type::eof()))
		{
			return false;
		}

		Value += ch;
	}

} // SearchForLeadOut


//-----------------------------------------------------------------------------
KHTMLObject::ObjectType KHTMLCData::Type() const
//-----------------------------------------------------------------------------
{
	return CDATA;
}

//-----------------------------------------------------------------------------
bool KHTMLCData::SearchForLeadOut(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		std::iostream::int_type ch = InStream.Read();

		if (DEKAF2_UNLIKELY(ch == ']'))
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

		if (DEKAF2_UNLIKELY(ch == std::iostream::traits_type::eof()))
		{
			return false;
		}

		Value += ch;
	}

} // SearchForLeadOut


//-----------------------------------------------------------------------------
KHTMLParser::KHTMLParser(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	Parse(InStream);

} // ctor

//-----------------------------------------------------------------------------
KHTMLParser::KHTMLParser(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	Parse(InStream);

} // ctor

//-----------------------------------------------------------------------------
KHTMLParser::KHTMLParser(KStringView sInput)
//-----------------------------------------------------------------------------
{
	Parse(sInput);

} // ctor

//-----------------------------------------------------------------------------
KHTMLParser::~KHTMLParser()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(KStringView sInvalid)
//-----------------------------------------------------------------------------
{
	for (auto ch : sInvalid)
	{
		Invalid(ch);
	}

} // Invalid

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(const KHTMLStringObject& Object)
//-----------------------------------------------------------------------------
{
	Invalid(Object.LeadIn());
	Invalid(Object.Value);

} // Invalid

//-----------------------------------------------------------------------------
void KHTMLParser::Invalid(const KHTMLTag& Tag)
//-----------------------------------------------------------------------------
{
	KString sOut;
	Tag.Serialize(sOut);

	if (!sOut.empty())
	{
		// remove the closing >
		sOut.remove_suffix(1);
	}
	else
	{
		sOut = '<';
	}

	Invalid(sOut);

} // Invalid

//-----------------------------------------------------------------------------
void KHTMLParser::SkipInvalid(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	std::istream::int_type ch;
	
	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::istream::traits_type::eof()))
	{
		Invalid(ch);
		if (DEKAF2_UNLIKELY(ch == '>'))
		{
			break;
		}
	}

} // SkipInvalid

//-----------------------------------------------------------------------------
void KHTMLParser::SkipScript(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	static const char ScriptEndTag[] = "</script>";

	const char* pScriptEndTag;
	std::istream::int_type ch;
	KString sFailedEnd;

	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::istream::traits_type::eof()))
	{
		if (DEKAF2_UNLIKELY(ch == '<'))
		{
			sFailedEnd = ch;
			pScriptEndTag = ScriptEndTag + 1;

			for (;;)
			{
				ch = InStream.Read();
				if (ch == std::istream::traits_type::eof())
				{
					Script(sFailedEnd);
					return;
				}

				sFailedEnd += ch;

				if (KASCII::kToLower(ch) == *pScriptEndTag)
				{
					++pScriptEndTag;
					if (!*pScriptEndTag)
					{
						// create a </script> object
						KHTMLTag ScriptTag(ScriptEndTag);
						// and emit it
						Object(ScriptTag);
						return;
					}
				}
				else
				{
					if (ch == '<')
					{
						pScriptEndTag = ScriptEndTag;
					}
					else
					{
						Script(sFailedEnd);
						break;
					}
				}
			}
		}
		else
		{
			Script(ch);
		}
	}

} // SkipScript

//-----------------------------------------------------------------------------
void KHTMLParser::EmitEntityAsUTF8(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	KString sEntity;

	sEntity += '&';

	for(;;)
	{
		auto ch = InStream.Read();

		if (DEKAF2_UNLIKELY(ch == std::istream::traits_type::eof()))
		{
			break;
		}
		else
		{
			if (ch == ';')
			{
				// normal end of entity
				sEntity = KHTMLEntity::DecodeOne(sEntity);
				break;
			}

			if (!(ch == '#' && sEntity.size() == 2) && !KASCII::kIsAlNum(ch))
			{
				// this runs into the next tag or whatever, restore the last char
				InStream.UnRead();
				// try to decode the entity - will return the input string on failure
				sEntity = KHTMLEntity::DecodeOne(sEntity);
				break;
			}

			sEntity += ch;
		}
	}

	for (auto ch : sEntity)
	{
		Content(ch);
	}

} // EmitEntityAsUTF8

//-----------------------------------------------------------------------------
bool KHTMLParser::Parse(KBufferedReader& InStream)
//-----------------------------------------------------------------------------
{
	std::istream::int_type ch;

	while (DEKAF2_LIKELY((ch = InStream.Read()) != std::istream::traits_type::eof()))
	{
		if (ch == '<')
		{
			// check if this starts a comment or other command construct
			ch = InStream.Read();

			if (DEKAF2_UNLIKELY(ch == std::iostream::traits_type::eof()))
			{
				Invalid('<');
				return false;
			}
			else if (DEKAF2_UNLIKELY(ch == '!'))
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
						SkipInvalid(InStream);
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
						SkipInvalid(InStream);
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
						SkipInvalid(InStream);
					}
				}
				continue;
			}
			else if (DEKAF2_UNLIKELY(ch == '?'))
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
					SkipInvalid(InStream);
				}
				continue;
			}

			InStream.UnRead();

			// no, this is most probably a tag
			KHTMLTag tag;
			if (DEKAF2_LIKELY(tag.Parse(InStream, "<")))
			{
				if (!tag.empty())
				{
					Object(tag);
					if (DEKAF2_UNLIKELY(tag.Name == "script" && !tag.IsClosing()))
					{
						SkipScript(InStream);
					}
				}
			}
			else
			{
				// print to Invalid() until next '>'
				Invalid(tag);
				SkipInvalid(InStream);
			}
		}
		else if (ch == '&' && m_bEmitEntitiesAsUTF8)
		{
			EmitEntityAsUTF8(InStream);
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
bool KHTMLParser::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	KBufferedStreamReader kbr(InStream);
	return Parse(kbr);

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLParser::Parse(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KBufferedStringReader kbr(sInput);
	return Parse(kbr);

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
void KHTMLParser::Script(char ch)
//-----------------------------------------------------------------------------
{
	// write script content to invalid
	Invalid(ch);

} // Script

//-----------------------------------------------------------------------------
void KHTMLParser::Script(KStringView sScript)
//-----------------------------------------------------------------------------
{
	for (auto ch : sScript)
	{
		Script(ch);
	}

} // Script


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

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringView KHTMLComment::LEAD_IN;
constexpr KStringView KHTMLComment::LEAD_OUT;
constexpr KStringView KHTMLDocumentType::LEAD_IN;
constexpr KStringView KHTMLDocumentType::LEAD_OUT;
constexpr KStringView KHTMLProcessingInstruction::LEAD_IN;
constexpr KStringView KHTMLProcessingInstruction::LEAD_OUT;
constexpr KStringView KHTMLCData::LEAD_IN;
constexpr KStringView KHTMLCData::LEAD_OUT;

#endif

} // end of namespace dekaf2

