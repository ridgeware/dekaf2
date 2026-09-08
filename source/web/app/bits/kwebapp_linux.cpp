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


#include <dekaf2/web/app/bits/kwebapp_platform.h>

#if DEKAF2_HAS_WEBVIEW && !DEKAF2_IS_MACOS && !DEKAF2_IS_WINDOWS

// the GTK platforms: what the shell tools provide today, native dialogs and menus follow

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/system/os/ksystem.h>
#include <thread>

DEKAF2_NAMESPACE_BEGIN

namespace kwebapp {

namespace {

// static initialization runs on the thread that later runs the UI loop
const std::thread::id s_MainThread = std::this_thread::get_id();

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool IsMainThread()
//-----------------------------------------------------------------------------
{
	return std::this_thread::get_id() == s_MainThread;
}

//-----------------------------------------------------------------------------
bool IsBundled()
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
bool OpenExternal(KStringView sURL)
//-----------------------------------------------------------------------------
{
	KString sCommand = kFormat("xdg-open {}", kEscapeForCommands(sURL));
	std::thread([sCommand] { kSystem(sCommand); }).detach();
	return true;

} // OpenExternal

//-----------------------------------------------------------------------------
bool Notify(KStringView sTitle, KStringView sBody)
//-----------------------------------------------------------------------------
{
	KString sCommand = kFormat("notify-send {} {}", kEscapeForCommands(sTitle), kEscapeForCommands(sBody));
	std::thread([sCommand] { kSystem(sCommand); }).detach();
	return true;

} // Notify

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* /*pWindow*/, KStringView /*sTitle*/, const std::vector<KString>& /*Extensions*/, bool /*bMultiple*/, bool /*bDirectories*/)
//-----------------------------------------------------------------------------
{
	kDebug(1, "no file dialogs on this platform yet");
	return {};

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* /*pWindow*/, KStringView /*sTitle*/, KStringView /*sSuggestedName*/, const std::vector<KString>& /*Extensions*/)
//-----------------------------------------------------------------------------
{
	kDebug(1, "no file dialogs on this platform yet");
	return {};

} // SaveFileDialog

//-----------------------------------------------------------------------------
void SetMenu(KStringView /*sAppName*/, const KJSON& /*jMenus*/, std::function<void(KStringView)> /*OnAction*/, std::function<void()> /*OnQuit*/)
//-----------------------------------------------------------------------------
{
	kDebug(2, "no application menus on this platform yet");

} // SetMenu

//-----------------------------------------------------------------------------
bool GetWindowFrame(void* /*pWindow*/, WindowFrame& /*Frame*/)
//-----------------------------------------------------------------------------
{
	return false;

} // GetWindowFrame

//-----------------------------------------------------------------------------
void SetWindowFrame(void* /*pWindow*/, const WindowFrame& /*Frame*/)
//-----------------------------------------------------------------------------
{
} // SetWindowFrame

//-----------------------------------------------------------------------------
void WatchWindowFrame(void* /*pWindow*/, std::function<void(const WindowFrame&)> /*OnChange*/)
//-----------------------------------------------------------------------------
{
} // WatchWindowFrame

//-----------------------------------------------------------------------------
void UnwatchWindowFrame()
//-----------------------------------------------------------------------------
{
} // UnwatchWindowFrame

//-----------------------------------------------------------------------------
bool ActivateProcess(int64_t /*iPID*/)
//-----------------------------------------------------------------------------
{
	return false;

} // ActivateProcess

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW && !DEKAF2_IS_MACOS && !DEKAF2_IS_WINDOWS
