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
#include "kstringview.h"
#include "kstring.h"
#include "khistory.h"

#ifndef DEKAF2_IS_WINDOWS
#include "bits/kunique_deleter.h"
struct termios;
#endif

DEKAF2_NAMESPACE_BEGIN

/// change terminal configuration on stdin
/// @param bRaw if true, echo and linemode will be switched off
/// @param iMinAvail how many characters shall be available before read returns: 0..255.
/// This parameter has no effect on Windows.
/// @param iMaxWait100ms after how many multiples of 100ms should read return even if not enough characters are read: 0..255.
/// This parameter has no effect on Windows
extern void kSetTerminal(bool bRaw, uint8_t iMinAvail, uint8_t iMaxWait100ms);

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A collection of ANSII codes to control terminals.
/// We do not use ncurses or similar anymore because there is no terminal left in the real world that is not
/// powered by XTerm, or even if, they speak ANSII, which was standardized in 1978.
/// It should be mentioned that even Windows terminals, which notoriously did not support ANSII, switched
/// to using it in 2016 with Windows 10.
struct KXTermCodes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
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
	/// clear from cursor to end of line
	static constexpr KStringView ClearToEndOfLine       () { return "\033[0K";  }
	/// clear from cursor to end of screen
	static constexpr KStringView ClearToEndOfScreen     () { return "\033[0J";  }
	/// clear entire line
	static constexpr KStringView ClearLine              () { return "\033[2K";  }
	/// clear from start of line to cursor
	static constexpr KStringView ClearFromStartOfLine   () { return "\033[1K";  }
	/// clear from start of screen to cursor
	static constexpr KStringView ClearFromStartOfScreen () { return "\033[1J";  }
	/// go to home position (0, 0)
	static constexpr KStringView Home                   () { return "\033[H";   }
	/// highlight following text
	static constexpr KStringView Highlight              () { return "\033[1m";  }
	/// switch highlight off
	static constexpr KStringView NoHighlight            () { return "\033[22m"; }

}; // KXTermCodes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KXTerm
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	KXTerm();
	KXTerm(uint16_t iRows, uint16_t iColumns);
#ifdef DEKAF2_IS_WINDOWS
	~KXTerm();
#endif

	/// is this an ANSII terminal, or a dumb terminal without cursor control?
	bool     IsTerminal        () const { return m_bIsTerminal;   }

	/// query a changed terminal size, use Rows() and Columns() to get the updated values
	void     QueryTermSize     ();
	/// query count of rows for the terminal
	uint16_t Rows              () const { return m_iRows;         }
	/// query count of columns for the terminal
	uint16_t Columns           () const { return m_iColumns;      }

	// functions to control cursor movement and character output on an xterm

	/// query current cursor position from the terminal, returns false if executed on a dumb terminal without cursor control
	bool     GetCursor         (uint16_t& Row, uint16_t& Column);
	/// get current cursor row
	uint16_t GetCursorRow      () const { return m_iCursorRow;    }
	/// get current cursor column
	uint16_t GetCursorColumn   () const { return m_iCursorColumn; }

	/// make cursor visible?
	void Cursor        (bool bOn) const;
	/// make cursor blink?
	void Blink         (bool bOn) const;

	/// clear screen
	void ClearScreen           () const { Command(KXTermCodes::ClearScreen());             }
	/// clear from cursor to end of line
	void ClearToEndOfLine      () const { Command(KXTermCodes::ClearToEndOfLine());        }
	/// clear from cursor to end of screen
	void ClearToEndOfScreen    () const { Command(KXTermCodes::ClearToEndOfScreen());      }
	/// clear entire line
	void ClearLine             () const { Command(KXTermCodes::ClearLine());               }
	/// clear from start of line to cursor
	void ClearFromStartOfLine  () const { Command(KXTermCodes::ClearFromStartOfLine());    }
	/// clear from start of screen to cursor
	void ClearFromStartOfScreen() const { Command(KXTermCodes::ClearFromStartOfScreen());  }

	/// highlight following text
	void Hightlight            () const { Command(KXTermCodes::Highlight());               }
	/// switch highlight off
	void NoHightlight          () const { Command(KXTermCodes::NoHighlight());             }

	/// go to home position (0, 0)
	void Home                  ();
	/// set to position
	void SetCursor  (uint16_t iRow, uint16_t iColumn) { IntSetCursor(iRow, iColumn, true); }
	/// set to column
	void SetColumn  (uint16_t iColumn);
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

	/// go back 1 column, deleting the previous character
	void Backspace();
	/// delete the next character, stay at position
	void Delete();

	/// write text at cursor position
	void Write(KStringView sText);
	/// write text at given position
	void Write(uint16_t iRow, uint16_t iColumn, KStringView sText);

	void Wrap(bool bYesNo)   { m_bWrap = bYesNo; }

	/// read a line of input, displays possible content of sLine after prompt, puts cursor at end
	bool ReadLine(KStringView sPrompt, KString& sLine);

	/// set a history file - if pathname is empty a program name derived default filename will be used in ~/.config/dekaf2/
	bool SetHistoryFile(KString sPathname = KString{}) { return m_History.SetFile(sPathname); }
	/// set the size for the history, both in memory and on disk
	/// @param iHistorySize the size for the history, minimum 100, default 10000
	void SetHistorySize(uint32_t iHistorySize)         { m_History.SetSize(iHistorySize);     }

//-------
private:
//-------

	enum CGroup { Esc, Csi, Dcs, Osc, Pri };

#ifndef DEKAF2_IS_WINDOWS
	/// init a terminal for non-canonical/non-linemode reading, returns ownership on a termios struct
	static struct termios* InitTerminal();
	/// reset a terminal to original mode from a termios struct, claims ownership
	static void ExitTerminal  (struct termios* Original);
	/// the deleter for our unique_ptr<termios>
	static void TermIOSDeleter(void* Data);
	KUniqueVoidPtr m_TermIOS;
#endif

	/// blocking read
	KCodePoint Read           ()                  const;
	/// non-blocking read (returns after 100ms, to keep spin loop low in CPU)
	int      RawRead          ()                  const { return getchar();                      }
	void     RawWrite         (KStringView sRaw)  const;
	void     WriteCodepoint   (KCodePoint chRaw);
	void     Command          (CGroup Group, KStringView sCommand) const;
	void     Command          (KStringView sCommand) const;
	KString  QueryTerminal    (KStringView sRequest);
	void     IntSetCursor     (uint16_t iRow, uint16_t iColumn, bool bCheck);
	void     Beep             ();

	uint16_t CheckColumn      (uint16_t iColumns) const { return std::min(iColumns, m_iColumns); }
	uint16_t CheckRow         (uint16_t iRows)    const { return std::min(iRows, m_iRows);       }
	uint16_t CheckColumnLeft  (uint16_t iColumns) const;
	uint16_t CheckColumnRight (uint16_t iColumns) const;
	uint16_t CheckRowUp       (uint16_t iRows)    const;
	uint16_t CheckRowDown     (uint16_t iRows)    const;

	bool     Wrap             ()                  const { return m_bWrap;                        }
	bool     CursorLimits     ()                  const { return m_bCursorLimits;                }

	KHistory m_History;

	uint16_t m_iRows;
	uint16_t m_iColumns;

	uint16_t m_iCursorRow         { 0 };
	uint16_t m_iCursorColumn      { 0 };

	uint16_t m_iSavedCursorRow    { 0 };
	uint16_t m_iSavedCursorColumn { 0 };

	uint8_t  m_bIsTerminal        { 2 }; // 2 means: not yet tested
	bool     m_bCursorLimits      { true  };
	bool     m_bWrap              { false };

}; // KXTerm

DEKAF2_NAMESPACE_END
