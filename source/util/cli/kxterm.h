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

#pragma once

/// @file kxterm.h
/// provides ANSI terminal control codes (KXTermCodes) and a stateful terminal manager (KXTerm)
/// with cursor control, color management (named, 256, and RGB), line editing with emacs key bindings,
/// and command history support. Does not depend on ncurses or termcap.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/containers/sequential/khistory.h>
#include <dekaf2/core/errors/kerror.h>
#include <memory>

#ifndef DEKAF2_IS_WINDOWS
	struct termios;
#else
	#ifdef RGB
		#undef RGB
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_cli
/// @{

/// change terminal configuration on stdin - you do not need to use this when using KXTerm, all parms
/// will be set automatically by the KXTerm constructor
/// @param iInputDevice file descriptor for input device, normally STDIN_FILENO (0)
/// @param bRaw if true, echo and linemode will be switched off
/// @param iMinAvail how many characters shall be available before read returns: 0..255.
/// This parameter has no effect on Windows.
/// @param iMaxWait100ms after how many multiples of 100ms should read return even if not enough characters are read: 0..255.
/// This parameter has no effect on Windows
DEKAF2_PUBLIC
extern void kSetTerminal(int iInputDevice, bool bRaw, uint8_t iMinAvail, uint8_t iMaxWait100ms);

/// Prompt on the controlling terminal for a single line of input with echo
/// temporarily disabled — the classic "Password: " flow used by CLI tools
/// (e.g. database login, service install, admin user bootstrap).
///
/// The original terminal settings are saved before echo is turned off and
/// restored via a scope guard, so an exception or signal during the read
/// cannot leave the terminal silent. A newline is emitted on stdout after
/// the (hidden) input so follow-up output starts on a fresh line.
///
/// When the input device is not a TTY (e.g. stdin is a pipe or redirected
/// file), echo cannot be toggled; the line is still read and returned
/// verbatim. Callers that require no-echo guarantees should verify
/// interactivity separately (e.g. via `::isatty()` on POSIX) before
/// calling this function.
///
/// @param sPrompt string written to stdout before reading (e.g.
///                "Password: "). No trailing space or newline is added —
///                pass exactly what should appear on screen.
/// @param iInputDevice file descriptor for the input device, normally
///                     STDIN_FILENO (which dekaf2 remaps to
///                     STD_INPUT_HANDLE on Windows via
///                     `core/init/kcompatibility.h`, so the same
///                     call-site source works on both platforms).
/// @return the line that was read, without the trailing newline. An
/// empty string is returned on EOF, read error, or when the user simply
/// pressed Enter — these cases are not distinguishable via the return
/// value; callers that need to tell them apart should check stream state
/// on KIn directly.
DEKAF2_NODISCARD DEKAF2_PUBLIC
extern KString kPromptForPassword(KStringView sPrompt, int iInputDevice = STDIN_FILENO);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A collection of static ANSI escape sequences for terminal control.
///
/// Provides constexpr accessors for cursor visibility, screen clearing, character styling (bold, italic,
/// underline, etc.), and color output in three tiers:
/// - **Named colors** (8 standard + 8 bright, via ColorCode enum)
/// - **256-color palette** (via FGColor256() / BGColor256())
/// - **24-bit RGB** (via FGColor(RGB) / BGColor(RGB), requires terminal support)
///
/// All methods return ANSI escape strings that can be written directly to a terminal device.
/// No terminal state is held — this struct is purely a code generator.
///
/// @note We do not use ncurses or termcap. There is no terminal left in the real world that does not
/// speak ANSI, which was standardized in 1978 (ECMA-48). Even Windows terminals support ANSI since 2016.
struct DEKAF2_PUBLIC KXTermCodes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	/// show cursor
	DEKAF2_NODISCARD
	static constexpr KStringView CursorOn               () { return "\033[?25h"; }
	/// hide cursor
	DEKAF2_NODISCARD
	static constexpr KStringView CursorOff              () { return "\033[?25l"; }
	/// blink cursor
	DEKAF2_NODISCARD
	static constexpr KStringView CursorBlink            () { return "\033[2q";  }
	/// don't blink cursor
	DEKAF2_NODISCARD
	static constexpr KStringView CursorNoBlink          () { return "\033[0q";  }
	/// save cursor position
	DEKAF2_NODISCARD
	static constexpr KStringView SaveCursor             () { return "\0337";    }
	/// restore cursor position from last saved
	DEKAF2_NODISCARD
	static constexpr KStringView RestoreCursor          () { return "\0338";    }
	/// clear entire screen
	DEKAF2_NODISCARD
	static constexpr KStringView ClearScreen            () { return "\033[2J";  }
	/// clear from cursor to end of screen
	DEKAF2_NODISCARD
	static constexpr KStringView ClearToEndOfScreen     () { return "\033[0J";  }
	/// clear from cursor to end of line
	DEKAF2_NODISCARD
	static constexpr KStringView ClearToEndOfLine       () { return "\033[0K";  }
	/// clear from start of line to cursor
	DEKAF2_NODISCARD
	static constexpr KStringView ClearFromStartOfLine   () { return "\033[1K";  }
	/// clear entire line
	DEKAF2_NODISCARD
	static constexpr KStringView ClearLine              () { return "\033[2K";  }
	/// clear from start of screen to cursor
	DEKAF2_NODISCARD
	static constexpr KStringView ClearFromStartOfScreen () { return "\033[1J";  }
	/// go to home position (0, 0)
	DEKAF2_NODISCARD
	static constexpr KStringView Home                   () { return "\033[H";   }
	/// move cursor to start of next line
	DEKAF2_NODISCARD
	static constexpr KStringView CurToStartOfNextLine   () { return "\033[1E";  }
	/// move cursor to start of previous line
	DEKAF2_NODISCARD
	static constexpr KStringView CurToStartOfPrevLine   () { return "\033[1F";  }
	/// reset all character style modes (and colors)
	DEKAF2_NODISCARD
	static constexpr KStringView ResetCharModes         () { return "\033[0m";  }

	DEKAF2_NODISCARD
	static constexpr KStringView Bold                   () { return "\033[1m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoBold                 () { return "\033[22m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Faint                  () { return "\033[2m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoFaint                () { return "\033[22m"; } // this is no typo - code is shared with NoBold ..
	DEKAF2_NODISCARD
	static constexpr KStringView Italic                 () { return "\033[3m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoItalic               () { return "\033[23m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Underline              () { return "\033[4m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoUnderline            () { return "\033[24m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Blinking               () { return "\033[5m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoBlinking             () { return "\033[25m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Inverse                () { return "\033[7m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoInverse              () { return "\033[27m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Hidden                 () { return "\033[8m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoHidden               () { return "\033[28m"; }
	DEKAF2_NODISCARD
	static constexpr KStringView Strikethrough          () { return "\033[9m";  }
	DEKAF2_NODISCARD
	static constexpr KStringView NoStrikethrough        () { return "\033[29m"; }

	/// Named terminal color codes for use with FGColor(), BGColor(), and Color().
	/// Values 0..7 are the standard ANSI colors, 9 is the terminal default,
	/// and 10..17 are the bright/bold variants (rendered with SGR bold prefix).
	///
	/// @note These color codes are supported by all terminal devices.
	enum ColorCode
	{
		Black         =  0, ///< standard black
		Red           =  1, ///< standard red
		Green         =  2, ///< standard green
		Yellow        =  3, ///< standard yellow
		Blue          =  4, ///< standard blue
		Magenta       =  5, ///< standard magenta
		Cyan          =  6, ///< standard cyan
		White         =  7, ///< standard white
		Default       =  9, ///< terminal default color
		BrightBlack   = 10, ///< bright black (dark gray)
		BrightRed     = 11, ///< bright red
		BrightGreen   = 12, ///< bright green
		BrightYellow  = 13, ///< bright yellow
		BrightBlue    = 14, ///< bright blue
		BrightMagenta = 15, ///< bright magenta
		BrightCyan    = 16, ///< bright cyan
		BrightWhite   = 17  ///< bright white
	};

	/// set both named foreground and background color (supported by all terminal devices)
	DEKAF2_NODISCARD
	static           KString     Color      (ColorCode FGC, ColorCode BGC);
	/// set named foreground color (supported by all terminal devices)
	DEKAF2_NODISCARD
	static           KString     FGColor    (ColorCode CC);
	/// set named background color (supported by all terminal devices)
	DEKAF2_NODISCARD
	static           KString     BGColor    (ColorCode CC);

	/// set one of 256 foreground and background colors each (supported by all terminal devices today)
	DEKAF2_NODISCARD
	static           KString     Color256   (uint8_t FG_Color, uint8_t BG_Color);
	/// set one of 256 foreground colors (supported by all terminal devices today)
	DEKAF2_NODISCARD
	static           KString     FGColor256 (uint8_t Color);
	/// set one of 256 background colors (supported by all terminal devices today)
	DEKAF2_NODISCARD
	static           KString     BGColor256 (uint8_t Color);

	/// A 24-bit RGB color value with 8-bit resolution per channel.
	/// Used with FGColor(RGB), BGColor(RGB), and Color(RGB, RGB) to produce
	/// ANSI true-color escape sequences (\033[38;2;R;G;Bm / \033[48;2;R;G;Bm).
	/// @note Not all terminals support 24-bit color — check HasRGBColors() first.
	struct RGB
	{
		/// construct from 3 discrete color channel values
		/// @param red   red channel intensity (0..255)
		/// @param green green channel intensity (0..255)
		/// @param blue  blue channel intensity (0..255)
		RGB(uint8_t red, uint8_t green, uint8_t blue)
		: Red(red), Green(green), Blue(blue) {}
		/// construct from a hex color string like "AABB00" or "#AABB00" (case insensitive).
		/// If the string is malformed or too short, all channels are set to 0.
		/// @param sColorValue a 6-character hex string, optionally prefixed with '#'
		RGB(KStringView sColorValue);

		/// red channel intensity (0..255)
		uint8_t Red;
		/// green channel intensity (0..255)
		uint8_t Green;
		/// blue channel intensity (0..255)
		uint8_t Blue;
	};

	/// set both foreground and background color in RGB values from 0..255 each (supported by
	/// terminal devices which pass theHasRGBColors() test)
	DEKAF2_NODISCARD
	static           KString     Color      (RGB fg_rgb, RGB bg_rgb);
	/// set foreground color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	DEKAF2_NODISCARD
	static           KString     FGColor    (RGB rgb);
	/// set background color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	DEKAF2_NODISCARD
	static           KString     BGColor    (RGB rgb);
	/// returns true if the terminal device (at stdout and declared by env vars) supports
	/// "true colors" / RGB colors (24 bits), false if unknown or unsupported
	DEKAF2_NODISCARD
	static           bool        HasRGBColors ();

//-------
private:
//-------

	static KString GetFGColor (ColorCode CC);
	static KString GetBGColor (ColorCode CC);

}; // KXTermCodes

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Stateful terminal manager for interactive command-line applications.
///
/// Wraps a pair of input/output file descriptors (typically stdin/stdout) and provides:
/// - **Cursor control**: absolute positioning, relative movement, save/restore
/// - **Screen management**: clear screen, clear line, clear regions
/// - **Text styling**: bold, italic, underline, inverse, strikethrough, etc.
/// - **Color output**: named colors, 256-color palette, 24-bit RGB
/// - **Line editing**: emacs-style key bindings (Ctrl+A/E/K/U/W/T/V/L/R/X/D, arrow keys, Home/End, Delete)
/// - **Command history**: in-memory and optional disk-persistent history with search (Tab / Ctrl+R)
/// - **Window title**: push/pop stack for XTerm-compatible window titles
///
/// The constructor switches the terminal into raw character mode (no echo, no line buffering).
/// The destructor restores the original terminal settings. The class is non-copyable and non-movable.
///
/// On construction, KXTerm auto-detects whether the attached device is a real terminal or a pipe/file
/// by sending a cursor position query. Use IsTerminal() to check the result.
///
/// @note Errors are reported through the KErrorBase interface (SetError() / GetLastError()).
///
/// ### Example
/// @code
/// KXTerm term;
/// term.SetHistory(500, true);
/// KString sLine;
/// while (term.EditLine(">> ", sLine))
/// {
///     // process sLine
///     sLine.clear();
/// }
/// @endcode
class KXTerm : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using ColorCode = KXTermCodes::ColorCode;
	using RGB       = KXTermCodes::RGB;

	/// Construct a terminal manager and switch the input device to raw mode.
	/// If iRows or iColumns are 0, the terminal size is queried automatically via ioctl.
	/// The original terminal settings are saved and will be restored by the destructor.
	/// @param iInputDevice  file descriptor for terminal input (default: STDIN_FILENO)
	/// @param iOutputDevice file descriptor for terminal output (default: STDOUT_FILENO)
	/// @param iRows         number of rows, or 0 to auto-detect
	/// @param iColumns      number of columns, or 0 to auto-detect
	KXTerm(
		int      iInputDevice  = STDIN_FILENO,
		int      iOutputDevice = STDOUT_FILENO,
		uint16_t iRows         = 0,
		uint16_t iColumns      = 0
	);
	/// Restore original terminal settings and pop any pushed window titles.
	~KXTerm();

	KXTerm(const KXTerm&) = delete;
	KXTerm& operator=(const KXTerm&) = delete;
	KXTerm(KXTerm&&) = delete;
	KXTerm& operator=(KXTerm&&) = delete;

	/// write text at cursor position
	void Write      (KStringView sText);
	/// write text at given position
	void Write      (uint16_t iRow, uint16_t iColumn, KStringView sText);
	/// write text at cursor position and goto next line
	void WriteLine  (KStringView sText = KStringView{});
	/// write text at given position and goto next line
	void WriteLine  (uint16_t iRow, uint16_t iColumn, KStringView sText);

	/// edit a line of input, displays possible content of sLine after prompt, puts cursor at end
	/// @param sPrompt will be printed at start of line if not empty
	/// @param sLine the line to be edited, cursor will be placed at end of text
	/// @param sPromptFormatStart the formatting to apply for the prompt
	/// @param sPromptFormatEnd the formatting to apply at the end of the prompt
	/// @return true if editing was ended with a newline/return key, false in case of an input error
	bool EditLine(
		KStringView sPrompt,
		KString&    sLine,
		KStringView sPromptFormatStart = KXTermCodes::Bold(),
		KStringView sPromptFormatEnd   = KXTermCodes::NoBold()
	);

	/// set the size for the history, both in memory and on disk
	/// @param iHistorySize the size for the history, minimum 100. 0 switches history off (which is the default)
	/// @param bToDisk if true stores the history in a file
	/// @param sPathname the file to store the history into. If empty a default filename will be used in
	/// ~/.config/{{program name}}/terminal-history.txt
	void SetHistory (uint32_t iHistorySize, bool bToDisk, KString sPathname = KString{})
	                     { m_History.SetSize(iHistorySize, bToDisk, std::move(sPathname)); }

	/// beep on invalid control codes or edges (on per default)
	void SetBeep    (bool bYesNo) { m_bBeep = bYesNo; }
	/// not yet implemented
	void SetWrap    (bool bYesNo) { m_bWrap = bYesNo; }
	/// make cursor visible?
	void ShowCursor    (bool bOn) const;
	/// make cursor blink?
	void SetBlink      (bool bOn) const;
	/// if the terminal is an XTerm or compatible, this sets the window title (it's in fact a stack, and you can restore
	/// previous window titles by calling ResetWindowTitle())
	/// @param sWindowTitle the new window title
	/// @return true if the window title has been changed, false if the title was the same as previously
	bool SetWindowTitle(KStringView sWindowTitle);
	/// restore the previous window title (will also be done by destructor)
	void RestoreWindowTitle();

	/// is this an ANSII terminal, or a dumb terminal without cursor control?
	DEKAF2_NODISCARD
	bool     IsTerminal        () const { return m_eIsTerminal == TerminalState::Yes;        }
	/// returns true if the terminal device supports "true colors" / RGB colors (24 bits)
	DEKAF2_NODISCARD
	bool     HasRGBColors      () const { return m_bHasRGBColors;                          }

	/// query a changed terminal size, use Rows() and Columns() to get the updated values
	void     QueryTermSize     ();
	/// query count of rows for the terminal
	DEKAF2_NODISCARD
	uint16_t Rows              () const { return m_iRows;                                  }
	/// query count of columns for the terminal
	DEKAF2_NODISCARD
	uint16_t Columns           () const { return m_iColumns;                               }

	// functions to control cursor movement and character output on an xterm

	/// query current cursor position from the terminal, returns false if executed on a dumb terminal without cursor control
	bool     GetCursor         (uint16_t& Row, uint16_t& Column);
	/// get current cursor row
	DEKAF2_NODISCARD
	uint16_t GetCursorRow      () const { return m_iCursorRow;                             }
	/// get current cursor column
	DEKAF2_NODISCARD
	uint16_t GetCursorColumn   () const { return m_iCursorColumn;                          }

	/// force the terminal to beep
	void Beep                  ()  const;

	/// go to home position (0, 0)
	void Home                  ();
	/// set to position
	void SetCursor  (uint16_t iRow, uint16_t iColumn) { IntSetCursor(iRow, iColumn, true); }
	/// set to column
	void CurToColumn(uint16_t iColumn);
	/// go left n columns
	void CurLeft    (uint16_t iColumns = 1);
	/// go right n columns
	void CurRight   (uint16_t iColumns = 1);
	/// go up n rows
	void CurUp      (uint16_t iRows   = 1);
	/// go down n rows
	void CurDown    (uint16_t iRows   = 1);
	/// save current cursor position (in terminal)
	void SaveCursor ();
	/// restore cursor position (in terminal) from last saved position
	void RestoreCursor ();

	/// clear screen
	void ClearScreen           () const { Command(KXTermCodes::ClearScreen            ()); }
	/// clear from cursor to end of line
	void ClearToEndOfLine      () const { Command(KXTermCodes::ClearToEndOfLine       ()); }
	/// clear from cursor to end of screen
	void ClearToEndOfScreen    () const { Command(KXTermCodes::ClearToEndOfScreen     ()); }
	/// clear entire line
	void ClearLine             ();
	/// clear from start of line to cursor
	void ClearFromStartOfLine  () const { Command(KXTermCodes::ClearFromStartOfLine   ()); }
	/// clear from start of screen to cursor
	void ClearFromStartOfScreen() const { Command(KXTermCodes::ClearFromStartOfScreen ()); }
	/// move cursor to start of next line
	void CurToStartOfNextLine  ();
	/// move cursor to start of previous line
	void CurToStartOfPrevLine  ();

	/// @name Character style methods
	/// Enable or disable SGR text attributes on the terminal output. Each method sends
	/// the corresponding ANSI escape sequence immediately. Use ResetCharModes() to clear all.
	/// @{
	void Bold                  () const { Command(KXTermCodes::Bold                   ()); } ///< enable bold text
	void NoBold                () const { Command(KXTermCodes::NoBold                 ()); } ///< disable bold text
	void Faint                 () const { Command(KXTermCodes::Faint                  ()); } ///< enable faint/dim text
	void NoFaint               () const { Command(KXTermCodes::NoFaint                ()); } ///< disable faint text (same code as NoBold)
	void Italic                () const { Command(KXTermCodes::Italic                 ()); } ///< enable italic text
	void NoItalic              () const { Command(KXTermCodes::NoItalic               ()); } ///< disable italic text
	void Underline             () const { Command(KXTermCodes::Underline              ()); } ///< enable underlined text
	void NoUnderline           () const { Command(KXTermCodes::NoUnderline            ()); } ///< disable underlined text
	void Blinking              () const { Command(KXTermCodes::Blinking               ()); } ///< enable blinking text
	void NoBlinking            () const { Command(KXTermCodes::NoBlinking             ()); } ///< disable blinking text
	void Inverse               () const { Command(KXTermCodes::Inverse                ()); } ///< enable inverse/reverse video
	void NoInverse             () const { Command(KXTermCodes::NoInverse              ()); } ///< disable inverse video
	void Hidden                () const { Command(KXTermCodes::Hidden                 ()); } ///< enable hidden/invisible text
	void NoHidden              () const { Command(KXTermCodes::NoHidden               ()); } ///< disable hidden text
	void Strikethrough         () const { Command(KXTermCodes::Strikethrough          ()); } ///< enable strikethrough text
	void NoStrikethrough       () const { Command(KXTermCodes::NoStrikethrough        ()); } ///< disable strikethrough text
	void ResetCharModes        () const { Command(KXTermCodes::ResetCharModes         ()); } ///< reset all character style modes (and colors)
	/// @}

	/// set both named foreground and background color (supported by all terminal devices)
	void Color      (ColorCode FGC, ColorCode BGC) const { Command(KXTermCodes::Color(FGC, BGC)); }
	/// set named foreground color (supported by all terminal devices)
	void FGColor    (ColorCode CC) const              { Command(KXTermCodes::FGColor(CC)); }
	/// set named background color (supported by all terminal devices)
	void BGColor    (ColorCode CC) const              { Command(KXTermCodes::BGColor(CC)); }

	/// set one of 256 foreground and background colors each (supported by all terminal devices today)
	void Color256   (uint8_t FG_Color, uint8_t BG_Color);
	/// set one of 256 foreground colors (supported by all terminal devices today)
	void FGColor256 (uint8_t Color);
	/// set one of 256 background colors (supported by all terminal devices today)
	void BGColor256 (uint8_t Color);

	/// set both foreground and background color in RGB values from 0..255 each (supported by
	/// terminal devices which pass theHasRGBColors() test)
	void Color      (RGB fg_rgb, RGB bg_rgb) { Command(KXTermCodes::Color(fg_rgb, bg_rgb));}
	/// set foreground color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	void FGColor    (RGB rgb)                        { Command(KXTermCodes::FGColor(rgb)); }
	/// set background color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	void BGColor    (RGB rgb)                        { Command(KXTermCodes::BGColor(rgb)); }

//-------
private:
//-------

	/// ANSI escape sequence group prefixes used by Command(CGroup, ...)
	enum CGroup
	{
		Esc, ///< ESC (\033) — simple escape
		Csi, ///< CSI (\033[) — Control Sequence Introducer
		Dcs, ///< DCS (\033P) — Device Control String
		Osc, ///< OSC (\033]) — Operating System Command
		Pri  ///< Private (\033[?) — private mode sequence
	};

	/// blocking read of a single raw byte from stdin
	DEKAF2_NODISCARD
	static int RawRead        ()                          { return getchar();                    }
	/// write raw bytes to the output device, bypassing cursor tracking
	void     RawWrite         (KStringView sRaw)  const;
	/// write a single Unicode codepoint to the output device as UTF-8
	void     WriteCodepoint   (KCodePoint chRaw);
	/// send an ANSI escape sequence with a group prefix (e.g. CSI, OSC)
	void     Command          (CGroup Group, KStringView sCommand) const;
	/// send a pre-formatted escape sequence string directly
	void     Command          (KStringView sCommand) const;
	/// send a query to the terminal and read the response (used for cursor position and terminal detection)
	/// @param sRequest the escape sequence to send (e.g. cursor position report)
	/// @return the terminal's response string, or empty if no response (not a terminal)
	DEKAF2_NODISCARD
	KString  QueryTerminal    (KStringView sRequest);
	/// internal cursor positioning with optional bounds checking
	void     IntSetCursor     (uint16_t iRow, uint16_t iColumn, bool bCheck);

	/// blocking read, returns unicode codepoints
	DEKAF2_NODISCARD
	static KCodePoint Read    (kutf::ReadIterator& it, const kutf::ReadIterator& ie);
	/// blocking read, returns unicode codepoints, unescapes ANSII sequences
	DEKAF2_NODISCARD
	static KCodePoint ReadEscaped(kutf::ReadIterator& it, const kutf::ReadIterator& ie);

	/// @name Cursor boundary checks
	/// These methods clamp or limit movement distances to stay within the terminal dimensions
	/// when CursorLimits() is enabled. They return the (possibly reduced) distance.
	/// @{
	DEKAF2_NODISCARD
	uint16_t CheckColumn      (uint16_t iColumns) const; ///< clamp absolute column to terminal width
	DEKAF2_NODISCARD
	uint16_t CheckRow         (uint16_t iRows)    const; ///< clamp absolute row to terminal height
	DEKAF2_NODISCARD
	uint16_t CheckColumnLeft  (uint16_t iColumns) const; ///< limit leftward movement to current column
	DEKAF2_NODISCARD
	uint16_t CheckColumnRight (uint16_t iColumns) const; ///< limit rightward movement to remaining columns
	DEKAF2_NODISCARD
	uint16_t CheckRowUp       (uint16_t iRows)    const; ///< limit upward movement to current row
	DEKAF2_NODISCARD
	uint16_t CheckRowDown     (uint16_t iRows)    const; ///< limit downward movement to remaining rows
	/// @}

	/// returns true if line wrapping is enabled (not yet implemented)
	DEKAF2_NODISCARD
	bool     Wrap             ()                  const { return m_bWrap;                        }
	/// returns true if cursor movement is clamped to terminal dimensions
	DEKAF2_NODISCARD
	bool     CursorLimits     ()                  const { return m_bCursorLimits;                }

	KHistory m_History;                                   ///< command line history (in-memory, optionally on disk)
	KString  m_sLastWindowTitle   { "\004\001" };         ///< last set window title (initialized to unlikely sentinel)

	int      m_iInputDevice;                              ///< file descriptor for terminal input
	int      m_iOutputDevice;                             ///< file descriptor for terminal output

	uint16_t m_iRows;                                     ///< terminal height in rows
	uint16_t m_iColumns;                                  ///< terminal width in columns

#ifndef DEKAF2_IS_WINDOWS
	std::unique_ptr<termios> m_Termios;                   ///< saved original terminal settings, restored in destructor
#endif

	uint16_t m_iCursorRow         { 0 };                  ///< tracked cursor row (0-based)
	uint16_t m_iCursorColumn      { 0 };                  ///< tracked cursor column (0-based)

	uint16_t m_iSavedCursorRow    { 0 };                  ///< saved cursor row for SaveCursor/RestoreCursor
	uint16_t m_iSavedCursorColumn { 0 };                  ///< saved cursor column for SaveCursor/RestoreCursor

	uint16_t m_iChangedWindowTitle{ 0 };                  ///< count of pushed window titles (for pop on restore)

	/// Terminal detection state, determined on first QueryTerminal() call
	enum class TerminalState : uint8_t
	{
		No      = 0, ///< device is not a terminal (pipe, file, or no response)
		Yes     = 1, ///< device is a real terminal (responded to cursor query)
		Unknown = 2  ///< not yet tested (initial state)
	};
	TerminalState m_eIsTerminal   { TerminalState::Unknown }; ///< current terminal detection state
	bool     m_bBeep              { true  };              ///< beep on invalid input or edge (Ctrl+G)
	bool     m_bCursorLimits      { true  };              ///< clamp cursor movement to terminal dimensions
	bool     m_bHasRGBColors      { false };              ///< terminal supports 24-bit RGB colors
	bool     m_bWrap              { false };              ///< line wrapping mode (not yet implemented)

}; // KXTerm


/// @}

DEKAF2_NAMESPACE_END
