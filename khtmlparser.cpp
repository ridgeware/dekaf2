/*
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
#include "kstringutils.h"
#include "klog.h"
#include "kfrozen.h"
#include "kctype.h"

namespace dekaf2 {

namespace {

//-----------------------------------------------------------------------------
template <typename Output>
void EncodeMandatoryAttributeValue(Output& Out, KStringView sValue, KHTMLAttribute::QuoteChar ChQuote)
//-----------------------------------------------------------------------------
{
	for (auto ch : sValue)
	{
		switch (ch)
		{
			case '"':
				if (ChQuote == '\'')
				{
					Out += ch;
				}
				else
				{
					Out += "&quot;";
				}
				break;
			case '&':
				Out += "&amp;";
				break;
			case '\'':
				if (ChQuote == '"')
				{
					Out += ch;
				}
				else
				{
					Out += "&apos;";
				}
				break;
			case '<':
				Out += "&lt;";
				break;
			case '>':
				Out += "&gt;";
				break;
			default:
				Out += ch;
				break;
		}
	}

} // EncodeMandatoryAttributeValue

//-----------------------------------------------------------------------------
template <typename Output>
void EncodeMandatoryHTMLContent(Output& Out, KStringView sContent)
//-----------------------------------------------------------------------------
{
	for (auto ch : sContent)
	{
		switch (ch)
		{
			case '&':
				Out += "&amp;";
				break;
			case '<':
				Out += "&lt;";
				break;
			case '>':
				Out += "&gt;";
				break;
			default:
				Out += ch;
				break;
		}
	}

} // EncodeMandatoryHTMLContent

} // end of anonymous namespace

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
bool KHTMLObject::Parse(KInStream& InStream, KStringView sOpening, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	KBufferedStreamReader kbr(InStream);
	return Parse(kbr, sOpening, bDecodeEntities);

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KBufferedReader& InStream, KStringView sOpening, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	// it would not make sense to call the string parser, as
	// we woud not know in advance how many characters of the
	// stream we would have to consume
	return false;

} // Parse

//-----------------------------------------------------------------------------
bool KHTMLObject::Parse(KStringView sInput, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	// call the stream parser
	KBufferedStringReader kbr(sInput);
	return Parse(kbr, "", bDecodeEntities);

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
void KHTMLObject::Serialize(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	// call the stream serializer - make sure always at least one of those
	// two serializers are implemented in derived classes!
	KOutStringStream oss(sOut);
	Serialize(oss);

} // Serialize

//-----------------------------------------------------------------------------
KString KHTMLObject::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sSerialized;
	Serialize(sSerialized);
	return sSerialized;

} // Serialize

//-----------------------------------------------------------------------------
KHTMLObject::TagProperty KHTMLObject::GetTagProperty(KStringView sTag)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_FROZEN
	static constexpr std::pair<KStringView, TagProperty> s_TagProps[]
#else
	static const std::unordered_map<KStringView, TagProperty> s_TagProp_Map
#endif
	{
		{ "a"_ksv        , TagProperty::Inline                             },
		{ "abbr"_ksv     , TagProperty::Inline                             },
		{ "area"_ksv     , TagProperty::Inline | TagProperty::Standalone   },
		{ "audio"_ksv    , TagProperty::Inline | TagProperty::Embedded     },
		{ "acronym"_ksv  , TagProperty::Inline                             },
		{ "b"_ksv        , TagProperty::Inline                             },
		{ "base"_ksv     , TagProperty::Standalone                         },
		{ "basefont"_ksv , TagProperty::Standalone                         },
		{ "bdi"_ksv      , TagProperty::Inline                             },
		{ "bdo"_ksv      , TagProperty::Inline                             },
		{ "big"_ksv      , TagProperty::Inline                             },
		{ "br"_ksv       , TagProperty::Inline | TagProperty::Standalone   },
		{ "button"_ksv   , TagProperty::Inline | TagProperty::InlineBlock  },
		{ "canvas"_ksv   , TagProperty::Inline | TagProperty::Embedded     },
		{ "cite"_ksv     , TagProperty::Inline                             },
		{ "code"_ksv     , TagProperty::Inline                             },
		{ "col"_ksv      , TagProperty::Standalone                         },
		{ "command"_ksv  , TagProperty::Standalone                         },
		{ "data"_ksv     , TagProperty::Inline                             },
		{ "datalist"_ksv , TagProperty::Inline                             },
		{ "del"_ksv      , TagProperty::Inline                             },
		{ "dfn"_ksv      , TagProperty::Inline                             },
		{ "em"_ksv       , TagProperty::Inline                             },
		{ "embed"_ksv    , TagProperty::Inline | TagProperty::Standalone | TagProperty::Embedded },
		{ "font"_ksv     , TagProperty::Inline | TagProperty::NotInlineInHead },
		{ "hr"_ksv       , TagProperty::Standalone                         },
		{ "i"_ksv        , TagProperty::Inline                             },
		{ "iframe"_ksv   , TagProperty::Inline | TagProperty::Embedded     },
		{ "img"_ksv      , TagProperty::Inline | TagProperty::Standalone | TagProperty::Embedded },
		{ "input"_ksv    , TagProperty::Inline | TagProperty::Standalone | TagProperty::InlineBlock },
		{ "ins"_ksv      , TagProperty::Inline                             },
		{ "isindex"_ksv  , TagProperty::Standalone                         },
		{ "kbd"_ksv      , TagProperty::Inline                             },
		{ "keygen"_ksv   , TagProperty::Inline | TagProperty::Standalone   },
		{ "label"_ksv    , TagProperty::Inline                             },
		{ "link"_ksv     , TagProperty::Inline | TagProperty::Standalone | TagProperty::NotInlineInHead },
		{ "map"_ksv      , TagProperty::Inline                             },
		{ "mark"_ksv     , TagProperty::Inline                             },
		{ "math"_ksv     , TagProperty::Inline | TagProperty::Embedded     },
		{ "meta"_ksv     , TagProperty::Inline | TagProperty::Standalone | TagProperty::NotInlineInHead },
		{ "meter"_ksv    , TagProperty::Inline                             },
		{ "noscript"_ksv , TagProperty::Inline                             },
		{ "object"_ksv   , TagProperty::Inline | TagProperty::Embedded     },
		{ "output"_ksv   , TagProperty::Inline                             },
		{ "param"_ksv    , TagProperty::Standalone                         },
		{ "picture"_ksv  , TagProperty::Inline | TagProperty::Embedded     },
		{ "progress"_ksv , TagProperty::Inline                             },
		{ "q"_ksv        , TagProperty::Inline                             },
		{ "rb"_ksv       , TagProperty::Inline                             },
		{ "rp"_ksv       , TagProperty::Inline                             },
		{ "rt"_ksv       , TagProperty::Inline                             },
		{ "rtc"_ksv      , TagProperty::Inline                             },
		{ "ruby"_ksv     , TagProperty::Inline                             },
		{ "s"_ksv        , TagProperty::Inline                             },
		{ "samp"_ksv     , TagProperty::Inline                             },
		{ "script"_ksv   , TagProperty::Inline                             },
		{ "select"_ksv   , TagProperty::Inline | TagProperty::InlineBlock  },
		{ "slot"_ksv     , TagProperty::Inline                             },
		{ "small"_ksv    , TagProperty::Inline                             },
		{ "source"_ksv   , TagProperty::Standalone                         },
		{ "span"_ksv     , TagProperty::Inline                             },
		{ "strike"_ksv   , TagProperty::Inline                             },
		{ "strong"_ksv   , TagProperty::Inline                             },
		{ "sub"_ksv      , TagProperty::Inline                             },
		{ "sup"_ksv      , TagProperty::Inline                             },
		{ "svg"_ksv      , TagProperty::Inline | TagProperty::Embedded     },
		{ "template"_ksv , TagProperty::Inline                             },
		{ "textarea"_ksv , TagProperty::Inline | TagProperty::InlineBlock  },
		{ "time"_ksv     , TagProperty::Inline                             },
		{ "track"_ksv    , TagProperty::Standalone                         },
		{ "tt"_ksv       , TagProperty::Inline                             },
		{ "u"_ksv        , TagProperty::Inline                             },
		{ "var"_ksv      , TagProperty::Inline                             },
		{ "video"_ksv    , TagProperty::Inline | TagProperty::Embedded     },
		{ "wbr"_ksv      , TagProperty::Inline | TagProperty::Standalone   },
	};

#ifdef DEKAF2_HAS_FROZEN
	static constexpr auto s_TagProp_Map = frozen::make_unordered_map(s_TagProps);
#endif

	auto it = s_TagProp_Map.find(sTag);

	if (it != s_TagProp_Map.end())
	{
		return it->second;
	}

	return TagProperty::Block;

} // GetTagProperty

//-----------------------------------------------------------------------------
bool KHTMLObject::IsBooleanAttribute(KStringView sAttributeName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_FROZEN
	// this set is created at compile time
	static constexpr auto s_BooleanAttributes {frozen::make_unordered_set(	{
#else
	// this set is created at run time
	static const std::unordered_set<KStringView> s_BooleanAttributes {
#endif
		"async"_ksv,
		"autocomplete"_ksv,
		"autofocus"_ksv,
		"autoplay"_ksv,
		"border"_ksv,
		"challenge"_ksv,
		"checked"_ksv,
		"compact"_ksv,
		"contenteditable"_ksv,
		"controls"_ksv,
		"default"_ksv,
		"defer"_ksv,
		"disabled"_ksv,
		"formnovalidate"_ksv,
		"frameborder"_ksv,
		"hidden"_ksv,
		"indeterminate"_ksv,
		"ismap"_ksv,
		"loop"_ksv,
		"multiple"_ksv,
		"muted"_ksv,
		"nohref"_ksv,
		"noresize"_ksv,
		"noshade"_ksv,
		"novalidate"_ksv,
		"nowrap"_ksv,
		"open"_ksv,
		"readonly"_ksv,
		"required"_ksv,
		"reversed"_ksv,
		"scoped"_ksv,
		"scrolling"_ksv,
		"seamless"_ksv,
		"selected"_ksv,
		"sortable"_ksv,
		"spellcheck"_ksv,
		"translate"_ksv
#ifdef DEKAF2_HAS_FROZEN
	})};
#else
	};
#endif

	return s_BooleanAttributes.find(sAttributeName) != s_BooleanAttributes.end();

} // IsInline

//-----------------------------------------------------------------------------
KString KHTMLObject::DecodeEntity(KBufferedReader& InStream)
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

	return sEntity;

} // DecodeEntity

//-----------------------------------------------------------------------------
bool KHTMLText::Parse(KStringView sInput, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	if (bDecodeEntities)
	{
		sText = KHTMLEntity::Decode(sInput);
		bIsEntityEncoded = false;
	}
	else
	{
		sText = sInput;
		bIsEntityEncoded = true;
	}
	return true;
}

//-----------------------------------------------------------------------------
void KHTMLText::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	if (bIsEntityEncoded)
	{
		OutStream.Write(sText);
	}
	else
	{
		EncodeMandatoryHTMLContent(OutStream, sText);
	}
}

//-----------------------------------------------------------------------------
void KHTMLText::Serialize(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	if (bIsEntityEncoded)
	{
		sOut += sText;
	}
	else
	{
		EncodeMandatoryHTMLContent(sOut, sText);
	}
}

//-----------------------------------------------------------------------------
void KHTMLText::clear()
//-----------------------------------------------------------------------------
{
	sText.clear();
	bIsEntityEncoded = false;
}

//-----------------------------------------------------------------------------
bool KHTMLText::empty() const
//-----------------------------------------------------------------------------
{
	return sText.empty();
}

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
bool KHTMLStringObject::Parse(KBufferedReader& InStream, KStringView sOpening, bool bDecodeEnitities)
//-----------------------------------------------------------------------------
{
	// for now we ignore bDecodeEnitities

	// <!-- opens a comment until -->
	// <!DOCTYPE opens a DTD until >
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
			if (KASCII::kToUpper(ch) == m_sLeadIn[iStart])
			{
				++iStart;
			}
			else
			{
				if (KASCII::kIsSpace(ch) && m_sLeadIn[iStart] == ' ')
				{
					++iStart;
				}
				else
				{
					return false;
				}
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
	bIsEntityEncoded = false;
}

//-----------------------------------------------------------------------------
bool KHTMLAttribute::empty() const
//-----------------------------------------------------------------------------
{
	return Name.empty();
}

//-----------------------------------------------------------------------------
bool KHTMLAttribute::Parse(KBufferedReader& InStream, KStringView sOpening, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	clear();

	bIsEntityEncoded = !bDecodeEntities;

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
						if (DEKAF2_UNLIKELY(bDecodeEntities && ch == '&'))
						{
							Value = KHTMLObject::DecodeEntity(InStream);
						}
						else
						{
							Value.assign(1, ch);
						}
					}
					state = VALUE;
				}
				break;

			case VALUE:
				if (DEKAF2_UNLIKELY(bDecodeEntities && ch == '&'))
				{
					Value += KHTMLObject::DecodeEntity(InStream);
				}
				else if (Quote)
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
void KHTMLAttribute::Serialize(KStringRef& sOut) const
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
				static KFindSetOfChars FindSet(" \f\v\t\r\n\b\"'=<>`");
				// lazy check if we need a quote (maybe the value was changed)
				if (FindSet.find_first_in(Value) != KString::npos)
				{
					Quote = '"';
				}
			}

			if (Quote)
			{
				sOut += Quote;
			}

			if (bIsEntityEncoded)
			{
				sOut += Value;
			}
			else
			{
				EncodeMandatoryAttributeValue(sOut, Value, Quote);
			}

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
				static KFindSetOfChars FindSet(" \f\v\t\r\n\b\"'=<>`");
				// lazy check if we need a quote (maybe the value was changed)
				if (FindSet.find_first_in(Value) != KString::npos)
				{
					Quote = '"';
				}
			}

			if (Quote)
			{
				OutStream.Write(Quote);
			}

			if (bIsEntityEncoded)
			{
				OutStream.Write(Value);
			}
			else
			{
				EncodeMandatoryAttributeValue(OutStream, Value, Quote);
			}

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
bool KHTMLAttributes::Has(KStringView sAttributeName) const
//-----------------------------------------------------------------------------
{
	return m_Attributes.find(sAttributeName) != m_Attributes.end();

} // Has

//-----------------------------------------------------------------------------
KHTMLAttributes& KHTMLAttributes::Set(KHTMLAttribute Attribute)
//-----------------------------------------------------------------------------
{
	if (Attribute.Name.empty())
	{
		kDebug(1, "cannot add an attribute with an empty name");
		return *this;
	}

	if (Attribute.Value.empty())
	{
		if (!KHTMLObject::IsBooleanAttribute(Attribute.Name))
		{
			kDebug(2, "cannot add an attribute '{}' with an empty value that is not a predefined boolean attribute", Attribute.Name);
			return *this;
		}
	}

	auto pair = m_Attributes.insert(std::move(Attribute));

	if (!pair.second)
	{
		// when the insert failed the attribute value was not moved,
		// we still have access to it
		pair.first->Value = std::move(Attribute.Value);
	}

	return *this;

} // Set

//-----------------------------------------------------------------------------
KHTMLAttributes& KHTMLAttributes::Set(KHTMLAttributes Attributes)
//-----------------------------------------------------------------------------
{
	for (auto& Attribute : Attributes)
	{
		Set(std::move(Attribute));
	}
	return *this;

} // Set

//-----------------------------------------------------------------------------
void KHTMLAttributes::Remove(KStringView sAttributeName)
//-----------------------------------------------------------------------------
{
	auto it = m_Attributes.find(sAttributeName);
	if (it != m_Attributes.end())
	{
		m_Attributes.erase(it);
	}

} // Get

//-----------------------------------------------------------------------------
bool KHTMLAttributes::Parse(KBufferedReader& InStream, KStringView sOpening, bool bDecodeEntities)
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
				attribute.Parse(InStream, KStringView{}, bDecodeEntities);
				if (!attribute.empty())
				{
					Set(std::move(attribute));
				}
			}
		}
	}

	return false;

} // Parse

//-----------------------------------------------------------------------------
void KHTMLAttributes::Serialize(KStringRef& sOut) const
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
bool KHTMLTag::Parse(KBufferedReader& InStream, KStringView sOpening, bool bDecodeEntities)
//-----------------------------------------------------------------------------
{
	clear();

	enum pstate { START, OPEN, NAME, CLOSE };
	pstate state { START };
	std::iostream::int_type ch;
	std::size_t iCloseCount { 0 }; // helper to unread in case of invalid html

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
					Attributes.Parse(InStream, KStringView{}, bDecodeEntities);
					state = CLOSE;
				}
				else if (ch == '>')
				{
					// we're done
					return true;
				}
				else if (ch == '/')
				{
					// we should now not have attributes following!
					TagType = TagType::STANDALONE;
					state = CLOSE;
				}
				else
				{
					Name += KASCII::kToLower(ch);
				}
				break;

			case CLOSE:
				++iCloseCount;

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
					else
					{
						// this is invalid HTML, e.g. a tag like
						// <br/ attr=value>
						while (iCloseCount--)
						{
							// it is not guaranteed that all unreads work in case there
							// were more than one - but as we are already in an error
							// situation that does not really matter..
							InStream.UnRead();
						}
						return false;
					}
				}
				break;

		}
	}

	return false;
	
} // Parse

//-----------------------------------------------------------------------------
void KHTMLTag::Serialize(KStringRef& sOut) const
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
						KHTMLTag ScriptTag;
						ScriptTag.Parse(ScriptEndTag);
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

			if (DEKAF2_UNLIKELY(ch == '!'))
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
			if (DEKAF2_LIKELY(tag.Parse(InStream, "<", m_bEmitEntitiesAsUTF8)))
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
			KString sEntity = KHTMLObject::DecodeEntity(InStream);

			for (auto ch : sEntity)
			{
				Content(ch);
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
constexpr KStringView KHTMLText::s_sObjectName;
constexpr KStringView KHTMLTag::s_sObjectName;
constexpr KStringView KHTMLComment::s_sObjectName;
constexpr KStringView KHTMLDocumentType::s_sObjectName;
constexpr KStringView KHTMLProcessingInstruction::s_sObjectName;
constexpr KStringView KHTMLCData::s_sObjectName;
#endif

} // end of namespace dekaf2

