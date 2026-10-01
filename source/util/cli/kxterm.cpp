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

#include <dekaf2/io/readwrite/kwrite.h>
#include <dekaf2/io/readwrite/kreader.h>   // KIn, kReadLine for kPromptForPassword
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/core/strings/kutf.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <string>
#include <cstdio>                           // getchar()

#ifndef DEKAF2_IS_WINDOWS
	#include <termios.h>
	#include <unistd.h>                     // ::isatty for kPromptForPassword
#else
	#include <windows.h>
	// for Windows SDKs older than Windows 10
	#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
		#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#endif
	#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
		#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
	#endif
#endif

#include <dekaf2/util/cli/kxterm.h> // keep this at the end of includes, it removes a freak RGB definition in windows headers

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_IS_WINDOWS
// the console of the KXTerm that switched it into the virtual terminal modes, for the
// restore on Ctrl-C
static HANDLE s_hModesInput  { nullptr };
static HANDLE s_hModesOutput { nullptr };
static DWORD  s_dwInputMode  { 0 };
static DWORD  s_dwOutputMode { 0 };

//-----------------------------------------------------------------------------
DEKAF2_PRIVATE
BOOL WINAPI RestoreConsoleModesOnCtrl(DWORD /* dwCtrlType */)
//-----------------------------------------------------------------------------
{
	// Ctrl-C, Ctrl-Break and closing the console end the process with ExitProcess()
	// when no other handler takes the event, and the destructor of KXTerm does not run
	// then - so restore the console modes here, and pass the event on to the next handler
	if (s_hModesInput)
	{
		::SetConsoleMode(s_hModesInput,  s_dwInputMode );
		::SetConsoleMode(s_hModesOutput, s_dwOutputMode);
	}

	return FALSE;

} // RestoreConsoleModesOnCtrl
#endif

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
		SetConsoleMode(hStdin, mode | (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
	}

#endif

} // SetTerm

//-----------------------------------------------------------------------------
KString kPromptForPassword(KStringView sPrompt, int iInputDevice)
//-----------------------------------------------------------------------------
{
	KOut.Write(sPrompt).Flush();

#if defined(DEKAF2_IS_WINDOWS)

	// On Windows `iInputDevice` is expected to already be a STD_*_HANDLE
	// token — the kcompatibility.h remap ensures STDIN_FILENO resolves
	// to STD_INPUT_HANDLE at compile time, so the same call-site source
	// works on both platforms. This matches how kSetTerminal() treats
	// the parameter.
	const HANDLE hStdin    = GetStdHandle(static_cast<DWORD>(iInputDevice));
	DWORD        dwOldMode = 0;
	const bool   bIsTty    = (hStdin != INVALID_HANDLE_VALUE)
	                      && GetConsoleMode(hStdin, &dwOldMode);

	if (bIsTty)
	{
		SetConsoleMode(hStdin, dwOldMode & ~ENABLE_ECHO_INPUT);
	}

	KAtScopeEnd( if (bIsTty) SetConsoleMode(hStdin, dwOldMode); );

#else

	const bool    bIsTty = ::isatty(iInputDevice);
	struct termios OldTermios {};

	if (bIsTty && ::tcgetattr(iInputDevice, &OldTermios) == 0)
	{
		struct termios NewTermios = OldTermios;
		// Only the ECHO bit is cleared — we deliberately keep ICANON set
		// so backspace / line editing on the user's side still work.
		// ECHONL is a no-op unless ICANON is also on (kept implicitly).
		NewTermios.c_lflag &= ~static_cast<tcflag_t>(ECHO);
		::tcsetattr(iInputDevice, TCSANOW, &NewTermios);
	}

	KAtScopeEnd( if (bIsTty) ::tcsetattr(iInputDevice, TCSANOW, &OldTermios); );

#endif

	KString sLine;
	kReadLine(KIn, sLine);

	// User pressed Enter but the character was not echoed — emit our own
	// newline so follow-up output does not stack onto the prompt line.
	KOut.WriteLine().Flush();

	return sLine;

} // kPromptForPassword

//-----------------------------------------------------------------------------
KXTermCodes::RGB::RGB(KStringView sColorValue)
//-----------------------------------------------------------------------------
{
	sColorValue.remove_prefix('#');

	if (sColorValue.size() == 6 || sColorValue.Trim().size() == 6)
	{
		Red   = static_cast<uint8_t>(sColorValue.Left (   2).UInt16(/*bIsHex*/true));
		Green = static_cast<uint8_t>(sColorValue.Mid  (2, 2).UInt16(/*bIsHex*/true));
		Blue  = static_cast<uint8_t>(sColorValue.Right(   2).UInt16(/*bIsHex*/true));
	}
	else
	{
		kDebug(3, "invalid RGB color string: {}", sColorValue.LeftUTF8(20));
		Red   = 0;
		Green = 0;
		Blue  = 0;
	}

} // ctor

//-----------------------------------------------------------------------------
bool KXTermCodes::HasRGBColors()
//-----------------------------------------------------------------------------
{
	KStringViewZ sTerm = kGetEnv("COLORTERM");
	kDebug(3, "env COLORTERM is '{}'", sTerm);

	if (sTerm.In("truecolor,24bit"))
	{
		kDebug(3, "terminal has RGB colors");
		return true;
	}

	sTerm = kGetEnv("TERM");
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
	return kFormat("\033[38;2;{};{};{}m", rgb.Red, rgb.Green, rgb.Blue);
}

//-----------------------------------------------------------------------------
KString KXTermCodes::BGColor(RGB rgb)
//-----------------------------------------------------------------------------
{
	return kFormat("\033[48;2;{};{};{}m", rgb.Red, rgb.Green, rgb.Blue);
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

	// A console of Windows 10 or later processes the escape sequences of an xterm once the
	// virtual terminal modes are switched on for its output and its input. If one of the
	// devices is no console, or the console refuses a mode, both devices stay as they are:
	// the console then echoes and edits the typed lines itself.
	HANDLE hInput       = ::GetStdHandle(static_cast<DWORD>(m_iInputDevice));
	HANDLE hOutput      = ::GetStdHandle(static_cast<DWORD>(m_iOutputDevice));
	DWORD  dwInputMode  = 0;
	DWORD  dwOutputMode = 0;

	if (::GetConsoleMode(hInput,  &dwInputMode) &&
	    ::GetConsoleMode(hOutput, &dwOutputMode))
	{
		if (::SetConsoleMode(hOutput, dwOutputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
		{
			// raw input as on the other platforms: no echo and no line editing by the console,
			// but ENABLE_PROCESSED_INPUT stays on, so that Ctrl-C still raises the signal
			if (::SetConsoleMode(hInput, (dwInputMode | ENABLE_VIRTUAL_TERMINAL_INPUT) & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)))
			{
				m_iSavedInputMode    = dwInputMode;
				m_iSavedOutputMode   = dwOutputMode;
				m_bConsoleModesSaved = true;
				m_eIsTerminal        = TerminalState::Yes;

				s_hModesInput        = hInput;
				s_hModesOutput       = hOutput;
				s_dwInputMode        = dwInputMode;
				s_dwOutputMode       = dwOutputMode;
				::SetConsoleCtrlHandler(RestoreConsoleModesOnCtrl, TRUE);
			}
			else
			{
				::SetConsoleMode(hOutput, dwOutputMode);
			}
		}
	}

	if (m_eIsTerminal != TerminalState::Yes)
	{
		kDebug(1, "input device {} and output device {} are no console with virtual terminal modes - no terminal control codes will be written",
		       m_iInputDevice, m_iOutputDevice);
		m_eIsTerminal = TerminalState::No;
	}

#else

	// the probes and control codes are written to the output device, so it has to be a
	// terminal as well. If it is not, the input device is left untouched: the terminal
	// driver then keeps echoing and editing the typed lines, which a raw input device
	// without a terminal on the output side would not do.
	if (!kIsTerminal(m_iOutputDevice))
	{
		kDebug(1, "output device {} is not a terminal - no terminal control codes will be written", m_iOutputDevice);
		m_eIsTerminal = TerminalState::No;
	}
	else
	{
		m_Termios = std::make_unique<termios>();
	}

	if (m_Termios)
	{
		bool bIsRealTerm = false;

		auto iResult = ::tcgetattr(m_iInputDevice, m_Termios.get());

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
			m_Termios.reset();
		}

		if (!bIsRealTerm)
		{
			kDebug(1, "this is not a terminal: {}", strerror(errno));
			// this is not a real terminal
			m_eIsTerminal = TerminalState::No;
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

	if (m_bConsoleModesSaved)
	{
		::SetConsoleCtrlHandler(RestoreConsoleModesOnCtrl, FALSE);
		s_hModesInput = nullptr;

		::SetConsoleMode(::GetStdHandle(static_cast<DWORD>(m_iInputDevice )), m_iSavedInputMode );
		::SetConsoleMode(::GetStdHandle(static_cast<DWORD>(m_iOutputDevice)), m_iSavedOutputMode);
	}

#else

	if (m_Termios)
	{
		::tcsetattr(m_iInputDevice, TCSANOW, m_Termios.get());
	}

#endif

} // dtor

//-----------------------------------------------------------------------------
void KXTerm::QueryTermSize()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	// the console answers the cursor query only into the input stream, which cannot be read
	// with a timeout here - but it tells its size directly
	auto TTY   = kGetTerminalSize(m_iOutputDevice, 80, 25);
	m_iRows    = TTY.lines;
	m_iColumns = TTY.columns;

#else

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

#endif

	kDebug(2, "Rows: {}, Columns: {}", m_iRows, m_iColumns);

} // QueryTermSize

//-----------------------------------------------------------------------------
bool KXTerm::GetCursor(uint16_t& iRow, uint16_t& iColumn)
//-----------------------------------------------------------------------------
{
	if (m_eIsTerminal == TerminalState::No)
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
		if (DEKAF2_UNLIKELY(m_iChangedWindowTitle == std::numeric_limits<uint16_t>::max())) --m_iChangedWindowTitle;
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

namespace {
inline constexpr uint8_t Control(uint8_t c) { return c - ('a' - 1); }
}

//-----------------------------------------------------------------------------
KCodePoint KXTerm::Read(kutf::ReadIterator& it, const kutf::ReadIterator& ie)
//-----------------------------------------------------------------------------
{
	return kutf::CodepointFromUTF8(it, ie);
}

//-----------------------------------------------------------------------------
KCodePoint KXTerm::ReadEscaped(kutf::ReadIterator& it, const kutf::ReadIterator& ie)
//-----------------------------------------------------------------------------
{
	auto ch = Read(it, ie);

	if (ch != '\033') return ch;

	ch = Read(it, ie);

	switch (ch)
	{
		case '[':
			ch = Read(it, ie);

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
					ch = Read(it, ie);

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
			return 0x222B;
//			return U'∫';

		case 'f':
			return 0x0192;
//			return U'ƒ';

		default:
			kDebug(2, "ESC {}", char(ch));
			break;
	}

	return 0;

} // EscapeToControl

//-----------------------------------------------------------------------------
bool KXTerm::EditLine(
	KStringView sPrompt,
	KStringRef& sLine,
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

	using UCString = std::basic_string<kutf::codepoint_t>;

	UCString sUnicode;
	UCString sClipboard;

	if (!kutf::Convert(sLine, sUnicode))
	{
		return SetError(kFormat("input is not UTF8: {}", sLine));
	}

	// the number of terminal columns of a range of the edited line (CJK characters take two)
	auto TextColumns = [&sUnicode](std::size_t iFrom, std::size_t iTo) -> std::size_t
	{
		std::size_t iColumns { 0 };

		for (; iFrom < iTo; ++iFrom)
		{
			iColumns += KCodePoint(sUnicode[iFrom]).GetColumnWidth();
		}

		return iColumns;
	};

	auto iPos  = sUnicode.size();
	auto iLast = iPos;
	// the column of the cursor on the screen, counted from the start of the edited text -
	// m_iCursorColumn is not tracked while the cursor limits are switched off
	auto iCursorColumn = TextColumns(0, iPos);
	std::size_t iStoredPos = 0;
	bool bCurLineHasEdits = true;
	auto bCursorLimits = m_bCursorLimits;
	m_bCursorLimits = false;
	KAtScopeEnd( m_bCursorLimits = bCursorLimits );

	kutf::ReadIterator it(&KXTerm::RawRead);
	kutf::ReadIterator ie;

	for (;;)
	{
		// do escape processing first, replace common sequences with control codes
		auto ch = ReadEscaped(it, ie);

		bool bRefreshWholeLine = false;

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

			case kutf::END_OF_INPUT:
				return false;

			case kutf::INVALID_CODEPOINT:
				kDebug(2, "invalid codepoint");
				Beep();
				break;

			case '\r':   // the Enter key of a raw Windows console
			case '\n':   // done ..
				sLine.clear();
				kutf::Convert(sUnicode, sLine);
				m_History.Add(sLine);
				m_bCursorLimits = bCursorLimits;
				return true;

			case '\b':   // Ctrl-H
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

					// a combining mark moves together with the character it belongs to
					while (iPos && KCodePoint(sUnicode[iPos]).GetColumnWidth() == 0)
					{
						--iPos;
					}

					auto iColumns = TextColumns(iPos, iLast);
					CurLeft(iColumns);
					iCursorColumn -= iColumns;
					iLast = iPos;
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

					// a combining mark moves together with the character it belongs to
					while (iPos < sUnicode.size() && KCodePoint(sUnicode[iPos]).GetColumnWidth() == 0)
					{
						++iPos;
					}

					auto iColumns = TextColumns(iLast, iPos);
					CurRight(iColumns);
					iCursorColumn += iColumns;
					iLast = iPos;
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
					sUnicode          = kutf::Convert<UCString>(m_History.GetNewer());
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
					m_History.Stash(kutf::Convert<KString>(sUnicode));
				}
				if (m_History.HaveOlder())
				{
					if (!m_History.HaveStashed())
					{
						// save current edit
						m_History.Stash(kutf::Convert<KString>(sUnicode));
					}
					sUnicode          = kutf::Convert<UCString>(m_History.GetOlder());
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
				Home();
				if (!sPrompt.empty())
				{
					Command(sPromptFormatStart);
					Write(sPrompt);
					Command(sPromptFormatEnd);
				}
				Write(kutf::Convert<KString>(sUnicode.begin(), sUnicode.end()));
				CurLeft(TextColumns(iPos, sUnicode.size()));
				iLast = iPos;
				iCursorColumn = TextColumns(0, iPos);
				continue;

			case Control('t'): // transpose chars (readline semantics)
				if (iPos == sUnicode.size() && iPos > 1)
				{
					// at end of line: swap last two chars
					std::swap(sUnicode[iPos-2], sUnicode[iPos-1]);
					bRefreshWholeLine = true;
					break;
				}
				else if (iPos > 0 && iPos < sUnicode.size())
				{
					// in middle: swap char before cursor with char at cursor, advance
					std::swap(sUnicode[iPos-1], sUnicode[iPos]);
					++iPos;
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
			{
				auto iOldPos = iPos;
				while (iPos > 0 &&  kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				while (iPos > 0 && !kIsSpace(sUnicode[iPos - 1])) { --iPos; }
				sClipboard = sUnicode.substr(iPos, iOldPos - iPos);
				sUnicode.erase(iPos, iOldPos - iPos);
				break;
			}

			case Control('v'): // paste clipboard (non-standard, ctrl-y pauses on mac)
				sUnicode.insert(iPos, sClipboard);
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

//			case U'ƒ':  // move forward one word
			case 0x0192:
				if (!IsTerminal() || iPos >= sUnicode.size())
				{
					Beep();
					continue;
				}
				while (iPos < sUnicode.size() && !kIsSpace(sUnicode[iPos])) { ++iPos; }
				while (iPos < sUnicode.size() &&  kIsSpace(sUnicode[iPos])) { ++iPos; }
				break;

//			case U'∫':  // move backward one word
			case 0x222B:
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
					KString sSearch = kutf::Convert<KString>(sUnicode.begin(), sUnicode.begin() + iPos);
					sUnicode = kutf::Convert<UCString>(m_History.Find(sSearch, true));
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
						iCursorColumn += ch.GetColumnWidth();
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
				// back to the start of the line - the new line may have other widths
				CurLeft(iCursorColumn);
				iCursorColumn = 0;
				iLast = 0;
			}

			auto iStart = std::min(iLast, iPos);

			if (iStart > sUnicode.size())
			{
				// better safe than sorry
				Write(" *** input error ***");
				return SetError(" *** input error ***");
			}

			// the text before iStart is unchanged on the screen - refresh the line right of it
			auto iStartColumn = TextColumns(0, iStart);

			if (iCursorColumn > iStartColumn)
			{
				CurLeft(iCursorColumn - iStartColumn);
			}

			ClearToEndOfLine();

			auto sOut   = kutf::Convert<KString>(sUnicode.begin() + iStart, sUnicode.end());
			Write(sOut);

			CurLeft(TextColumns(iPos, sUnicode.size()));
			iCursorColumn = TextColumns(0, iPos);
		}

		iLast = iPos;
	}

} // ReadLine

//-----------------------------------------------------------------------------
void KXTerm::RawWrite(KStringView sRaw) const
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	if (sRaw.empty())
	{
		return;
	}

	// on Windows the devices are GetStdHandle() identifiers, which kWrite() does not take.
	// A console is written in UTF-16, whatever its output code page is, other devices
	// (pipes and files) get the bytes as they are.
	HANDLE hOutput   = ::GetStdHandle(static_cast<DWORD>(m_iOutputDevice));
	DWORD  dwMode    = 0;
	DWORD  dwWritten = 0;

	if (::GetConsoleMode(hOutput, &dwMode))
	{
		auto sWide = kutf::Convert<std::wstring>(sRaw);
		::WriteConsoleW(hOutput, sWide.data(), static_cast<DWORD>(sWide.size()), &dwWritten, nullptr);
	}
	else
	{
		::WriteFile(hOutput, sRaw.data(), static_cast<DWORD>(sRaw.size()), &dwWritten, nullptr);
	}

#else

	kWrite(m_iOutputDevice, sRaw.data(), sRaw.size());

#endif

} // RawWrite

//-----------------------------------------------------------------------------
int KXTerm::RawRead()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	// A console is read in UTF-16 and handed out as UTF-8, whatever its input code page is
	// (the C runtime would translate the bytes of that code page in its text mode). Other
	// input devices (pipes and files) are read through the C runtime.
	static thread_local KString     s_sPending;
	static thread_local std::size_t s_iPending { 0 };

	if (s_iPending < s_sPending.size())
	{
		return static_cast<unsigned char>(s_sPending[s_iPending++]);
	}

	HANDLE hInput = ::GetStdHandle(STD_INPUT_HANDLE);
	DWORD  dwMode = 0;

	if (::GetConsoleMode(hInput, &dwMode))
	{
		wchar_t Chars[2];
		DWORD   dwRead = 0;

		if (!::ReadConsoleW(hInput, &Chars[0], 1, &dwRead, nullptr) || dwRead != 1)
		{
			return EOF;
		}

		DWORD dwCount = 1;

		if (IS_HIGH_SURROGATE(Chars[0]))
		{
			// the second half of the surrogate pair follows
			if (::ReadConsoleW(hInput, &Chars[1], 1, &dwRead, nullptr) && dwRead == 1)
			{
				dwCount = 2;
			}
		}

		if (dwCount == 1 && Chars[0] == 0x1A)
		{
			// Ctrl-Z is the end of input on Windows
			return EOF;
		}

		s_sPending = kutf::Convert<KString>(std::wstring(&Chars[0], dwCount));
		s_iPending = 0;

		if (s_sPending.empty())
		{
			return EOF;
		}

		return static_cast<unsigned char>(s_sPending[s_iPending++]);
	}

#endif

	return getchar();

} // RawRead

//-----------------------------------------------------------------------------
void KXTerm::Write(KStringView sText)
//-----------------------------------------------------------------------------
{
	if (CursorLimits())
	{
		// get the terminal columns of the text (CJK characters take two)
		auto iCount = sText.ColumnWidth();

		if (iCount + m_iCursorColumn > Columns())
		{
			if (m_iCursorColumn >= Columns())
			{
				// cursor is already at or past the right edge
				iCount = 0;
				sText  = KStringView{};
			}
			else if (Wrap())
			{
				// not yet implemented
			}
			else
			{
				// just clip the text ..
				sText  = sText.LeftColumns(Columns() - m_iCursorColumn);
				iCount = sText.ColumnWidth();
			}
		}

		m_iCursorColumn += static_cast<uint16_t>(iCount);
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
	RawWrite(KStringView(kutf::ToUTF<KString>(chRaw.value())));
	m_iCursorColumn += chRaw.GetColumnWidth();

} // WriteCodepoint

//-----------------------------------------------------------------------------
void KXTerm::Beep() const
//-----------------------------------------------------------------------------
{
	if (m_bBeep && IsTerminal())
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
	KStringViewZ sTerm = kGetEnv("TERM");

	if (sTerm.empty() || sTerm == "dumb")
	{
		kDebug(3, "env TERM is '{}' - detected as not a terminal", sTerm);
		m_eIsTerminal = TerminalState::No;
		return sResponse;
	}

	RawWrite(sRequest);

	if (m_eIsTerminal == TerminalState::Unknown)
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

	if (m_eIsTerminal == TerminalState::Unknown)
	{
		// first call - check if this is a real terminal
		if (sResponse.empty())
		{
			kDebug(1, "no response - detected as not a terminal");
			m_eIsTerminal = TerminalState::No;
		}
		else
		{
			kDebug(3, "response '{}' - detected as terminal", kEscapeForLogging(sResponse));
			m_eIsTerminal = TerminalState::Yes;
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
