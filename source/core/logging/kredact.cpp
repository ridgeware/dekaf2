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

DEKAF2_NAMESPACE_BEGIN

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
/// Is this a character that ends a value in a log line - white space or a quote?
static inline bool kIsValueEnd(char ch)
//---------------------------------------------------------------------------
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\'';
}

//---------------------------------------------------------------------------
/// Does sName end with sWord, in any case? sWord is in lowercase. The comparison
/// runs from the back, as the last characters already differ for nearly all names
/// of parameters - this runs for each '=' of a line.
static inline bool kEndsWithAnyCase(KStringView sName, KStringView sWord)
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
/// credential? Short names like "pass" or "key" count only in a URL, after a '?'
/// or '&', as "pass=3 fail=0" or "key=..." are ordinary output elsewhere.
///
/// This runs for each '=' of a line, so it compares from the '=' backwards and
/// never reads the whole name: the last character selects the few candidates,
/// and nearly all names of ordinary parameters fail at their last or second to
/// last character.
/// @param sBefore the text up to the '='
static bool kEndsWithSecretName(KStringView sBefore)
//---------------------------------------------------------------------------
{
	// does sBefore end with sWord as a whole name?
	auto IsName = [sBefore](KStringView sWord)
	{
		return kEndsWithAnyCase(sBefore, sWord)
		    && (sBefore.size() == sWord.size() || !kIsNameChar(sBefore[sBefore.size() - sWord.size() - 1]));
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
			return IsName("password") || IsName("passwd") || EndsWith("_password") || IsNameInURL("pwd");

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
/// Finds the value of a parameter "name=value" whose name says that it holds a
/// credential, from iPos on. memchr scans for the '=', and only its hits compare
/// the name before it. The value runs to white space or a quote, and in a URL also
/// to the next '&' or '#' - in other text these may be part of a password.
/// @return the value, or an empty view if there is none
static KStringView kFindSecretParameter(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, '=', iPos); iHit != KStringView::npos; iHit = kFind(sText, '=', iHit + 1))
	{
		if (!kEndsWithSecretName(sText.substr(0, iHit)))
		{
			continue;
		}

		// only now the start of the name, to see if it is in a URL
		auto iName = iHit;

		while (iName > 0 && kIsNameChar(sText[iName - 1]))
		{
			--iName;
		}

		bool bInURL = iName > 0 && (sText[iName - 1] == '?' || sText[iName - 1] == '&');
		auto iEnd   = iHit + 1;

		while (iEnd < sText.size() && !kIsValueEnd(sText[iEnd]) && !(bInURL && (sText[iEnd] == '&' || sText[iEnd] == '#')))
		{
			++iEnd;
		}

		auto sValue = sText.substr(iHit + 1, iEnd - iHit - 1);

		// never redact a redaction - "access_token=[redacted]" from the JWT pass
		if (!sValue.empty() && !sValue.starts_with("[redacted]"))
		{
			return sValue;
		}
	}

	return {};

} // kFindSecretParameter

//---------------------------------------------------------------------------
/// Finds the password in the userinfo of a URL, "user:password@host", from iPos
/// on, also without a scheme. memchr scans for the '@', and only its hits read the
/// text before it: back to white space, a quote, a bracket or a '/'. A '/' must be
/// the one of a "//" after the scheme, else the '@' is part of a path. A ':' in
/// that run separates the password - an e-mail address has none, and "mailto:"
/// and "sip:" are schemes, not users.
/// @return the password, or an empty view if there is none
static KStringView kFindUserinfoPassword(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	static constexpr KStringView Schemes[] = { "mailto", "sip", "sips", "xmpp" };

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

		auto sUser     = sText.substr(iRun, iColon - iRun);
		auto sPassword = sText.substr(iColon + 1, iAt - iColon - 1);

		if (sPassword.empty() || sPassword.starts_with("[redacted]"))
		{
			continue;
		}

		if (!bAfterScheme)
		{
			bool bIsScheme { false };

			for (auto sScheme : Schemes)
			{
				if (kCaselessEqualLeft(sUser, sScheme))
				{
					bIsScheme = true;
					break;
				}
			}

			if (bIsScheme)
			{
				continue;
			}
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
///   Authorization: <val>  the value only, the header name is kept
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
	// the fast path: no copy, no allocation, no scan beyond these five - the anchors
	// are the rare characters 'J', 'b', 'z', '=' and '@'
	if (DEKAF2_LIKELY(kFindJWT(sMessage)                            == KStringView::npos
	               && kFindAnyCase(sMessage, "bearer ",        'b') == KStringView::npos
	               && kFindAnyCase(sMessage, "authorization:", 'z') == KStringView::npos
	               && kFindSecretParameter(sMessage).empty()
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

		if (iDots >= 2 && (iRun - iPos) > 20)
		{
			sBuffer.replace(iPos, iRun - iPos, "[redacted]");
			iPos += 10;  // length of the replacement, so we do not rescan it
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
	struct Prefix
	{
		KStringView sWord;    // in lowercase
		char        chAnchor; // a rare letter of sWord
	};

	static constexpr Prefix      Prefixes[] = { { "authorization:", 'z' }, { "bearer ", 'b' } };
	static constexpr KStringView Schemes[]  = { "bearer ", "basic ", "digest ", "token " };

	for (const auto& Prefix : Prefixes)
	{
		for (KString::size_type iPos = 0; (iPos = kFindAnyCase(sBuffer, Prefix.sWord, Prefix.chAnchor, iPos)) != KString::npos; )
		{
			auto iValue = iPos + Prefix.sWord.size();

			while (iValue < sBuffer.size() && (sBuffer[iValue] == ' ' || sBuffer[iValue] == '\t'))
			{
				++iValue;
			}

			// step over "Bearer" / "Basic" / ... so the scheme survives the redaction
			for (auto sScheme : Schemes)
			{
				if (kCaselessBeginsWithLeft(KStringView(sBuffer).substr(iValue), sScheme))
				{
					iValue += sScheme.size();
					break;
				}
			}

			// never redact a redaction -- the passes overlap by design
			if (sBuffer.compare(iValue, 10, "[redacted]", 10) == 0)
			{
				iPos = iValue + 10;
				continue;
			}

			auto iEnd = iValue;

			// the value runs to whitespace, a quote, or the end of the line
			while (iEnd < sBuffer.size()
			    && sBuffer[iEnd] != ' '  && sBuffer[iEnd] != '\t'
			    && sBuffer[iEnd] != '\n' && sBuffer[iEnd] != '\r'
			    && sBuffer[iEnd] != '"'  && sBuffer[iEnd] != '\'')
			{
				++iEnd;
			}

			if (iEnd > iValue)
			{
				sBuffer.replace(iValue, iEnd - iValue, "[redacted]");
				iPos = iValue + 10;  // length of "[redacted]"
			}
			else
			{
				iPos = iValue;
			}
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
			sBuffer.replace(iStart, sSecret.size(), "[redacted]");
			iPos = iStart + 10;  // length of "[redacted]"
		}
	};

	RedactAll(kFindSecretParameter);
	RedactAll(kFindUserinfoPassword);

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

DEKAF2_NAMESPACE_END
