/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

#include "kxterm.h"
#include "kwrite.h"
#include "kctype.h"
#include "kutf8.h"
#include "klog.h"
#include <string>

#ifndef DEKAF2_IS_WINDOWS
	#include <termios.h>
#else
	#include <windows.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void kSetTerminal(int iInputDevice, bool bRaw, uint8_t iMinAvail, uint8_t iMaxWait100ms)
//-----------------------------------------------------------------------------
{

#ifndef DEKAF2_IS_WINDOWS

	struct termios Settings;

	::tcgetattr(iInputDevice, &Settings);

	if (bRaw)
	{
		Settings.c_lflag    &= ~(ICANON | ECHO | ECHONL);
	}
	else
	{
		Settings.c_lflag    |=  (ICANON | ECHO | ECHONL);
	}

	Settings.c_cc[VMIN]  = iMinAvail;
	Settings.c_cc[VTIME] = iMaxWait100ms;

	kDebug(3, "setting terminal to {}, min chars {}, timeout {}ms",
	       bRaw ? "raw char non echo mode" : "line mode with echo",
	       iMinAvail, iMaxWait100ms * 100);

	::tcsetattr(iInputDevice, TCSANOW, &Settings);

#else

	HANDLE hStdin = GetStdHandle(iInputDevice);

	DWORD mode = 0;
	GetConsoleMode(hStdin, &mode);

	if (bRaw)
	{
		kDebug(3, "setting terminal to raw char non echo mode");
		SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
	}
	else
	{
		kDebug(3, "setting terminal to line mode with echo");
		SetConsoleMode(hStdin, mode | (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
	}

#endif

} // SetTerm


//-----------------------------------------------------------------------------
bool KXTermCodes::HasRGBColors()
//-----------------------------------------------------------------------------
{
	KStringViewZ sTerm = ::getenv("COLORTERM");
	kDebug(3, "env COLORTERM is '{}'", sTerm);

	if (sTerm.In("truecolor,24bit"))
	{
		kDebug(3, "terminal has RGB colors");
		return true;
	}

	sTerm = ::getenv("TERM");
	kDebug(3, "env TERM is '{}'", sTerm);

	if (sTerm == "iterm" || sTerm.contains("truecolor") || sTerm.starts_with("vte"))
	{
		kDebug(3, "terminal has RGB colors");
		return true;
	}

	kDebug(3, "terminal does not have RGB colors");
	return false;

} // HasRGBColors

//-----------------------------------------------------------------------------
KString KXTermCodes::Color(RGB fg_rgb, RGB bg_rgb)
//-----------------------------------------------------------------------------
{
	return FGColor(fg_rgb) + BGColor(bg_rgb);
}

//-----------------------------------------------------------------------------
KString KXTermCodes::FGColor(RGB rgb)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[38;2{};{};{}m", rgb.Red, rgb.Green, rgb.Blue);
}

//-----------------------------------------------------------------------------
KString KXTermCodes::BGColor(RGB rgb)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[48;2{};{};{}m", rgb.Red, rgb.Green, rgb.Blue);
}

//-----------------------------------------------------------------------------
KString KXTermCodes::Color(ColorCode FGC, ColorCode BGC)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[{};{}m", GetFGColor(FGC), GetBGColor(BGC));
}

//-----------------------------------------------------------------------------
KString KXTermCodes::FGColor(ColorCode CC)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[{}m", GetFGColor(CC));
}

//-----------------------------------------------------------------------------
KString KXTermCodes::BGColor(ColorCode CC)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[{}m", GetBGColor(CC));
}

//-----------------------------------------------------------------------------
KString KXTermCodes::GetFGColor(ColorCode CC)
//-----------------------------------------------------------------------------
{
	if (CC >= 10)
	{
		return kFormat("1;3{}", CC - 10);
	}
	else
	{
		return kFormat("3{}", CC - 0);
	}
}

//-----------------------------------------------------------------------------
KString KXTermCodes::GetBGColor(ColorCode CC)
//-----------------------------------------------------------------------------
{
	if (CC >= 10)
	{
		return kFormat("1;4{}", CC - 10);
	}
	else
	{
		return kFormat("4{}", CC - 0);
	}
}


//-----------------------------------------------------------------------------
KXTerm::KXTerm(int iInputDevice, int iOutputDevice, uint16_t iRows, uint16_t iColumns)
//-----------------------------------------------------------------------------
: m_iInputDevice  (iInputDevice)
, m_iOutputDevice (iOutputDevice)
, m_iRows         (iRows)
, m_iColumns      (iColumns)
, m_bHasRGBColors (KXTermCodes::HasRGBColors())
{

#ifdef DEKAF2_IS_WINDOWS

	kSetTerminal(m_iInputDevice, true, 1, 0);

#else

	m_Termios = new struct termios;

	if (m_Termios)
	{
		bool bIsRealTerm = false;

		auto iResult = ::tcgetattr(m_iInputDevice, m_Termios);

		if (!iResult)
		{
			termios Changed     = *m_Termios;
			Changed.c_lflag    &= ~(ICANON | ECHO | ECHONL);
			Changed.c_cc[VMIN]  = 1;
			Changed.c_cc[VTIME] = 0;

			kDebug(3, "setting terminal to {}, min chars {}, timeout {}ms",
			          "raw char non echo mode", 1, 0 * 100);

			if (!::tcsetattr(m_iInputDevice, TCSANOW, &Changed))
			{
				termios Set;
				if (!::tcgetattr(m_iInputDevice, &Set))
				{
					if (!memcmp(&Changed, &Set, sizeof(termios)))
					{
						// we could read back our settings, so we
						// assume this is really a terminal
						bIsRealTerm = true;
					}
					else
					{
						kDebug(2, "could not read back new config");
						// nevertheless we set bIsRealTerm to true,
						// as this seems to happen with real terminals,
						// too. Needs further investigation.
						bIsRealTerm = true;
					}
				}
			}
		}
		else
		{
			// the termios struct is not valid
			delete m_Termios;
			m_Termios = nullptr;
		}

		if (!bIsRealTerm)
		{
			kDebug(1, "this is not a terminal: {}", strerror(errno));
			// this is not a real terminal
			m_iIsTerminal = 0;
		}
	}

#endif

	if (iRows == 0 || iColumns == 0)
	{
		QueryTermSize();
	}

} // ctor

//-----------------------------------------------------------------------------
KXTerm::~KXTerm()
//-----------------------------------------------------------------------------
{
	while (m_iChangedWindowTitle)
	{
		RestoreWindowTitle();
	}

#ifdef DEKAF2_IS_WINDOWS

	kSetTerminal(m_iInputDevice, false, 1, 0);

#else

	if (m_Termios)
	{
		::tcsetattr(m_iInputDevice, TCSANOW, m_Termios);

		delete m_Termios;
		m_Termios = nullptr;
	}

#endif

} // dtor

//-----------------------------------------------------------------------------
void KXTerm::QueryTermSize()
//-----------------------------------------------------------------------------
{
	uint16_t iRow, iCol;

	if (GetCursor(iRow, iCol))
	{
		IntSetCursor(9999, 9999, false); // false = permit too large values
		GetCursor(m_iRows, m_iColumns);
		SetCursor(iRow, iCol);
	}
	else
	{
		kDebug(2, "cannot query terminal size");
		// assume defaults
		m_iRows    = 25;
		m_iColumns = 80;
	}

	kDebug(2, "Rows: {}, Columns: {}", m_iRows, m_iColumns);

} // QueryTermSize

//-----------------------------------------------------------------------------
bool KXTerm::GetCursor(uint16_t& iRow, uint16_t& iColumn)
//-----------------------------------------------------------------------------
{
	if (!IsTerminal())
	{
		return false;
	}

	auto sResponse = QueryTerminal("\033[6n");

	if (sResponse.remove_prefix('['))
	{
		auto Parts = sResponse.Split(";");

		if (Parts.size() == 2)
		{
			m_iCursorRow    = iRow    = Parts[0].UInt16();
			m_iCursorColumn = iColumn = Parts[1].UInt16();
			kDebug(3, "row {} col {}", iRow, iColumn);
			return true;
		}
	}

	kDebug(1, "could not get cursor position");
	return false;

} // GetCursor

//-----------------------------------------------------------------------------
void KXTerm::IntSetCursor(uint16_t iRow, uint16_t iColumn, bool bCheck)
//-----------------------------------------------------------------------------
{
	if (IsTerminal())
	{
		if (bCheck && CursorLimits())
		{
			// verify requested position for validity
			iColumn = CheckColumn (iColumn);
			iRow    = CheckRow    (iRow);
		}
		Command(Csi, kFormat("{};{}H", iRow, iColumn));
		m_iCursorRow    = iRow;
		m_iCursorColumn = iColumn;
	}

} // SetCursor

//-----------------------------------------------------------------------------
void KXTerm::CurToColumn(uint16_t iColumn)
//-----------------------------------------------------------------------------
{
	iColumn = CheckColumn(iColumn);
	Command(Csi, kFormat("{}G", iColumn));
	m_iCursorColumn = iColumn;
}

//-----------------------------------------------------------------------------
void KXTerm::CurLeft(uint16_t iColumns)
//-----------------------------------------------------------------------------
{
	if (iColumns)
	{
		iColumns = CheckColumnLeft(iColumns);
		Command(Csi, kFormat("{}D", iColumns));
		m_iCursorColumn -= iColumns;
	}
}

//-----------------------------------------------------------------------------
void KXTerm::CurRight(uint16_t iColumns)
//-----------------------------------------------------------------------------
{
	if (iColumns)
	{
		iColumns = CheckColumnRight(iColumns);
		Command(Csi, kFormat("{}C", iColumns));
		m_iCursorColumn += iColumns;
	}
}

//-----------------------------------------------------------------------------
void KXTerm::CurUp(uint16_t iRows)
//-----------------------------------------------------------------------------
{
	if (iRows)
	{
		iRows = CheckRowUp(iRows);
		Command(Csi, kFormat("{}A", iRows));
		m_iCursorRow -= iRows;
	}
}

//-----------------------------------------------------------------------------
void KXTerm::CurDown(uint16_t iRows)
//-----------------------------------------------------------------------------
{
	if (iRows)
	{
		iRows = CheckRowDown(iRows);
		Command(Csi, kFormat("{}B", iRows));
		m_iCursorRow += iRows;
	}
}

//-----------------------------------------------------------------------------
void KXTerm::ShowCursor(bool bOn) const
//-----------------------------------------------------------------------------
{
	Command(bOn ? KXTermCodes::CursorOn() : KXTermCodes::CursorOff());
}

//-----------------------------------------------------------------------------
void KXTerm::SetBlink(bool bOn) const
//-----------------------------------------------------------------------------
{
	Command(bOn ? KXTermCodes::CursorBlink() : KXTermCodes::CursorNoBlink());
}

//-----------------------------------------------------------------------------
bool KXTerm::SetWindowTitle(KStringView sWindowTitle)
//-----------------------------------------------------------------------------
{
	if (sWindowTitle != m_sLastWindowTitle)
	{
		m_sLastWindowTitle = sWindowTitle;
		++m_iChangedWindowTitle;
		Command("\033[22t"); // push
		Command("\033]0;");  // set
		Command(m_sLastWindowTitle);
		Command("\033\\");
		return true;
	}

	return false;

} // SetWindowTitle

//-----------------------------------------------------------------------------
void KXTerm::RestoreWindowTitle()
//-----------------------------------------------------------------------------
{
	if (m_iChangedWindowTitle)
	{
#ifdef DEKAF2_IS_MACOS
		// this is actually for the terminal app, and it only offers a reset, no stack
		Command("\033]2;\033\\"); // reset
#else
		Command("\033[23t");      // pop
#endif
		--m_iChangedWindowTitle;
		m_sLastWindowTitle = "\004\001";
	}

} // RestoreWindowTitle

//-----------------------------------------------------------------------------
void KXTerm::SaveCursor()
//-----------------------------------------------------------------------------
{
	m_iSavedCursorRow    = m_iCursorRow;
	m_iSavedCursorColumn = m_iCursorColumn;
	Command(KXTermCodes::SaveCursor());
}

//-----------------------------------------------------------------------------
void KXTerm::RestoreCursor()
//-----------------------------------------------------------------------------
{
	Command(KXTermCodes::RestoreCursor());
	m_iCursorRow    = m_iSavedCursorRow;
	m_iCursorColumn = m_iSavedCursorColumn;
}

//-----------------------------------------------------------------------------
void KXTerm::Home()
//-----------------------------------------------------------------------------
{
	Command(KXTermCodes::Home());
	m_iCursorRow    = 0;
	m_iCursorColumn = 0;
}

//-----------------------------------------------------------------------------
void KXTerm::ClearLine()
//-----------------------------------------------------------------------------
{
	Command(KXTermCodes::ClearLine());
	m_iCursorColumn = 0;
}

//-----------------------------------------------------------------------------
void KXTerm::CurToStartOfNextLine()
//-----------------------------------------------------------------------------
{
	Command(KXTermCodes::CurToStartOfNextLine());
	m_iCursorRow   += CheckRowDown(1);
	m_iCursorColumn = 0;
}

//-----------------------------------------------------------------------------
void KXTerm::CurToStartOfPrevLine()
//-----------------------------------------------------------------------------
{
	Command(KXTermCodes::CurToStartOfPrevLine());
	m_iCursorRow   -= CheckRowUp(1);
	m_iCursorColumn = 0;
}

//-----------------------------------------------------------------------------
KCodePoint KXTerm::Read() const
//-----------------------------------------------------------------------------
{
	return Unicode::CodepointFromUTF8Reader([this]()
	{
		return RawRead();

	}, EOF);
}

namespace {
inline constexpr uint8_t Control(uint8_t c) { return c - ('a' - 1); }
}

//-----------------------------------------------------------------------------
KCodePoint KXTerm::EscapeToControl(KCodePoint ch) const
//-----------------------------------------------------------------------------
{
	if (ch != '\033') return ch;

	ch = Read();

	switch (ch)
	{
		case '[':
			ch = Read();

			switch (ch)
			{
				case 'A': // CUR UP
					return Control('p');

				case 'B': // CUR DOWN
					return Control('n');

				case 'C': // CUR RIGHT
					return Control('f');

				case 'D': // CUR LEFT
					return Control('b');

				case '3':
					ch = Read();
					switch (ch)
					{
						case '~':   // DEL
							return Control('d');
					}
					break;

				default:
					kDebug(2, "ESC [{}", char(ch));
					break;
			}
			break;

		case 'b':
			return U'∫';

		case 'f':
			return U'ƒ';

		default:
			kDebug(2, "ESC {}", char(ch));
			break;
	}

	return 0;

} // EscapeToControl

//-----------------------------------------------------------------------------
bool KXTerm::EditLine(
	KStringView sPrompt,
	KString&    sLine,
	KStringView sPromptFormatStart,
	KStringView sPromptFormatEnd
)
//-----------------------------------------------------------------------------
{
	m_iCursorColumn = 0;

	if (!sPrompt.empty())
	{
		Command(sPromptFormatStart);
		Write(sPrompt);
		Command(sPromptFormatEnd);
	}

	Write(sLine);

	using UCString = std::basic_string<Unicode::codepoint_t>;

	UCString sUnicode;
	UCString sClipboard;

	if (!Unicode::ConvertUTF(sLine, sUnicode))
	{
		kDebug(2, "input is not UTF8: {}", sLine);
		return false;
	}

	auto iPos  = sUnicode.size();
	auto iLast = iPos;
	std::size_t iStoredPos = 0;
	bool bCurLineHasEdits = true;
	auto bCursorLimits = m_bCursorLimits;
	m_bCursorLimits = false;

	for (;;)
	{
		auto ch = Read();
		bool bRefreshWholeLine = false;

		// do escape processing first, replace common sequences with control codes
		ch = EscapeToControl(ch);

		if (!IsTerminal() && (ch.IsASCIICntrl() && ch != '\n'))
		{
			// a control char - we cannot display their effects on a non-terminal
			continue;
		}

		switch (ch)
		{
			case '\000': // this came from escape processing and did not
						 // find a replacement
				Beep();
				continue;

			case Unicode::END_OF_INPUT:
				return false;

			case Unicode::INVALID_CODEPOINT:
				kDebug(2, "invalid codepoint");
				Beep();
				break;

			case '\n':   // done ..
				sLine.clear();
				Unicode::ConvertUTF(sUnicode, sLine);
				m_History.Add(sLine);
				m_bCursorLimits = bCursorLimits;
				return true;

			case '\177': // BS
				if (iPos)
				{
					--iPos;
					sUnicode.erase(iPos, 1);
				}
				else
				{
					Beep();
					continue;
				}
				break;

			case Control('d'): // DELETE
				if (iPos < sUnicode.size())
				{
					sUnicode.erase(iPos, 1);
				}
				else if (sUnicode.empty())
				{
					// a ctrl-d on an empty line is like EOF
					Write("\n");
					return false;
				}
				else
				{
					Beep();
					continue;
				}
				break;

			case Control('a'): // goto start of line
				iPos = 0;
				break;

			case Control('e'): // goto end of line
				iPos = sUnicode.size();
				break;

			case Control('b'): // cur left
				if (iPos)
				{
					--iPos;
					iLast = iPos;
					CurLeft(1);
				}
				else
				{
					Beep();
				}
				continue;

			case Control('f'): // cur right
				if (iPos < sUnicode.size())
				{
					++iPos;
					iLast = iPos;
					CurRight(1);
				}
				else
				{
					Beep();
				}
				continue;

			case Control('n'): // cur down
				// if we have touched this line then it became the new active line
				// and has no history successor
				if (bCurLineHasEdits == false && m_History.HaveNewer())
				{
					sUnicode          = Unicode::ConvertUTF<UCString>(m_History.GetNewer());
					iPos              = sUnicode.size();
					bRefreshWholeLine = true;
				}
				else
				{
					Beep();
				}
				break;

			case Control('p'): // cur up
				if (bCurLineHasEdits)
				{
					m_History.Stash(Unicode::ConvertUTF<KString>(sUnicode));
				}
				if (m_History.HaveOlder())
				{
					if (!m_History.HaveStashed())
					{
						// save current edit
						m_History.Stash(Unicode::ConvertUTF<KString>(sUnicode));
					}
					sUnicode          = Unicode::ConvertUTF<UCString>(m_History.GetOlder());
					iPos              = sUnicode.size();
					bRefreshWholeLine = true;
					bCurLineHasEdits  = false;
				}
				else
				{
					Beep();
				}
				break;

			case Control('l'): // Clear
				ClearScreen();
				sUnicode.clear();
				iPos = 0;
				iLast = 0;
				// need to write prompt if existing
				continue;

			case Control('t'): // transpose last two chars
				if (iPos > 1)
				{
					auto c1 = sUnicode[iPos-1];
					auto c2 = sUnicode[iPos-2];
					sUnicode[iPos-1] = c2;
					sUnicode[iPos-2] = c1;
					bRefreshWholeLine = true;
					break;
				}
				else
				{
					Beep();
				}
				continue;

			case Control('k'): // clear line after cursor and move into clipboard
				sClipboard = sUnicode.substr(iPos, npos);
				sUnicode.erase(iPos, npos);
				ClearToEndOfLine();
				continue;

			case Control('u'): // clear line before cursor and move into clipboard
				sClipboard = sUnicode.substr(0, iPos);
				sUnicode.erase(0, iPos);
				iPos = 0;
				break;

			case Control('w'): // clear word before cursor and move into clipboard
				while (iPos > 0 &&  kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				while (iPos > 0 && !kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				sClipboard = sUnicode.substr(iPos, npos);
				sUnicode.erase(iPos, npos);
				break;

			case Control('v'): // paste clipboard (non-standard, ctrl-y pauses on mac)
				sUnicode += sClipboard;
				iPos += sClipboard.size();
				break;

			case Control('x'): // x-x toggle between start of line and current position
				if (iPos)
				{
					iStoredPos = iPos;
					iPos = 0;
				}
				else
				{
					iPos = std::min(iStoredPos, sUnicode.size());
				}
				break;

			case U'ƒ':  // move forward one word
				if (!IsTerminal() || iPos >= sUnicode.size())
				{
					Beep();
					continue;
				}
				while (iPos < sUnicode.size() && !kIsSpace(sUnicode[iPos])) { ++iPos; }
				while (iPos < sUnicode.size() &&  kIsSpace(sUnicode[iPos])) { ++iPos; }
				break;

			case U'∫':  // move backward one word
				if (!IsTerminal() || iPos == 0)
				{
					Beep();
					continue;
				}
				while (iPos > 0 &&  kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				while (iPos > 0 && !kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				break;

			case '\t':         // TAB
			case Control('r'): // search in history, use line until cursor as search string
				if (iPos == 0 || iPos > sUnicode.size())
				{
					Beep();
				}
				else
				{
					KString sSearch = Unicode::ConvertUTF<KString>(sUnicode.begin(), sUnicode.begin() + iPos-1);
					sUnicode = Unicode::ConvertUTF<UCString>(m_History.Find(sSearch, true));
					if (iPos > sUnicode.size())
					{
						iPos = sUnicode.size();
					}
					bCurLineHasEdits = false;
				}
				break;

			default:
				bCurLineHasEdits = true;
				if (iPos == sUnicode.size())
				{
					sUnicode += ch.value();

					if (iLast == iPos)
					{
						if (IsTerminal())
						{
							// shortcut: just output this character at the end of line
							WriteCodepoint(ch);
						}
						// and advance pos and last
						++iPos;
						++iLast;
						// and continue reading
						continue;
					}
				}
				else // if (iPos < sUnicode.size())
				{
					sUnicode.insert(iPos, 1, ch.value());
				}
				++iPos;
				break;
		}

		if (IsTerminal())
		{
			if (bRefreshWholeLine)
			{
				CurLeft(iLast);
				iLast = 0;
			}
			else
			{
				// refresh line right of pos
				if (iLast > iPos) CurLeft(iLast - iPos);
			}

			ClearToEndOfLine();

			auto iStart = std::min(iLast, iPos);

			if (iStart > sUnicode.size())
			{
				// better safe than sorry
				Write(" *** input error ***");
				return false;
			}

			auto sOut   = Unicode::ConvertUTF<KString>(sUnicode.begin() + iStart, sUnicode.end());
			Write(sOut);

			CurLeft(sUnicode.size() - iPos);
		}

		iLast = iPos;
	}

} // ReadLine

//-----------------------------------------------------------------------------
void KXTerm::RawWrite(KStringView sRaw) const
//-----------------------------------------------------------------------------
{
	kWrite(m_iOutputDevice, sRaw.data(), sRaw.size());

} // RawWrite

//-----------------------------------------------------------------------------
void KXTerm::Write(KStringView sText)
//-----------------------------------------------------------------------------
{
	if (CursorLimits())
	{
		// get the unicode codepoint count for the text
		auto iCount = sText.SizeUTF8();

		if (iCount + m_iCursorColumn > Columns())
		{
			if (Wrap())
			{
				// not yet implemented
			}
			else
			{
				// just clip the text ..
				iCount = Columns() - m_iCursorColumn;
				sText  = sText.LeftUTF8(iCount);
			}
		}

		m_iCursorColumn += iCount;
	}

	RawWrite(sText);

} // Write

//-----------------------------------------------------------------------------
void KXTerm::Write(uint16_t iRow, uint16_t iColumn, KStringView sText)
//-----------------------------------------------------------------------------
{
	SetCursor(iRow, iColumn);
	Write(sText);
}

//-----------------------------------------------------------------------------
void KXTerm::WriteLine(KStringView sText)
//-----------------------------------------------------------------------------
{
	Write(sText);
	CurToStartOfNextLine();
}

//-----------------------------------------------------------------------------
void KXTerm::WriteLine(uint16_t iRow, uint16_t iColumn, KStringView sText)
//-----------------------------------------------------------------------------
{
	Write(iRow, iColumn, sText);
	CurToStartOfNextLine();
}

//-----------------------------------------------------------------------------
void KXTerm::WriteCodepoint (KCodePoint chRaw)
//-----------------------------------------------------------------------------
{
	RawWrite(KStringView(Unicode::ToUTF<KString>(chRaw.value())));
	m_iCursorColumn += 1;

} // WriteCodepoint

//-----------------------------------------------------------------------------
void KXTerm::Beep() const
//-----------------------------------------------------------------------------
{
	if (m_bBeep)
	{
		RawWrite("\007"); // bell
	}

} // Beep

//-----------------------------------------------------------------------------
void KXTerm::Command(CGroup Group, KStringView sCommand) const
//-----------------------------------------------------------------------------
{
	if (IsTerminal())
	{
		switch (Group)
		{
			case Esc:
				RawWrite("\033");
				break;
			case Csi:
				RawWrite("\033[");
				break;
			case Dcs:
				RawWrite("\033P");
				break;
			case Osc:
				RawWrite("\033]");
				break;
			case Pri:
				RawWrite("\033?");
				break;
		}

		RawWrite(sCommand);
	}

} // Command

//-----------------------------------------------------------------------------
void KXTerm::Command(KStringView sCommand) const
//-----------------------------------------------------------------------------
{
	if (IsTerminal())
	{
		RawWrite(sCommand);
	}

} // Command

//-----------------------------------------------------------------------------
KString KXTerm::QueryTerminal(KStringView sRequest)
//-----------------------------------------------------------------------------
{
	KString sResponse;

#ifndef DEKAF2_IS_WINDOWS

	if (sRequest.empty())
	{
		return sResponse;
	}

	// check if we are inside a non-terminal
	KStringViewZ sTerm = ::getenv("TERM");

	if (sTerm.empty() || sTerm == "dumb")
	{
		kDebug(3, "env TERM is '{}' - detected as not a terminal", sTerm);
		m_iIsTerminal = 0;
		return sResponse;
	}

	RawWrite(sRequest);

	if (m_iIsTerminal == 2)
	{
		kDebug(3, "query '{}'", kEscapeForLogging(sRequest));
		// we do not know yet if this is a real terminal, so switch blocking off
		kSetTerminal(m_iInputDevice, true, 0, 1);
	}

	int ch;
	while ((ch = RawRead()) != 'R') // R terminates the response
	{
		if (EOF == ch) break;

		if (sResponse.size() > 50)
		{
			sResponse.clear();
			break;
		}

		if (KASCII::kIsPrint(ch))
		{
			sResponse += ch;
		}
	}

	if (m_iIsTerminal == 2)
	{
		// first call - check if this is a real terminal
		if (sResponse.empty())
		{
			kDebug(1, "no response - detected as not a terminal");
			m_iIsTerminal = 0;
		}
		else
		{
			kDebug(3, "response '{}' - detected as terminal", kEscapeForLogging(sResponse));
			m_iIsTerminal = 1;
		}

		// now switch blocking mode on
		kSetTerminal(m_iInputDevice, true, 1, 0);
	}

#endif

	return sResponse;

} // QueryTerminal

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckColumn       (uint16_t iColumns) const
//-----------------------------------------------------------------------------
{
	return CursorLimits() ? std::min(iColumns, m_iColumns) : iColumns;
}

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckRow          (uint16_t iRows) const
//-----------------------------------------------------------------------------
{
	return CursorLimits() ? std::min(iRows, m_iRows) : iRows;
}

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckColumnLeft   (uint16_t iColumns) const
//-----------------------------------------------------------------------------
{
	return (CursorLimits() && iColumns > m_iCursorColumn) ? 0 : iColumns;
}

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckColumnRight (uint16_t iColumns) const
//-----------------------------------------------------------------------------
{
	return (CursorLimits() && iColumns + m_iCursorColumn > m_iColumns) ? m_iColumns - m_iCursorColumn : iColumns;
}

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckRowUp      (uint16_t iRows) const
//-----------------------------------------------------------------------------
{
	return (CursorLimits() && iRows > m_iCursorRow) ? 0 : iRows;
}

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckRowDown    (uint16_t iRows) const
//-----------------------------------------------------------------------------
{
	return (CursorLimits() && iRows + m_iCursorRow > m_iRows) ? m_iRows - m_iCursorRow : iRows;
}

DEKAF2_NAMESPACE_END
