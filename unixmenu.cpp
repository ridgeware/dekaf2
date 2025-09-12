/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
//
*/

#include "kdefinitions.h"
#include "klog.h"
#include "kstring.h"
#include "kstringview.h"
#include "kstringutils.h"
#include "kformat.h"
#include "kwriter.h"
#include "ksystem.h"
#include "kfilesystem.h"
#include "kcompatibility.h"

#include <stdio.h>
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

using namespace DEKAF2_NAMESPACE_NAME;

static constexpr KStringViewZ RUN_N_WAIT {"runNwait"};
static constexpr KStringViewZ EXIT       {"EXIT"};
static constexpr KStringViewZ DEBUG      {"DEBUG"};

#define UsingSystem	/* vs. execl() */

char	*index(), *fgets(), *getenv();

static const char* g_Synopsis[] = {
" ",
"unixmenu - an arbitrary menuing system designed for UNIX.",
" ",
"usage: unixmenu <fullpath-to-menu-file> ",
" ",
"Menu file format:",
"  '#! XXX' -menu title specification (should appear once in menufile)",
"  '#@ XXX' -menu option specification for NEXT LINE as COMMAND",
"  '#@'     -menu option spacer (puts blanks lines in menu)",
"  '# XXX'  -full line comment",
"  BLANK-LINES: you can put arbitrary white space in a menu file",
" ",
"Special COMMAND directives:",
"  runNwait XXXX -runs your command (XXX) and prompts for user to hit return",
"  EXIT          -stops interpreting, restores screen, and exits",
"  DEBUG         -displays internal debugging information",
" ",
"Environment variable overrides:",
"  MENU_SHELL    -override /bin/sh",
"  MENU_OPTION   -set to an integer <i> (1...N) to start with item <i>",
"  MENU_WAITSTR  -overrides the string 'Hit return to continue: '",
"  MENU_SLEEP    -pause after selecting a menu item (defaults to 1 sec)",
"  MENU_HEIGHT   -screen height (ignores stty and defaults to 24 chars)",
"  MENU_WIDTH    -screen width (ignores stty and defaults to 80 chars)",
" ",
"Sample menu file:",
"  #! **Menu Title**",
" ",
"  #@",
"  #@ See Processes",
"  runNwait ps -aux | grep $USER | more",
" ",
"  #@ Change Password",
"  runNwait passwd $USER",
" ",
"  #@",
"  #@ Exit",
"  EXIT",
" ",
//"Active menu keys:",
//"  Cursor-UP:    VT-UP-ARROW    k  K  ^P",
//"  Cursor-DOWN:  VT-DOWN-ARROW  j  J  ^N  TAB",
//"  ACCEPT:       RETURN  LINEFEED  ^C ^D",
//" ",
"History",
"  22-Jan-91 (keef)  -created",
"  17-Nov-92 (keef)  -improved",
"  19-Juk-22 (keef)  -refactored for DEKAF2",
" ",
""
};

/*
MACROS:
*/
inline void raw_io()	{ raw(); crmode(); /*noecho();*/ }
inline void cooked_io()	{ noraw(); nocrmode(); echo(); }

#undef addstr
#define addstr(s)       { waddnstr(stdscr,s.c_str(),static_cast<int>(s.size())); }


/*
CONSTANTS:
*/
enum {MAXOPTIONS =     19};	/* max number of menu items */
#define DEFAULT_TITLE	"**  M E N U **"
#define	DEFAULT_MSG	    "SELECTION:"

/*
GLOBALS:
*/
struct menuT
{
	int8_t iNRows;
	int8_t iSleep;
	int8_t iMargin;
	int8_t iTopRow;
	int8_t iMaxRows;
	KString sMenuTitle;
	KString labels[MAXOPTIONS];
	KString commands[MAXOPTIONS];
	KString sSelectMSG;
};

KString g_sBuffer;
KString g_sWaitString{ "Hit return to continue: "};
KString g_sShell{   "/bin/sh"};
int8_t	g_iSleep{1};   // secs
int8_t g_iHeight{24}; //chars
int8_t g_iWidth{80};  //chars

int8_t  interpreter_loop (KStringViewZ sMenuFile, int8_t iCPick);
void     run_command (KStringView sCommand);
void     parse_menufile (KStringViewZ sContents, menuT* menu);
int8_t  select_item (menuT* menu, int8_t iCPick);
void     warning (KStringView string, KStringView sarg);
int8_t  error_and_exit (KStringView string, KStringView sarg);
void     synopsis_abort ();
int8_t  menu_loop (menuT* menu, int8_t iCPick);
void     keef_box (int x1, int y1, int x2, int y2);

inline void Pause() { char foo[50+1]; if (!fgets(foo, 50, stdin)) {/* suppress warning about unused result*/}; }

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	int8_t iCPick{0};
	KString sMenuFile;

	/*
	** CMDLINE:
	*/
	for (int ii=1; ii < argc; ++ii)
	{
		KString sArg{argv[ii]};
		if (kStrIn (sArg, "-d,-dd,-ddd,-dddd"))
		{
			KLog::getInstance().SetDebugLog (KLog::STDOUT);
			KLog::getInstance().SetLevel (static_cast<int>(sArg.size()) - 1);
			kDebug (1, "debug set to: {}", KLog::getInstance().GetLevel());
		}
		else if (kStrIn (sArg, "-h,--h,-help,--help"))
		{
			synopsis_abort();
		}
		else if (!sMenuFile)
		{
			sMenuFile = sArg;
		}
		else
		{
			KErr.FormatLine (">> unknown cli option: {}", sArg);
		}
	}

	if (!sMenuFile)
	{
		synopsis_abort ();
	}

	/*
	** ENVIRONMENT:
	*/
	if (getenv("MENU_OPTION"))		iCPick        = atoi(getenv("MENU_OPTION"));
	if (getenv("MENU_WAITSTR"))		g_sWaitString = getenv("MENU_WAITSTR");
	if (getenv("MENU_SLEEP"))		g_iSleep      = atoi(getenv("MENU_SLEEP"));
	if (getenv("MENU_HEIGHT"))		g_iHeight     = atoi(getenv("MENU_HEIGHT"));
	if (getenv("MENU_WIDTH"))		g_iWidth      = atoi(getenv("MENU_WIDTH"));
	if (getenv("MENU_SHELL"))		g_sShell      = getenv("MENU_SHELL");

	/*
	** CHECKING:
	*/
	if ((iCPick < 0) || (iCPick > MAXOPTIONS)) iCPick = 0;
	if (g_iSleep < 0)   g_iSleep  =  1;
	if (g_iSleep <= 6)  g_iHeight = 24;
	if (g_iSleep <= 10) g_iWidth  = 80;

	/*
	** MAIN LOOP:
	*/
	do
	{
		iCPick = interpreter_loop (sMenuFile, iCPick);
	}
	while (iCPick >= 0);

} /* main- unixmenu */

//-----------------------------------------------------------------------------
int8_t interpreter_loop (KStringViewZ sMenuFile, int8_t iCPick)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	menuT   selections;

	parse_menufile (sMenuFile, &selections); // or exit

	/* MENU/PICK.. */
	iCPick = select_item (&selections, iCPick);

	/* ACTION.. */
	if (EXIT == selections.commands[iCPick])
	{
	    return -1;
	}
	else if (DEBUG == selections.commands[iCPick])
	{
		KOut.WriteLine  ("DEBUG VALUES  (version 2.0:17-Nov-92):");
		KOut.FormatLine ("      Menu Pick = {}\n", iCPick);
		KOut.FormatLine ("    Menu Option = '{}'\n", selections.commands[iCPick]);
		KOut.FormatLine (" last-command:\n {}\n\n", g_sBuffer);
		KOut.Write ("CONFIRM: ").Flush();
		Pause();
	}
	else
	{
		run_command (selections.commands[iCPick]);
	}

	return (iCPick);

} /* interpreter_loop */

//-----------------------------------------------------------------------------
void run_command (KStringView sCommand)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	bool    bRunNwait{false};
	int8_t iSysErr{0};

	if (g_sShell.Contains("csh") || g_sShell.Contains("zsh"))
	{
		g_sBuffer.Format ("{} -fc \"", g_sShell); // csh syntax
	}
	else
	{
		g_sBuffer.Format ("{} -c \"", g_sShell);  // bash syntax
	}

	if (sCommand.StartsWith (RUN_N_WAIT))
	{
		bRunNwait = true;
		g_sBuffer += sCommand.Mid (strlen("runNwait "));
	}

	g_sBuffer += kFormat ("{}\"", sCommand);

	iSysErr = kSystem (g_sBuffer);

	if (bRunNwait || iSysErr)
	{
		KOut.Format ("\n{}", g_sWaitString).Flush();
		Pause();
	}

} /* run_command */

//-----------------------------------------------------------------------------
void parse_menufile (KStringViewZ sMenuFile, menuT* menu)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	bool    bReadCommand{false};
	size_t  iMaxWidth{0};
	KString sContents;
	int8_t iLineNo{0};
	int8_t iErrors{0};
	bool    bAtLeastOne{false};

	if (!kReadTextFile (sMenuFile, sContents, /*convert newlines=*/true))
	{
		KErr.FormatLine ("unixmenu: system error: couldn't read %s (%s)", sMenuFile, strerror(errno));
		exit (1);
	}

	/* DEFAULT MENU PARAMETERS: */
	menu->iNRows     = 0;
	menu->sMenuTitle = DEFAULT_TITLE;
	iMaxWidth        = menu->sMenuTitle.size();
	menu->sSelectMSG = DEFAULT_MSG;
	menu->iSleep     = g_iSleep;

	for (auto sLine : sContents.Split('\n'))
	{
		++iLineNo;
		sLine.Trim();
		if (!sLine)
		{
			// blank line
		}
		else if (sLine.starts_with ('#'))
		{
			switch (sLine[1])
			{
			// #! XXX    -menu title specification (should appear once in menufile)
			case '!':
				menu->sMenuTitle = sLine.Mid(3).TrimLeft();
				break;

			// #@ XXX     -menu option specification for NEXT LINE as COMMAND
			// #@         -menu option spacer (puts blanks lines in menu)
			case '@':
				if (sLine.size() > 3)
				{
					bReadCommand = true;
					menu->labels[menu->iNRows] = sLine.Mid(3);
				}
				else
				{
					menu->labels[menu->iNRows].clear();
					menu->labels[menu->iNRows].clear();
				    ++ (menu->iNRows);
				}
				break;

			default:
				// full line comment
				break;
			}
	    }
		else if (bReadCommand)
		{
			menu->commands[menu->iNRows] = sLine;
			++(menu->iNRows);
			bReadCommand = false;
			bAtLeastOne = true;
	    }
		else
		{
			KErr.FormatLine ("{}:{}: malformed line: {}", sMenuFile, iLineNo, sLine);
			++iErrors;
		}

	} // while parsing lines

	if (iErrors)
	{
		exit (1);
	}

	menu->iTopRow = (g_iHeight - menu->iNRows - 2) / 2;
	if (menu->iTopRow < 1) menu->iTopRow = 1;
	menu->iMargin = (g_iWidth - iMaxWidth - 8) / 2;
	if (menu->iMargin < 0) menu->iMargin = 0;

	if (! bAtLeastOne)
	{
		KErr.FormatLine ("unixmenu: format problem with {} (no #@ style comments)", sMenuFile);
	}

} /* parse_menufile */

//-----------------------------------------------------------------------------
int8_t select_item (menuT* menu, int8_t iCurrent)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	int8_t ii;
	int8_t iCPick{iCurrent};

	savetty ();
	raw_io ();

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	kDebug (1, "setup screen...");
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	initscr ();
	erase ();
	keef_box (menu->iMargin, menu->iTopRow-2, g_iWidth-menu->iMargin, menu->iTopRow+menu->iNRows+1);

	if (menu->iNRows >= 17)
	{
		move (menu->iTopRow-2, menu->iMargin+3);
		standout ();
		addstr (menu->sMenuTitle);
		standend ();
	}
	else
	{
		move (menu->iTopRow-3, menu->iMargin+3);
		addstr (menu->sMenuTitle);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	kDebug (1, "draw initial menu {} options...", menu->iNRows);
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	for (ii = (menu->iNRows)-1; ii >= 0; --ii)
	{
		kDebug (1, "drawing row {}: {}", ii, menu->labels[ii]);
		move (menu->iTopRow+ii, menu->iMargin+3);
		if (ii==iCPick) standout();
		addstr (menu->labels[ii]);
		standend();
	}

	while (!(menu->labels[iCPick]))
	{
		++iCPick;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	kDebug (1, "loop through menu...");
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	iCPick = menu_loop (menu, iCPick);

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	kDebug (1, "display selection message...");
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (menu->sSelectMSG)
	{
		erase ();
		standend ();
		move (menu->iTopRow+iCPick, menu->iMargin+3);
		addstr (menu->labels[iCPick]);
		move (menu->iTopRow+iCPick-4, menu->iMargin);
		addstr (menu->sSelectMSG);
		keef_box (menu->iMargin, menu->iTopRow+iCPick-2, g_iWidth - menu->iMargin - 2, menu->iTopRow+iCPick+2);

		refresh ();
		sleep (menu->iSleep);
	}

	erase ();
	refresh ();
	endwin ();

	cooked_io();

	return (iCPick);

} /* select_item */

//-----------------------------------------------------------------------------
int8_t menu_loop (menuT* menu, int8_t iCPick)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	int8_t  iOPick{-1};
	bool    bDone{false};
	char    c;

	do
	{
		if (iOPick != iCPick)
		{
		    if (iOPick != -1)
		    {
				move (menu->iTopRow+iOPick, menu->iMargin+3);
				standend ();
				addstr (menu->labels[iOPick]);
			}

			move (menu->iTopRow+iCPick, menu->iMargin+3);
			standout ();
			addstr (menu->labels[iCPick]);

			iOPick = iCPick;
		}

		refresh ();

		c = getch();
		c &= ~(0x80);
		switch (c)
		{
		case 3:
		case 4:
		case 10:
		case 13: /*CONFIRM*/
			bDone = true;

		case 27: /*IGNORE*/
			break;

		case 'k':
		case 'K':
		case 16:
		case 65: /*UP*/
			do
			{
				if (iCPick) --iCPick;
				else iCPick = menu->iNRows - 1;
			}
			while (!(menu->labels[iCPick]));
			break;

		case 8:
		case 'j':
		case 'J':
		case 14:
		case 66: /*DOWN*/
			do
			{
			    if (iCPick < (menu->iNRows - 1)) ++iCPick;
			    else iCPick = 0;
			}
			while (!(menu->labels[iCPick]));
			break;

	    default:
			break;
		}

	} while (!bDone) ;

	return (iCPick);

} /* menu_loop */

//-----------------------------------------------------------------------------
void keef_box (int x1, int y1, int x2, int y2)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	int i;

	if (x2 < x1) return;
	if (y2 < y1) return;

	standout ();

	for (i=x1; i <= x2; ++i)
	{
		move (y1, i);	addch (' ');
		move (y2, i);	addch (' ');
	}

	for (i=y1; i <= y2; ++i)
	{
		move (i, x1);	addch (' ');
		move (i, x2);	addch (' ');
	}

	standend ();

} /* keef_box */

//-----------------------------------------------------------------------------
void synopsis_abort ()
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	for (int ii=0; *g_Synopsis[ii]; ++ii)
	{
		KOut.FormatLine ("{}", g_Synopsis[ii]);
	}

	exit (1);

} /* synopsis_abort */

//-----------------------------------------------------------------------------
void warning (KStringView string, KStringView sarg)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	standend ();

	move (24, 10);
	addch (7);
	clrtoeol ();

	move (24, 10);
	KOut.Format (KRuntimeFormat(string), sarg).Flush();
	refresh ();

	sleep (g_iSleep*3);
	move (24, 10);
	clrtoeol ();
	refresh ();

} /* warning */

//-----------------------------------------------------------------------------
int8_t error_and_exit (KStringView string, KStringView sarg)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	KErr.Write ("unixmenu: ");
	KErr.Format (KRuntimeFormat(string), sarg);
	KErr.Write (".\n");

	exit (1);

} /* error_and_exit */

