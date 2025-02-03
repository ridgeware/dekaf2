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
void kSetTerminal(bool bRaw, uint8_t iMinAvail, uint8_t iMaxWait100ms)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS

	struct termios Settings;

	::tcgetattr(STDIN_FILENO, &Settings);

	if (bRaw)
	{
		Settings.c_lflag    &= ~(ICANON | ECHO | ECHONL);
	}
	else
	{
		Settings.c_lflag    |= (ICANON | ECHO);
	}

	Settings.c_cc[VMIN]  = iMinAvail;
	Settings.c_cc[VTIME] = iMaxWait100ms;

	::tcsetattr(STDIN_FILENO, TCSANOW, &Settings);

#else

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

	DWORD mode = 0;
	GetConsoleMode(hStdin, &mode);

	if (bRaw)
	{
		SetConsoleMode(hStdin, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
	}
	else
	{
		SetConsoleMode(hStdin, mode | (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
	}

#endif
} // SetTerm

//-----------------------------------------------------------------------------
KXTerm::KXTerm()
//-----------------------------------------------------------------------------
: KXTerm(0, 0)
{
	QueryTermSize();

} // ctor

#ifndef DEKAF2_IS_WINDOWS
//-----------------------------------------------------------------------------
KXTerm::KXTerm(uint16_t iRows, uint16_t iColumns)
//-----------------------------------------------------------------------------
: m_TermIOS(InitTerminal(), TermIOSDeleter)
, m_iRows(iRows)
, m_iColumns(iColumns)
{
}

//-----------------------------------------------------------------------------
void KXTerm::TermIOSDeleter(void* Data)
//-----------------------------------------------------------------------------
{
	KXTerm::ExitTerminal(static_cast<termios*>(Data));
};

//-----------------------------------------------------------------------------
struct termios* KXTerm::InitTerminal()
//-----------------------------------------------------------------------------
{
	auto Original = new struct termios;

	if (Original)
	{
		int fd = STDIN_FILENO;

		::tcgetattr(fd, Original);

		termios Changed     = *Original;
		Changed.c_lflag    &= ~(ICANON | ECHO | ECHONL);
		Changed.c_cc[VMIN]  = 1;
		Changed.c_cc[VTIME] = 0;

		::tcsetattr(fd, TCSANOW, &Changed);
	}

	return Original;

} // InitTerminal

//-----------------------------------------------------------------------------
void KXTerm::ExitTerminal(struct termios* Original)
//-----------------------------------------------------------------------------
{
	if (Original)
	{
		int fd = STDIN_FILENO;
		::tcsetattr(fd, TCSANOW, Original);

		delete Original;
	}

} // ExitTerminal

#else // is windows

//-----------------------------------------------------------------------------
KXTerm::KXTerm(uint16_t iRows, uint16_t iColumns)
//-----------------------------------------------------------------------------
: m_iRows(iRows)
, m_iColumns(iColumns)
{
	kSetTerminal(true, 1, 0);
}

KXTerm::~KXTerm()
{
	kSetTerminal(false, 1, 0);
}

#endif // is windows

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
			return true;
		}
	}

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
void KXTerm::Cursor(bool bOn) const
//-----------------------------------------------------------------------------
{
	Command(bOn ? KXTermCodes::CursorOn() : KXTermCodes::CursorOff());
}

//-----------------------------------------------------------------------------
void KXTerm::Blink(bool bOn) const
//-----------------------------------------------------------------------------
{
	Command(bOn ? KXTermCodes::CursorBlink() : KXTermCodes::CursorNoBlink());
}

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
KCodePoint KXTerm::Read() const
//-----------------------------------------------------------------------------
{
	return Unicode::CodepointFromUTF8Reader([this]()
	{
		return RawRead();

	}, EOF);
}

//-----------------------------------------------------------------------------
bool KXTerm::ReadLine(KStringView sPrompt, KString& sLine)
//-----------------------------------------------------------------------------
{
	m_iCursorColumn = 0;

	if (!sPrompt.empty())
	{
		Hightlight();
		Write(sPrompt);
		NoHightlight();
	}

	Write(sLine);

	using UCString = std::basic_string<Unicode::codepoint_t>;

	UCString sUnicode;

	if (!Unicode::FromUTF8(sLine, sUnicode))
	{
		kDebug(2, "input is not UTF8: {}", sLine);
		return false;
	}

	auto iPos  = sUnicode.size();
	auto iLast = iPos;
	auto bCursorLimits = m_bCursorLimits;
	m_bCursorLimits = false;

	for (;;)
	{
		auto ch = Read();
		bool bRefreshWholeLine = false;

		switch (ch)
		{
			case '\033': // ESC
				ch = Read();
				switch (ch)
				{
					case '[':
						ch = Read();
						switch (ch)
						{
							case 'A': // CUR UP
								if (m_History.HaveOlder())
								{
									if (!m_History.HaveStashed())
									{
										// save current edit
										m_History.Stash(Unicode::ToUTF8<KString>(sUnicode));
									}
									sUnicode = Unicode::FromUTF8<UCString>(m_History.GetOlder());
									iPos     = sUnicode.size();
									bRefreshWholeLine = true;
								}
								else
								{
									Beep();
								}
								break;

							case 'B': // CUR DOWN
								if (m_History.HaveNewer())
								{
									sUnicode = Unicode::FromUTF8<UCString>(m_History.GetNewer());
									iPos     = sUnicode.size();
									bRefreshWholeLine = true;
								}
								else
								{
									Beep();
								}
								break;

							case 'C': // CUR RIGHT
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

							case 'D': // CUR LEFT
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

							case 'b':
								kDebug(2, "Word left");
								break;

							case 'f':
								kDebug(2, "Word right");
								break;

							case '3':
								ch = Read();
								switch (ch)
								{
									case '~':   // DEL
										if (iPos < sUnicode.size())
										{
											sUnicode.erase(iPos, 1);
										}
										else
										{
											Beep();
											continue;
										}
										break;
								}
								break;

							default:
								Beep();
								kDebug(2, "ESC [{}", char(ch));
								break;
						}
						break;
					case ']':
						kDebug(2, "ESC ]");
						break;
					case 'P':
						kDebug(2, "ESC P");
						break;
					case '?':
						kDebug(2, "ESC ?");
						break;
					default:
						kDebug(2, "ESC {}", char(ch));
						break;
				}
				break;

			case '\n':   // done ..
				sLine.clear();
				Unicode::ToUTF8(sUnicode, sLine);
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

			default:
				if (iPos == sUnicode.size())
				{
					sUnicode += ch.value();
				}
				else // if (iPos < sUnicode.size())
				{
					sUnicode.insert(iPos, 1, ch.value());
				}
				++iPos;
				break;
		}

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
		Write(Unicode::ToUTF8<KString>(sUnicode.substr(iStart, sUnicode.size() - iStart)));
		CurLeft(sUnicode.size() - iPos);
		iLast = iPos;
	}

} // ReadLine

//-----------------------------------------------------------------------------
void KXTerm::Write(KStringView sText)
//-----------------------------------------------------------------------------
{
	// get the unicode codepoint count for the text
	auto iCount = sText.SizeUTF8();

	if (CursorLimits() && (iCount + m_iCursorColumn) > Columns())
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

	RawWrite(sText);

	m_iCursorColumn += iCount;

} // Write

//-----------------------------------------------------------------------------
void KXTerm::Write(uint16_t iRow, uint16_t iColumn, KStringView sText)
//-----------------------------------------------------------------------------
{
	SetCursor(iRow, iColumn);
	Write(sText);
}

//-----------------------------------------------------------------------------
void KXTerm::RawWrite(KStringView sRaw) const
//-----------------------------------------------------------------------------
{
	kWrite(
#ifndef DEKAF2_IS_WINDOWS
	       STDOUT_FILENO
#else
	       STD_OUTPUT_HANDLE
#endif
	       , sRaw.data(), sRaw.size());

} // RawWrite

//-----------------------------------------------------------------------------
void KXTerm::WriteCodepoint (KCodePoint chRaw)
//-----------------------------------------------------------------------------
{
	RawWrite(KStringView(Unicode::ToUTF8<KString>(chRaw.value())));
	m_iCursorColumn += 1;

} // WriteCodepoint

//-----------------------------------------------------------------------------
void KXTerm::Beep()
//-----------------------------------------------------------------------------
{
	RawWrite("\007"); // bell
}

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
		m_bIsTerminal = 0;
		return sResponse;
	}

	RawWrite(sRequest);

	if (m_bIsTerminal == 2)
	{
		// we do not know yet if this is a real terminal, so switch blocking off
		kSetTerminal(true, 0, 1);
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

	if (m_bIsTerminal == 2)
	{
		// first call - check if this is a real terminal
		if (sResponse.empty())
		{
			m_bIsTerminal = 0;
		}
		else
		{
			m_bIsTerminal = 1;
		}

		// now switch blocking mode on
		kSetTerminal(true, 1, 0);
	}

#endif

	return sResponse;

} // ReadTerminalResponse

//-----------------------------------------------------------------------------
uint16_t KXTerm::CheckColumnLeft   (uint16_t iColumns) const
//-----------------------------------------------------------------------------
{
	return (CursorLimits() &&iColumns > m_iCursorColumn) ? 0 : iColumns;
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
