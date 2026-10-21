/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/core/logging/kredact.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/strings/kcaseless.h>
#include <dekaf2/core/types/kctype.h>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// the replacement for a credential that is too short to show anything of
static constexpr KStringView REDACTED     { "[redacted]"     };
/// what sits between the first and the last characters of a longer one
static constexpr KStringView REDACTED_MID { "...redacted..." };
/// how many characters survive at each end
static constexpr KStringView::size_type REDACTED_KEEP { 3 };
/// the shortest credential that keeps its ends - see kMaskSecret
static constexpr KStringView::size_type REDACTED_MIN  { 16 };

//---------------------------------------------------------------------------
/// The text that replaces a credential.
///
/// A long credential KEEPS ITS FIRST AND LAST THREE CHARACTERS, because a log
/// line that says only "[redacted]" cannot answer the two questions that are
/// actually asked of it: which key is this, and is it still the same one as
/// yesterday? Three characters at each end identify a key among the handful a
/// service holds, and are no use to anyone trying to present one.
///
/// A SHORT ONE IS REPLACED WHOLE. Six characters of an eight character password
/// is the password, and no amount of debugging convenience is worth that, so
/// below REDACTED_MIN nothing of it survives.
/// @param sSecret the credential to mask
//---------------------------------------------------------------------------
static KString kMaskSecret(KStringView sSecret)
//---------------------------------------------------------------------------
{
	if (sSecret.size() < REDACTED_MIN)
	{
		return KString(REDACTED);
	}

	KString sMasked;

	sMasked.reserve(REDACTED_KEEP * 2 + REDACTED_MID.size());
	sMasked += sSecret.Left  (REDACTED_KEEP);
	sMasked += REDACTED_MID;
	sMasked += sSecret.Right (REDACTED_KEEP);

	return sMasked;

} // kMaskSecret

//---------------------------------------------------------------------------
/// Has this text already been through the redactor? The passes overlap by
/// design, and a masked credential no longer starts with the marker - it starts
/// with three characters of the credential - so the marker is looked for
/// anywhere in it. No credential contains "...redacted...": base64url, hex and
/// the alphabets of the services all exclude the '.' runs.
/// @param sValue the value to test
//---------------------------------------------------------------------------
static bool kIsRedacted(KStringView sValue)
//---------------------------------------------------------------------------
{
	return sValue.starts_with(REDACTED) || sValue.contains(REDACTED_MID);

} // kIsRedacted

//---------------------------------------------------------------------------
/// Is this a character that can appear inside a base64url token body?
//---------------------------------------------------------------------------
static inline bool kIsTokenChar(char ch)
//---------------------------------------------------------------------------
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
	    || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '=';
}

//---------------------------------------------------------------------------
/// Finds "eyJ", the start of a JWT, from iPos on. memchr scans for the 'J', which
/// is rare in log text - a substring search would stop at nearly every 'e'.
static KStringView::size_type kFindJWT(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, 'J', iPos + 2); iHit != KStringView::npos; iHit = kFind(sText, 'J', iHit + 1))
	{
		if (sText[iHit - 2] == 'e' && sText[iHit - 1] == 'y')
		{
			return iHit - 2;
		}
	}

	return KStringView::npos;

} // kFindJWT

//---------------------------------------------------------------------------
/// Finds a lowercase ASCII word in any case, from iPos on. memchr scans for the
/// anchor, a letter of the word, in its two cases, and only its hits compare the
/// whole word - with a rare letter as the anchor this is faster than a substring
/// search, while a caseless search compares at every position.
static KStringView::size_type kFindAnyCase(KStringView sText, KStringView sWord, char chAnchor, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	auto iAnchor = sWord.find(chAnchor);
	auto iFound  = KStringView::npos;

	for (auto chCase : { chAnchor, KASCII::kToUpper(chAnchor) })
	{
		for (auto iHit = kFind(sText, chCase, iPos + iAnchor); iHit < iFound; iHit = kFind(sText, chCase, iHit + 1))
		{
			auto iStart = iHit - iAnchor;

			if (sText.size() - iStart < sWord.size())
			{
				break;
			}

			if (kCaselessBeginsWithLeft(sText.substr(iStart), sWord))
			{
				iFound = iStart;
				break;
			}
		}
	}

	return iFound;

} // kFindAnyCase

//---------------------------------------------------------------------------
/// Is this a character that ends a value in a log line - white space, a quote, or
/// a backtick? The BACKTICK IS A DELIMITER AND NEVER PART OF A CREDENTIAL: slack
/// and markdown wrap a value in them, and a span that swallows them ends up
/// masking the formatting instead of the key, as "sk-...redacted...```".
static inline bool kIsValueEnd(char ch)
//---------------------------------------------------------------------------
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\'' || ch == '`';
}

//---------------------------------------------------------------------------
/// Does sName end with sWord, in any case? sWord is in lowercase. The comparison
/// runs from the back, as the last characters already differ for nearly all names
/// of parameters - this runs for each '=' of a line, and is therefore always
/// inlined.
static DEKAF2_ALWAYS_INLINE bool kEndsWithAnyCase(KStringView sName, KStringView sWord)
//---------------------------------------------------------------------------
{
	if (sWord.size() > sName.size())
	{
		return false;
	}

	auto iName = sName.size();

	for (auto iWord = sWord.size(); iWord > 0; )
	{
		if (KASCII::kToLower(sName[--iName]) != sWord[--iWord])
		{
			return false;
		}
	}

	return true;

} // kEndsWithAnyCase

//---------------------------------------------------------------------------
/// Is this a character of the name of a parameter - a letter, a digit, '_', '-' or '.'?
/// This runs for each '=' of a line, and the tests are combined with '|' instead
/// of '||', which compiles without branches - the version with branches was
/// measurably slower on lines like "a=1 b=2 c=3".
static inline bool kIsNameChar(char ch)
//---------------------------------------------------------------------------
{
	auto c = static_cast<unsigned char>(ch);

	// (c | 0x20) folds the upper case letters onto the lower case ones
	return (static_cast<unsigned char>((c | 0x20) - 'a') < 26)
	     | (static_cast<unsigned char>(c - '0') < 10)
	     | (c == '_')
	     | (c == '-')
	     | (c == '.');
}

//---------------------------------------------------------------------------
/// Does the text before a '=' end with the name of a parameter that holds a
/// credential? Short names like "pass", "key" or "sid" count only in a
/// URL, after a '?' or '&', as "pass=3 fail=0" or "sid=..." are ordinary output
/// elsewhere.
///
/// This runs for each '=' of a line, so it compares from the '=' backwards and
/// never reads the whole name: the last character selects the few candidates,
/// and nearly all names of ordinary parameters fail at their last or second to
/// last character.
/// @param sBefore the text up to the '='
static bool kEndsWithSecretName(KStringView sBefore)
//---------------------------------------------------------------------------
{
	// does sBefore end with sWord as a whole name? A '.' in front of it separates
	// a qualifier, as in "U.password" in SQL or "db.password" in a configuration
	auto IsName = [sBefore](KStringView sWord)
	{
		return kEndsWithAnyCase(sBefore, sWord)
		    && (sBefore.size() == sWord.size()
		     || !kIsNameChar(sBefore[sBefore.size() - sWord.size() - 1])
		     || sBefore[sBefore.size() - sWord.size() - 1] == '.');
	};

	// does sBefore end with sWord as a whole name in a URL?
	auto IsNameInURL = [sBefore](KStringView sWord)
	{
		if (sBefore.size() <= sWord.size() || !kEndsWithAnyCase(sBefore, sWord))
		{
			return false;
		}

		auto chBefore = sBefore[sBefore.size() - sWord.size() - 1];

		return chBefore == '?' || chBefore == '&';
	};

	auto EndsWith = [sBefore](KStringView sSuffix)
	{
		return kEndsWithAnyCase(sBefore, sSuffix);
	};

	if (sBefore.empty())
	{
		return false;
	}

	switch (KASCII::kToLower(sBefore.back()))
	{
		case 'd':
			return IsName("password") || IsName("passwd") || EndsWith("_password") || IsNameInURL("pwd") || IsNameInURL("sid");

		case 't':
			return IsName("secret") || EndsWith("_secret");

		case 'y':
			return IsName("apikey") || IsName("api_key") || IsNameInURL("key");

		case 'n':
			return EndsWith("_token") || IsNameInURL("token");

		case 's':
			return IsNameInURL("pass");

		case 'e':
			return IsNameInURL("code") || IsNameInURL("signature");

		case 'g':
			return IsNameInURL("sig");

		default:
			return false;
	}

} // kEndsWithSecretName

//---------------------------------------------------------------------------
/// Returns the content of a value in single or double quotes that starts at
/// iPos, as in SQL. The escapes '' and \' are stepped over, and an N in front
/// of the quote, which marks a unicode literal in SQL Server, is skipped. An
/// unterminated value runs to the end of sText.
///
/// This is not inlined: it only runs for a name that denotes a credential, and
/// inlined into kFindSecretParameter it made every call of that function slower.
/// @return the content between the quotes, or an empty view if there is no
/// quote at iPos
static DEKAF2_NEVER_INLINE KStringView kGetQuotedValue(KStringView sText, KStringView::size_type iPos)
//---------------------------------------------------------------------------
{
	if (iPos + 1 < sText.size() && (sText[iPos] == 'N' || sText[iPos] == 'n') && sText[iPos + 1] == '\'')
	{
		++iPos;
	}

	if (iPos >= sText.size() || (sText[iPos] != '\'' && sText[iPos] != '"'))
	{
		return {};
	}

	auto chQuote = sText[iPos];
	auto iEnd    = iPos + 1;

	while (iEnd < sText.size())
	{
		if (sText[iEnd] == '\\')
		{
			iEnd += 2;
		}
		else if (sText[iEnd] != chQuote)
		{
			++iEnd;
		}
		else if (iEnd + 1 < sText.size() && sText[iEnd + 1] == chQuote)
		{
			// a doubled quote is part of the value
			iEnd += 2;
		}
		else
		{
			break;
		}
	}

	return sText.substr(iPos + 1, iEnd - iPos - 1);

} // kGetQuotedValue

//---------------------------------------------------------------------------
/// Finds the value of a parameter "name=value" whose name says that it holds a
/// credential, from iPos on. memchr scans for the '=', and only its hits compare
/// the name before it. The value runs to white space or a quote, and in a URL also
/// to the next '&', '#' or '<' - in other text these may be part of a password.
/// A '<' is never part of a URL, but ends one inside XML or HTML text, as in
/// "<Path>/list?sid=...</Path>".
///
/// A value in single or double quotes runs to the closing quote. In this form
/// white space may surround the '=', and the name may be in SQL identifier
/// quotes, as in "set `password` = 'value'".
/// @return the value, or an empty view if there is none
static KStringView kFindSecretParameter(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, '=', iPos); iHit != KStringView::npos; iHit = kFind(sText, '=', iHit + 1))
	{
		// white space and an identifier quote between the name and the '=' are
		// only accepted in front of a quoted value
		auto iNameEnd = iHit;

		while (iNameEnd > 0 && (sText[iNameEnd - 1] == ' ' || sText[iNameEnd - 1] == '\t'))
		{
			--iNameEnd;
		}

		if (iNameEnd > 0 && (sText[iNameEnd - 1] == '`' || sText[iNameEnd - 1] == '"' || sText[iNameEnd - 1] == ']'))
		{
			--iNameEnd;
		}

		if (!kEndsWithSecretName(sText.substr(0, iNameEnd)))
		{
			continue;
		}

		auto iValue = iHit + 1;

		while (iValue < sText.size() && (sText[iValue] == ' ' || sText[iValue] == '\t'))
		{
			++iValue;
		}

		auto sValue = kGetQuotedValue(sText, iValue);

		if (sValue.empty() && iNameEnd == iHit && iValue == iHit + 1)
		{
			// only now the start of the name, to see if it is in a URL
			auto iName = iHit;

			while (iName > 0 && kIsNameChar(sText[iName - 1]))
			{
				--iName;
			}

			bool bInURL = iName > 0 && (sText[iName - 1] == '?' || sText[iName - 1] == '&');
			auto iEnd   = iHit + 1;

			while (iEnd < sText.size() && !kIsValueEnd(sText[iEnd]) && !(bInURL && (sText[iEnd] == '&' || sText[iEnd] == '#' || sText[iEnd] == '<')))
			{
				++iEnd;
			}

			sValue = sText.substr(iHit + 1, iEnd - iHit - 1);
		}

		// never redact a redaction - "access_token=[redacted]" from the JWT pass
		if (!sValue.empty() && !kIsRedacted(sValue))
		{
			return sValue;
		}
	}

	return {};

} // kFindSecretParameter

//---------------------------------------------------------------------------
/// Does the name of a JSON field, or of an SQL column, denote a credential?
///
/// THIS IS NARROWER THAN kEndsWithSecretName ON PURPOSE, and the difference is
/// the whole reason it is a separate function: in a JSON body the name is a
/// field name, and field names ending in "_key" are identifiers in every schema
/// that has ever been written - project_key, brand_key, tenant_key. Accepting a
/// bare "key" here would redact the identifier out of every error message that
/// names a row, which costs far more than it saves.
///
/// It is always inlined: it runs for each ':' after the first '"' of a message,
/// and with the SQL search as a second caller the compiler made it a call.
/// @param sName the field name, or the text up to its closing quote - only the
/// end is compared
static DEKAF2_ALWAYS_INLINE bool kIsSecretJSONName(KStringView sName)
//---------------------------------------------------------------------------
{
	if (sName.empty())
	{
		return false;
	}

	switch (KASCII::kToLower(sName.back()))
	{
		case 'd': return kEndsWithAnyCase(sName, "password")    || kEndsWithAnyCase(sName, "passwd");
		case 't': return kEndsWithAnyCase(sName, "secret");
		case 'y': return kEndsWithAnyCase(sName, "apikey")      || kEndsWithAnyCase(sName, "api_key");
		case 'n': return kEndsWithAnyCase(sName, "token");
		case 'l': return kEndsWithAnyCase(sName, "credential");
		case 's': return kEndsWithAnyCase(sName, "secrets")     || kEndsWithAnyCase(sName, "tokens")
		              || kEndsWithAnyCase(sName, "apikeys")     || kEndsWithAnyCase(sName, "api_keys")
		              || kEndsWithAnyCase(sName, "credentials");
		default:  return false;
	}

} // kIsSecretJSONName

//---------------------------------------------------------------------------
/// Skips white space from iPos on
static inline KStringView::size_type kSkipJSONSpace(KStringView sText, KStringView::size_type iPos)
//---------------------------------------------------------------------------
{
	while (iPos < sText.size() && (sText[iPos] == ' ' || sText[iPos] == '\t' || sText[iPos] == '\n' || sText[iPos] == '\r'))
	{
		++iPos;
	}

	return iPos;
}

static KStringView::size_type kFindSecretJSONColonFromQuote(KStringView sText, KStringView::size_type iFirstQuote);

//---------------------------------------------------------------------------
/// Finds the ':' of a JSON field '"name": "value"' whose name denotes a
/// credential and whose value is a string, or an array whose first element is
/// one, from iPos on.
///
/// Log messages often contain a ':', as in "host: example.com", but rarely a '"'.
/// The search therefore starts with one memchr for the first '"' - without one
/// there is no JSON field. This part is inlined, so that a message without a '"'
/// costs no function call.
///
/// ONLY A STRING VALUE COUNTS, and that single rule is what makes this safe to
/// run over anything: '"input_tokens": 1500' is a count, and a service that
/// reports its usage is full of them, while a quoted value under the same name
/// is a credential. On the XAPIS corpus the split is exact - every quoted
/// token field is a credential, every numeric one is the token ledger.
/// @return the index of the ':', or npos if there is none
static DEKAF2_ALWAYS_INLINE KStringView::size_type kFindSecretJSONColon(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	auto iFirstQuote = kFind(sText, '"', iPos);

	if (iFirstQuote == KStringView::npos)
	{
		return KStringView::npos;
	}

	return kFindSecretJSONColonFromQuote(sText, iFirstQuote);

} // kFindSecretJSONColon

//---------------------------------------------------------------------------
/// The search of kFindSecretJSONColon from the first '"' on. memchr searches for
/// the ':', because a JSON field with a string value has four quotes but only one
/// colon, and each hit checks whether the closing quote of a name precedes it.
/// @param iFirstQuote the index of the first '"' at or after the start of the search
/// @return the index of the ':', or npos if there is none
static KStringView::size_type kFindSecretJSONColonFromQuote(KStringView sText, KStringView::size_type iFirstQuote)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, ':', iFirstQuote + 1); iHit != KStringView::npos; iHit = kFind(sText, ':', iHit + 1))
	{
		// is the ':' preceded by the closing quote of a field name - '"name" :'?
		auto iQuote = iHit - 1;

		while (iQuote > iFirstQuote && (sText[iQuote] == ' ' || sText[iQuote] == '\t'))
		{
			--iQuote;
		}

		if (sText[iQuote] != '"')
		{
			continue;
		}

		// the name test compares backwards from the closing quote, so the start
		// of the name is only searched for names that end like a credential
		if (!kIsSecretJSONName(sText.substr(0, iQuote)))
		{
			continue;
		}

		// the name runs back from the closing quote to its opening one
		auto iStart = iQuote;

		while (iStart > 0 && kIsNameChar(sText[iStart - 1]))
		{
			--iStart;
		}

		if (iStart == 0 || sText[iStart - 1] != '"')
		{
			continue;
		}

		// the value: a string, or an array that starts with one
		auto iValue = kSkipJSONSpace(sText, iHit + 1);

		if (iValue < sText.size() && sText[iValue] == '[')
		{
			iValue = kSkipJSONSpace(sText, iValue + 1);
		}

		// a number, an object, null - not a credential we can recognise
		if (iValue >= sText.size() || sText[iValue] != '"')
		{
			continue;
		}

		// an empty string holds no credential
		if (iValue + 1 >= sText.size() || sText[iValue + 1] == '"')
		{
			continue;
		}

		// never redact a redaction - the passes overlap by design. The test is
		// BOUNDED TO THIS VALUE: kIsRedacted looks for the marker anywhere, and
		// over the rest of the text an already masked field further along would
		// answer for this one and leave a live credential in place.
		auto iEnd = iValue + 1;

		while (iEnd < sText.size() && sText[iEnd] != '"')
		{
			iEnd += (sText[iEnd] == '\\') ? 2 : 1;
		}

		if (iEnd <= sText.size() && !kIsRedacted(sText.substr(iValue + 1, iEnd - iValue - 1)))
		{
			return iHit;
		}
	}

	return KStringView::npos;

} // kFindSecretJSONColonFromQuote

//---------------------------------------------------------------------------
/// Finds the password in the userinfo of a URL, "scheme://user:password@host",
/// from iPos on. memchr scans for the '@', and only its hits read the text before
/// it: back to white space, a quote, a bracket or a '/'. A ':' in that run
/// separates the password - an e-mail address has none.
///
/// WITHOUT THE "//" OF A SCHEME THE HOST HAS TO CARRY A PORT OR A PATH, and a
/// false positive in production is why. A label before an address parses exactly
/// like a user before a password, so "email:joe@example.com" became
/// "email:[redacted]@example.com", and so did every "from:", "to:" and "contact:"
/// in every line that klog redacts. What separates the two is what follows the
/// host: a scheme-less connection string is "bob:pw@db1:3306/app" and an address
/// is "joe@example.com," - a port or a path says authority, and an address has
/// neither. "mailto:bob@example.com" falls out of the same rule.
/// @return the password, or an empty view if there is none
static KStringView kFindUserinfoPassword(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iAt = kFind(sText, '@', iPos); iAt != KStringView::npos; iAt = kFind(sText, '@', iAt + 1))
	{
		auto iRun   = iAt;
		auto iColon = KStringView::npos;

		while (iRun > 0)
		{
			auto ch = sText[iRun - 1];

			if (kIsValueEnd(ch) || ch == '/' || ch == '<' || ch == '(' || ch == '[')
			{
				break;
			}

			if (ch == ':')
			{
				// the leftmost colon remains
				iColon = iRun - 1;
			}

			--iRun;
		}

		// an e-mail address has no colon
		if (iColon == KStringView::npos)
		{
			continue;
		}

		bool bAfterScheme = iRun > 1 && sText[iRun - 1] == '/' && sText[iRun - 2] == '/';

		if (iRun > 0 && sText[iRun - 1] == '/' && !bAfterScheme)
		{
			continue;
		}

		if (!bAfterScheme)
		{
			// no scheme: the host has to be followed by a port or a path
			auto iHost = iAt + 1;

			while (iHost < sText.size() && kIsNameChar(sText[iHost]))
			{
				++iHost;
			}

			if (iHost >= sText.size()
			 || !(sText[iHost] == '/'
			   || (sText[iHost] == ':' && iHost + 1 < sText.size()
			    && sText[iHost + 1] >= '0' && sText[iHost + 1] <= '9')))
			{
				continue;
			}
		}

		auto sPassword = sText.substr(iColon + 1, iAt - iColon - 1);

		if (sPassword.empty() || kIsRedacted(sPassword))
		{
			continue;
		}

		return sPassword;
	}

	return {};

} // kFindUserinfoPassword

//---------------------------------------------------------------------------
/// Redact bearer credentials from a log message before it is written anywhere.
///
/// WHY THIS LIVES IN THE LOG WRITER rather than at each call site: in one sweep
/// of a single application SIX separate places were found
/// logging a live credential -- a full IDP access token, a full JWT on every
/// authenticated call, an OAuth token response, a Slack token into a WARNING log,
/// and, worst, two sites that logged the CONFIGURED token on a failed auth, so a
/// deliberately wrong guess made the server write the right answer into the log.
/// Those were all fixed, but the seventh will be written next month. Per-site
/// discipline has already been tried and has already failed; this is the net.
///
/// It redacts five shapes:
///   eyJ....  a JWT -- three base64url runs separated by dots. The "eyJ" prefix
///            is '{"' base64url-encoded, so every JWT begins with it.
///   Bearer <tok>          the value only, the scheme is kept
///   Authorization: <val>  the value only, the header name and the scheme are
///                         kept: Bearer, Basic, Digest, Negotiate, NTLM, and a
///                         word that ends with "key" or "token". Of any other
///                         first word, it and the word after it are redacted
///   password=<val>        the value of a parameter with a name of a credential,
///                         the name is kept (see kEndsWithSecretName())
///   user:<pass>@host      the password in the userinfo of a URL, with or
///                         without a scheme
/// Header names and authentication schemes are case insensitive in HTTP, so
/// "Bearer" and "Authorization" are recognised in any case, and so are the
/// names of parameters.
///
/// THE GUARD MATTERS MORE THAN THE REDACTION. This runs on every log line at
/// every level, so the common path must be a few memchr scans for rare characters
/// that all miss. Only a line that already looks like it carries a credential
/// pays for the copy.
///
/// It is a safety net and not a guarantee: a token logged under a name this does
/// not recognise, or in a shape that is not a JWT, still gets through. Do not
/// treat it as permission to log credentials.
///
/// @param sMessage the message to log
/// @param sBuffer receives the redacted copy of the message
/// @return true if sBuffer holds the message to log, false if sMessage needs no
/// change and sBuffer is left alone - the caller owns both, so neither result
/// can outlive what it points into
//---------------------------------------------------------------------------
bool kRedactCredentials(KStringView sMessage, KString& sBuffer)
//---------------------------------------------------------------------------
{
	// the fast path: no copy, no allocation, no scan beyond these six - the anchors
	// are the rare characters 'J', 'b', 'z', '=', '"' and '@'
	if (DEKAF2_LIKELY(kFindJWT(sMessage)                            == KStringView::npos
	               && kFindAnyCase(sMessage, "bearer ",        'b') == KStringView::npos
	               && kFindAnyCase(sMessage, "authorization:", 'z') == KStringView::npos
	               && kFindSecretParameter(sMessage).empty()
	               && kFindSecretJSONColon(sMessage)                == KStringView::npos
	               && kFindUserinfoPassword(sMessage).empty()))
	{
		return false;
	}

	sBuffer = sMessage;

	// - - - JWTs - - -
	// A JWT is header.payload.signature, all base64url. We only replace when the
	// shape really is three dotted runs, so a log line that merely mentions "eyJ"
	// in prose survives intact.
	for (KString::size_type iPos = 0; (iPos = kFindJWT(sBuffer, iPos)) != KString::npos; )
	{
		// a JWT starts at the beginning of a word - "keyJar.settings.production"
		// holds "eyJ" and dots as well, but is not one. A '=' separates, as in
		// "id_token=eyJ...": it is base64url padding only at the end of a run
		if (iPos > 0)
		{
			auto chBefore = sBuffer[iPos - 1];

			if (chBefore != '=' && (kIsTokenChar(chBefore) || chBefore == '.'))
			{
				iPos += 3;
				continue;
			}
		}

		auto iRun = iPos;
		int  iDots { 0 };

		while (iRun < sBuffer.size() && (kIsTokenChar(sBuffer[iRun]) || sBuffer[iRun] == '.'))
		{
			if (sBuffer[iRun] == '.')
			{
				++iDots;
			}
			++iRun;
		}

		// a masked JWT still begins with "eyJ" and still holds dots, so it would
		// match again on a second call - the marker is what tells them apart
		if (iDots >= 2 && (iRun - iPos) > 20 && !kIsRedacted(KStringView(sBuffer).substr(iPos, iRun - iPos)))
		{
			auto sMasked = kMaskSecret(KStringView(sBuffer).substr(iPos, iRun - iPos));

			sBuffer.replace(iPos, iRun - iPos, sMasked);
			iPos += sMasked.size();  // so we do not rescan the replacement
		}
		else
		{
			iPos += 3;
		}
	}

	// - - - "Authorization: [scheme] <value>" and a bare "Bearer <token>" - - -
	// The header name AND the scheme are kept: "which auth did it try?" is a real
	// debugging question and answering it costs nothing. All words match in any
	// case, and white space may follow the colon of the header, or none at all.
	//
	// ORDER AND SCHEME-SKIPPING BOTH MATTER, and a test caught this. Handling
	// "Bearer " first turned "Authorization: Bearer sk-live-..." into
	// "Authorization: Bearer [redacted]", and then the "Authorization: " pass took
	// the word *Bearer* as its value and ate it too. So Authorization goes first and
	// steps over a recognised scheme word before redacting what follows; the later
	// Bearer pass then finds an already-redacted value and leaves it alone.
	//
	// A scheme that is not in the list is no value of one word either: the header is
	// "<scheme> <credentials>" (RFC 9110 section 11.6.2), and only redacting the
	// first word left the credentials of "Authorization: DeepL-Auth-Key <key>"
	// readable. The schemes of services name the kind of their credentials, so a
	// first word of letters, '-' and '_' that ends with "key" or "token" and that
	// another word follows is a scheme as well. A credential nearly always has a
	// digit, so a word with a digit is no scheme, and "key" has to end the word:
	// about one in a thousand random base64 credentials of 40 characters contains
	// "key" somewhere.
	// Any other first word cannot be told from a credential that other text
	// follows, so it is redacted together with the word after it.
	struct Prefix
	{
		KStringView sWord;    // in lowercase
		char        chAnchor; // a rare letter of sWord
		bool        bHeader;  // the value may start with a scheme
	};

	static constexpr Prefix      Prefixes[] = { { "authorization:", 'z', true }, { "bearer ", 'b', false } };
	static constexpr KStringView Schemes[]  = { "bearer", "basic", "digest", "negotiate", "ntlm" };

	// is this the name of a scheme - a listed one, or one that names the kind of its
	// credentials, as "DeepL-Auth-Key", "ApiKey" or "Token"?
	auto IsScheme = [](KStringView sWord)
	{
		for (auto sScheme : Schemes)
		{
			if (kCaselessEqualLeft(sWord, sScheme))
			{
				return true;
			}
		}

		for (auto ch : sWord)
		{
			if (!(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && ch != '-' && ch != '_')
			{
				return false;
			}
		}

		return kEndsWithAnyCase(sWord, "key") || kEndsWithAnyCase(sWord, "token");
	};

	for (const auto& Prefix : Prefixes)
	{
		for (KString::size_type iPos = 0; (iPos = kFindAnyCase(sBuffer, Prefix.sWord, Prefix.chAnchor, iPos)) != KString::npos; )
		{
			iPos += Prefix.sWord.size();

			// the value runs to the end of the line, to a quote or to a backtick
			KStringView sValue(sBuffer);
			sValue.remove_prefix(iPos);
			sValue = sValue.substr(0, sValue.find_first_of("\n\r\"'`"));

			auto sSecret = kGetWord(sValue);
			auto sNext   = kFirstWord(sValue);

			if (Prefix.bHeader && !sNext.empty())
			{
				if (IsScheme(sSecret))
				{
					// the scheme survives the redaction
					sSecret = sNext;
				}
				else
				{
					// an unknown scheme, or a credential that other text follows - the
					// next word of the line is redacted as well
					sSecret = KStringView(sSecret.data(), sNext.data() + sNext.size() - sSecret.data());
				}
			}

			// never redact a redaction -- the passes overlap by design
			if (sSecret.empty() || kIsRedacted(sSecret))
			{
				continue;
			}

			auto iSecret = static_cast<KString::size_type>(sSecret.data() - sBuffer.data());

			auto sMasked = kMaskSecret(sSecret);

			sBuffer.replace(iSecret, sSecret.size(), sMasked);
			iPos = iSecret + sMasked.size();
		}
	}

	// - - - "password=<value>" and "user:<password>@host" - - -
	// each finder returns the secret as a view into sBuffer, from iPos on
	auto RedactAll = [&sBuffer](KStringView (*Find)(KStringView, KStringView::size_type))
	{
		for (KString::size_type iPos = 0;;)
		{
			auto sSecret = Find(sBuffer, iPos);

			if (sSecret.empty())
			{
				break;
			}

			auto iStart = static_cast<KString::size_type>(sSecret.data() - sBuffer.data());
			auto sMasked = kMaskSecret(sSecret);

			sBuffer.replace(iStart, sSecret.size(), sMasked);

			// FORWARD PROGRESS IS NOT OPTIONAL HERE. kFindUserinfoPassword anchors
			// on the '@' that FOLLOWS the password, so the position just past the
			// replacement is that '@' - and resuming there hands the next call the
			// same match again. Only the already-redacted test inside the finder
			// then ends the loop, which makes a logging path that runs on every
			// line depend on it for termination rather than for correctness. One
			// character of forced progress removes that dependency.
			auto iNext = iStart + sMasked.size();

			iPos = (iNext > iPos) ? iNext : iPos + 1;

			if (iPos >= sBuffer.size())
			{
				break;
			}
		}
	};

	RedactAll(kFindSecretParameter);
	RedactAll(kFindUserinfoPassword);

	// - - - '"name": "value"' in a JSON body - - -
	// The passes above all key off a character that a JSON body does not use to
	// separate a name from its value, so a service that answers in JSON - which is
	// to say every service - got through them untouched. A body like
	// {"ApiTokens":["..."]} is exactly what an error from a provider carries, and
	// exactly what gets pasted into a ticket.
	//
	// This one does not go through RedactAll: a value here can be an ARRAY of
	// strings, and all of its elements belong to one name, so the whole value is
	// walked at the colon that introduces it.
	for (KString::size_type iPos = 0;;)
	{
		auto iColon = kFindSecretJSONColon(sBuffer, iPos);

		if (iColon == KString::npos)
		{
			break;
		}

		auto iValue  = kSkipJSONSpace(sBuffer, iColon + 1);
		bool bArray  = sBuffer[iValue] == '[';

		if (bArray)
		{
			iValue = kSkipJSONSpace(sBuffer, iValue + 1);
		}

		while (iValue < sBuffer.size() && sBuffer[iValue] == '"')
		{
			// the closing quote of the value, with backslash escapes stepped over
			auto iEnd = iValue + 1;

			while (iEnd < sBuffer.size() && sBuffer[iEnd] != '"')
			{
				iEnd += (sBuffer[iEnd] == '\\') ? 2 : 1;
			}

			if (iEnd >= sBuffer.size())
			{
				break;
			}

			auto iLen = iEnd - iValue - 1;

			if (iLen > 0 && !kIsRedacted(KStringView(sBuffer).substr(iValue + 1, iLen)))
			{
				auto sMasked = kMaskSecret(KStringView(sBuffer).substr(iValue + 1, iLen));

				sBuffer.replace(iValue + 1, iLen, sMasked);
				iEnd = iValue + 1 + sMasked.size();
			}

			iValue = iEnd + 1;  // past the closing quote

			if (!bArray)
			{
				break;
			}

			iValue = kSkipJSONSpace(sBuffer, iValue);

			if (iValue >= sBuffer.size() || sBuffer[iValue] != ',')
			{
				break;
			}

			iValue = kSkipJSONSpace(sBuffer, iValue + 1);
		}

		iPos = iValue;
	}

	return true;

} // kRedactCredentials

//---------------------------------------------------------------------------
KString kRedactCredentials(KStringView sMessage)
//---------------------------------------------------------------------------
{
	KString sBuffer;

	if (kRedactCredentials(sMessage, sBuffer))
	{
		return sBuffer;
	}

	// nothing needed redacting - the two-argument form left sBuffer alone
	return KString { sMessage };

} // kRedactCredentials

//---------------------------------------------------------------------------
/// Finds from iPos on the column list of an insert statement, as in "(name,
/// password) values", that holds a column whose name denotes a credential. Only
/// a "values" after a ')' counts, so "on duplicate key update x=values(x)" does not.
/// @param Secret receives for each column of the list whether its name denotes
/// a credential
/// @return the index after "values", or npos if there is no such column list
static KStringView::size_type kFindSQLInsertWithSecret(KStringView sSQL, KStringView::size_type iPos, std::vector<bool>& Secret)
//---------------------------------------------------------------------------
{
	static constexpr KStringView sKeyword { "values" };

	for (;;)
	{
		auto iValues = kFindAnyCase(sSQL, sKeyword, 'v', iPos);

		if (iValues == KStringView::npos)
		{
			return KStringView::npos;
		}

		iPos = iValues + sKeyword.size();

		if ((iPos < sSQL.size() && kIsNameChar(sSQL[iPos])) || (iValues > 0 && kIsNameChar(sSQL[iValues - 1])))
		{
			// part of a longer word
			continue;
		}

		auto iClose = iValues;

		while (iClose > 0 && (sSQL[iClose - 1] == ' ' || sSQL[iClose - 1] == '\t' || sSQL[iClose - 1] == '\n' || sSQL[iClose - 1] == '\r'))
		{
			--iClose;
		}

		if (iClose == 0 || sSQL[iClose - 1] != ')')
		{
			continue;
		}

		--iClose;

		auto iOpen = sSQL.rfind('(', iClose);

		if (iOpen == KStringView::npos)
		{
			continue;
		}

		auto sColumns = sSQL.substr(iOpen + 1, iClose - iOpen - 1);
		bool bSecret { false };

		Secret.clear();

		for (;;)
		{
			auto iComma  = sColumns.find(',');
			auto sColumn = sColumns.substr(0, iComma);

			sColumn.Trim(" \t\r\n`\"[]");

			Secret.push_back(kIsSecretJSONName(sColumn));
			bSecret |= Secret.back();

			if (iComma == KStringView::npos)
			{
				break;
			}

			sColumns.remove_prefix(iComma + 1);
		}

		if (bSecret)
		{
			return iPos;
		}
	}

} // kFindSQLInsertWithSecret

//---------------------------------------------------------------------------
/// Masks the strings in the columns that Secret marks, in the rows of an insert
/// statement that start at iPos, as in "('bob', 'value'), ('ann', 'value')". In a
/// value that is an expression, as in "aes_encrypt('value', @key)", every string
/// is masked.
/// @param bMasked is set to true if a string was masked
/// @return the index after the last row
static KString::size_type kMaskSQLRows(KString& sSQL, KString::size_type iPos, const std::vector<bool>& Secret, bool& bMasked)
//---------------------------------------------------------------------------
{
	for (;;)
	{
		iPos = kSkipJSONSpace(sSQL, iPos);

		if (iPos >= sSQL.size() || sSQL[iPos] != '(')
		{
			return iPos;
		}

		std::size_t iColumn { 0 };
		std::size_t iDepth  { 0 };

		for (++iPos; iPos < sSQL.size(); ++iPos)
		{
			auto ch = sSQL[iPos];

			if (ch == '\'' || ch == '"')
			{
				auto sValue = kGetQuotedValue(sSQL, iPos);
				auto iStart = iPos + 1;
				auto iSize  = sValue.size();

				if (iColumn < Secret.size() && Secret[iColumn] && !sValue.empty() && !kIsRedacted(sValue))
				{
					auto sMasked = kMaskSecret(sValue);

					sSQL.replace(iStart, iSize, sMasked);
					iSize   = sMasked.size();
					bMasked = true;
				}

				// at the closing quote, the loop steps past it
				iPos = iStart + iSize;
			}
			else if (ch == '(')
			{
				++iDepth;
			}
			else if (ch == ')')
			{
				if (iDepth == 0)
				{
					break;
				}

				--iDepth;
			}
			else if (ch == ',' && iDepth == 0)
			{
				++iColumn;
			}
		}

		// past the ')' of the row, to a ',' and the next row
		iPos = kSkipJSONSpace(sSQL, iPos + 1);

		if (iPos >= sSQL.size() || sSQL[iPos] != ',')
		{
			return iPos;
		}

		++iPos;
	}

} // kMaskSQLRows

//---------------------------------------------------------------------------
bool kRedactSQLCredentials(KStringView sSQL, KString& sBuffer)
//---------------------------------------------------------------------------
{
	std::vector<bool> Secret;

	auto iPos = kFindSQLInsertWithSecret(sSQL, 0, Secret);

	if (iPos == KStringView::npos)
	{
		return kRedactCredentials(sSQL, sBuffer);
	}

	KString sInsert { sSQL };
	bool    bMasked { false };

	while (iPos != KStringView::npos)
	{
		iPos = kMaskSQLRows(sInsert, iPos, Secret, bMasked);
		iPos = kFindSQLInsertWithSecret(sInsert, iPos, Secret);
	}

	if (kRedactCredentials(sInsert, sBuffer))
	{
		return true;
	}

	if (bMasked)
	{
		sBuffer = std::move(sInsert);
	}

	return bMasked;

} // kRedactSQLCredentials

//---------------------------------------------------------------------------
KString kRedactSQLCredentials(KStringView sSQL)
//---------------------------------------------------------------------------
{
	KString sBuffer;

	if (kRedactSQLCredentials(sSQL, sBuffer))
	{
		return sBuffer;
	}

	return KString { sSQL };

} // kRedactSQLCredentials

DEKAF2_NAMESPACE_END
