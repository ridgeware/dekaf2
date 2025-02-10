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
/// xterm (ANSII) controls, and a screen manager as class object

#include "kdefinitions.h"
#include "kcompatibility.h"
#include "kstringview.h"
#include "kstring.h"
#include "khistory.h"

#ifndef DEKAF2_IS_WINDOWS
struct termios;
#endif

DEKAF2_NAMESPACE_BEGIN

/// change terminal configuration on stdin - you do not need to use this when using KXTerm, all parms
/// will be set automatically by the KXTerm constructor
/// @param iInputDevice file descriptor for input device, normally STDIN_FILENO (0)
/// @param bRaw if true, echo and linemode will be switched off
/// @param iMinAvail how many characters shall be available before read returns: 0..255.
/// This parameter has no effect on Windows.
/// @param iMaxWait100ms after how many multiples of 100ms should read return even if not enough characters are read: 0..255.
/// This parameter has no effect on Windows
extern void kSetTerminal(int iInputDevice, bool bRaw, uint8_t iMinAvail, uint8_t iMaxWait100ms);

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A collection of ANSII codes to control terminals.
/// We do not use ncurses or similar anymore because there is no terminal left in the real world that is not
/// powered by XTerm, or even if, they speak ANSII, which was standardized in 1978.
/// It should be mentioned that even Windows terminals, which notoriously did not support ANSII, switched
/// to using it in 2016 with Windows 10.
struct KXTermCodes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	/// show cursor
	static constexpr KStringView CursorOn               () { return "\033?25h"; }
	/// hide cursor
	static constexpr KStringView CursorOff              () { return "\033?25l"; }
	/// blink cursor
	static constexpr KStringView CursorBlink            () { return "\033[2q";  }
	/// don't blink cursor
	static constexpr KStringView CursorNoBlink          () { return "\033[0q";  }
	/// save cursor position
	static constexpr KStringView SaveCursor             () { return "\0337";    }
	/// restore cursor position from last saved
	static constexpr KStringView RestoreCursor          () { return "\0338";    }
	/// clear entire screen
	static constexpr KStringView ClearScreen            () { return "\033[2J";  }
	/// clear from cursor to end of screen
	static constexpr KStringView ClearToEndOfScreen     () { return "\033[0J";  }
	/// clear from cursor to end of line
	static constexpr KStringView ClearToEndOfLine       () { return "\033[0K";  }
	/// clear from start of line to cursor
	static constexpr KStringView ClearFromStartOfLine   () { return "\033[1K";  }
	/// clear entire line
	static constexpr KStringView ClearLine              () { return "\033[2K";  }
	/// clear from start of screen to cursor
	static constexpr KStringView ClearFromStartOfScreen () { return "\033[1J";  }
	/// go to home position (0, 0)
	static constexpr KStringView Home                   () { return "\033[H";   }
	/// move cursor to start of next line
	static constexpr KStringView CurToStartOfNextLine   () { return "\033[1E";  }
	/// move cursor to start of previous line
	static constexpr KStringView CurToStartOfPrevLine   () { return "\033[1F";  }
	/// reset all character style modes (and colors)
	static constexpr KStringView ResetCharModes         () { return "\033[0m";  }

	static constexpr KStringView Bold                   () { return "\033[1m";  }
	static constexpr KStringView NoBold                 () { return "\033[22m"; }
	static constexpr KStringView Faint                  () { return "\033[2m";  }
	static constexpr KStringView NoFaint                () { return "\033[22m"; } // this is no typo - code is shared with NoBold ..
	static constexpr KStringView Italic                 () { return "\033[3m";  }
	static constexpr KStringView NoItalic               () { return "\033[23m"; }
	static constexpr KStringView Underline              () { return "\033[4m";  }
	static constexpr KStringView NoUnderline            () { return "\033[24m"; }
	static constexpr KStringView Blinking               () { return "\033[5m";  }
	static constexpr KStringView NoBlinking             () { return "\033[25m"; }
	static constexpr KStringView Inverse                () { return "\033[7m";  }
	static constexpr KStringView NoInverse              () { return "\033[27m"; }
	static constexpr KStringView Hidden                 () { return "\033[8m";  }
	static constexpr KStringView NoHidden               () { return "\033[28m"; }
	static constexpr KStringView Strikethrough          () { return "\033[9m";  }
	static constexpr KStringView NoStrikethrough        () { return "\033[29m"; }

	enum ColorCode { Default = 9, Black = 0, Red = 1, Green = 2, Yellow = 3,
	                 Blue = 4, Magenta = 5, Cyan = 6, White = 7,
	                 BrightBlack = 10, BrightRed = 11, BrightGreen = 12,
	                 BrightYellow = 13, BrightBlue = 14, BrightMagenta = 15,
	                 BrightCyan = 16, BrightWhite = 17 };

	/// set both named foreground and background color (supported by all terminal devices)
	static           KString     Color      (ColorCode FGC, ColorCode BGC);
	/// set named foreground color (supported by all terminal devices)
	static           KString     FGColor    (ColorCode CC);
	/// set named background color (supported by all terminal devices)
	static           KString     BGColor    (ColorCode CC);

	/// set one of 256 foreground and background colors each (supported by all terminal devices today)
	static           KString     Color256   (uint8_t FG_Color, uint8_t BG_Color);
	/// set one of 256 foreground colors (supported by all terminal devices today)
	static           KString     FGColor256 (uint8_t Color);
	/// set one of 256 background colors (supported by all terminal devices today)
	static           KString     BGColor256 (uint8_t Color);

	/// struct to take an RGB color value with 8 bit resolution each
	struct RGB
	{
		/// construct from 3 discrete color values for red, green and blue
		RGB(uint8_t red, uint8_t green, uint8_t blue)
		: Red(red), Green(green), Blue(blue) {}
		/// construct from a hex string like "AAEE99" or "#AAEE99" in
		/// upper or lower case
		RGB(KStringView sColorValue);

		uint8_t Red;
		uint8_t Green;
		uint8_t Blue;
	};

	/// set both foreground and background color in RGB values from 0..255 each (supported by
	/// terminal devices which pass theHasRGBColors() test)
	static           KString     Color      (RGB fg_rgb, RGB bg_rgb);
	/// set foreground color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	static           KString     FGColor    (RGB rgb);
	/// set background color in RGB values from 0..255 each (supported by terminal devices
	/// which pass the HasRGBColors() test)
	static           KString     BGColor    (RGB rgb);
	/// returns true if the terminal device (at stdout and declared by env vars) supports
	/// "true colors" / RGB colors (24 bits), false if unknown or unsupported
	static           bool        HasRGBColors ();

//-------
private:
//-------

	static KString GetFGColor (ColorCode CC);
	static KString GetBGColor (ColorCode CC);

}; // KXTermCodes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Talk with a terminal (emulator), with cursor control, colorisation, line editing with emacs commands,
/// and history
class KXTerm
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using ColorCode = KXTermCodes::ColorCode;
	using RGB       = KXTermCodes::RGB;

	KXTerm(
		int      iInputDevice  = STDIN_FILENO,
		int      iOutputDevice = STDOUT_FILENO,
		uint16_t iRows         = 0,
		uint16_t iColumns      = 0
	);
	~KXTerm();

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
	bool     IsTerminal        () const { return m_iIsTerminal;                            }
	/// returns true if the terminal device supports "true colors" / RGB colors (24 bits)
	bool     HasRGBColors      () const { return m_bHasRGBColors;                          }

	/// query a changed terminal size, use Rows() and Columns() to get the updated values
	void     QueryTermSize     ();
	/// query count of rows for the terminal
	uint16_t Rows              () const { return m_iRows;                                  }
	/// query count of columns for the terminal
	uint16_t Columns           () const { return m_iColumns;                               }

	// functions to control cursor movement and character output on an xterm

	/// query current cursor position from the terminal, returns false if executed on a dumb terminal without cursor control
	bool     GetCursor         (uint16_t& Row, uint16_t& Column);
	/// get current cursor row
	uint16_t GetCursorRow      () const { return m_iCursorRow;                             }
	/// get current cursor column
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

	void Bold                  () const { Command(KXTermCodes::Bold                   ()); }
	void NoBold                () const { Command(KXTermCodes::NoBold                 ()); }
	void Faint                 () const { Command(KXTermCodes::Faint                  ()); }
	void NoFaint               () const { Command(KXTermCodes::NoFaint                ()); }
	void Italic                () const { Command(KXTermCodes::Italic                 ()); }
	void NoItalic              () const { Command(KXTermCodes::NoItalic               ()); }
	void Underline             () const { Command(KXTermCodes::Underline              ()); }
	void NoUnderline           () const { Command(KXTermCodes::NoUnderline            ()); }
	void Blinking              () const { Command(KXTermCodes::Blinking               ()); }
	void NoBlinking            () const { Command(KXTermCodes::NoBlinking             ()); }
	void Inverse               () const { Command(KXTermCodes::Inverse                ()); }
	void NoInverse             () const { Command(KXTermCodes::NoInverse              ()); }
	void Hidden                () const { Command(KXTermCodes::Hidden                 ()); }
	void NoHidden              () const { Command(KXTermCodes::NoHidden               ()); }
	void Strikethrough         () const { Command(KXTermCodes::Strikethrough          ()); }
	void NoStrikethrough       () const { Command(KXTermCodes::NoStrikethrough        ()); }

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

	enum CGroup { Esc, Csi, Dcs, Osc, Pri };

	/// blocking read, returns unicode codepoints
	KCodePoint Read           ()                  const;
	/// blocking read, returns single 8 bit chars
	int      RawRead          ()                  const { return getchar();                      }
	void     RawWrite         (KStringView sRaw)  const;
	void     WriteCodepoint   (KCodePoint chRaw);
	void     Command          (CGroup Group, KStringView sCommand) const;
	void     Command          (KStringView sCommand) const;
	KString  QueryTerminal    (KStringView sRequest);
	void     IntSetCursor     (uint16_t iRow, uint16_t iColumn, bool bCheck);
	KCodePoint EscapeToControl(KCodePoint)        const;

	uint16_t CheckColumn      (uint16_t iColumns) const;
	uint16_t CheckRow         (uint16_t iRows)    const;
	uint16_t CheckColumnLeft  (uint16_t iColumns) const;
	uint16_t CheckColumnRight (uint16_t iColumns) const;
	uint16_t CheckRowUp       (uint16_t iRows)    const;
	uint16_t CheckRowDown     (uint16_t iRows)    const;

	bool     Wrap             ()                  const { return m_bWrap;                        }
	bool     CursorLimits     ()                  const { return m_bCursorLimits;                }

	KHistory m_History;
	KString  m_sLastWindowTitle   { "\004\001" }; // sequence that is probably unused..

	int      m_iInputDevice;
	int      m_iOutputDevice;

	uint16_t m_iRows;
	uint16_t m_iColumns;

#ifndef DEKAF2_IS_WINDOWS
	termios* m_Termios            { nullptr };
#endif

	uint16_t m_iCursorRow         { 0 };
	uint16_t m_iCursorColumn      { 0 };

	uint16_t m_iSavedCursorRow    { 0 };
	uint16_t m_iSavedCursorColumn { 0 };

	uint16_t m_iChangedWindowTitle{ 0 };

	uint8_t  m_iIsTerminal        { 2 }; // 2 means: not yet tested
	bool     m_bBeep              { true  };
	bool     m_bCursorLimits      { true  };
	bool     m_bHasRGBColors      { false };
	bool     m_bWrap              { false };

}; // KXTerm

DEKAF2_NAMESPACE_END
