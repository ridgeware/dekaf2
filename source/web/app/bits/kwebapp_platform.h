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


#pragma once

/// @file kwebapp_platform.h
/// the platform side of KWebApp - one implementation per operating system, all
/// of them plain C++ (on macOS through the Objective-C runtime). Functions that
/// touch the window or the menu run on the UI thread only, KWebApp sees to that

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <cstdint>
#include <functional>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace kwebapp {

/// a window's position and size in screen coordinates
struct WindowFrame
{
	int32_t iX      { 0 };
	int32_t iY      { 0 };
	int32_t iWidth  { 0 };
	int32_t iHeight { 0 };
};

/// true on the thread that runs the UI loop
bool IsMainThread();
/// true when the process runs from an application bundle - some services need one
bool IsBundled();
/// open a URL in the default browser
bool OpenExternal(KStringView sURL);
/// post a system notification
bool Notify(KStringView sTitle, KStringView sBody);
/// modal open dialog. Extensions without the dot, empty for any file
std::vector<KString> OpenFileDialog(void* pWindow, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories);
/// modal save dialog, empty result when cancelled
KString SaveFileDialog(void* pWindow, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions);
/// install the application menu: the standard application, edit and window menus
/// plus the menus described in jMenus - an entry's action is reported to OnAction,
/// the quit entry calls OnQuit
void SetMenu(KStringView sAppName, const KJSON& jMenus, std::function<void(KStringView sAction)> OnAction, std::function<void()> OnQuit);
/// the window's position and size
bool GetWindowFrame(void* pWindow, WindowFrame& Frame);
void SetWindowFrame(void* pWindow, const WindowFrame& Frame);
/// report the window's frame whenever it moved, was resized, or is about to close -
/// the window itself is gone once the UI loop has ended
void WatchWindowFrame(void* pWindow, std::function<void(const WindowFrame&)> OnChange);
/// bring the windows of another process to the front
bool ActivateProcess(int64_t iPID);

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
