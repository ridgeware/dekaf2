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
/// The operating system specific part of KWebApp. This header declares the
/// functions, and there is one implementation file per operating system:
/// kwebapp_macos.cpp, kwebapp_windows.cpp and kwebapp_linux.cpp. All of them are
/// plain C++ - the macOS one calls Objective-C through the runtime functions,
/// so no Objective-C source file is needed. The functions that touch the window
/// or the menu must be called on the UI thread; KWebApp takes care of that, the
/// implementations do not have to check it.

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
/// post a system notification. sTag replaces an earlier notification with the
/// same tag, sImagePath attaches a picture file - both where the platform can
bool Notify(KStringView sTitle, KStringView sBody, KStringView sTag, KStringView sImagePath);
/// report clicks on notifications: OnClick gets the tag the notification was
/// posted with, or an empty string when it had none. Pass nullptr to stop the
/// reports. Returns false when the platform cannot report clicks
bool WatchNotifications(std::function<void(KStringView sTag)> OnClick);
/// modal open dialog. Extensions without the dot, empty for any file
std::vector<KString> OpenFileDialog(void* pWindow, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories);
/// modal save dialog, empty result when cancelled
KString SaveFileDialog(void* pWindow, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions);
/// install the application menu: the standard application, edit and window menus
/// plus the menus described in jMenus - an entry's action is reported to OnAction,
/// the quit entry calls OnQuit. macOS has one menu for the application, the
/// other platforms put a menu bar into the window
void SetMenu(void* pWindow, KStringView sAppName, const KJSON& jMenus, std::function<void(KStringView sAction)> OnAction, std::function<void()> OnQuit);
/// the window's position and size
bool GetWindowFrame(void* pWindow, WindowFrame& Frame);
void SetWindowFrame(void* pWindow, const WindowFrame& Frame);
/// report the window's frame whenever it moved, was resized, or is about to close -
/// the window itself is gone once the UI loop has ended
void WatchWindowFrame(void* pWindow, std::function<void(const WindowFrame&)> OnChange);
/// stop watching, before the window goes away
void UnwatchWindowFrame();
/// bring the windows of another process to the front
bool ActivateProcess(int64_t iPID);
/// bring this window to the front and activate the application
bool ActivateWindow(void* pWindow);
/// let pages in this webview use camera and microphone: the webview grants
/// getUserMedia() without a prompt of its own - the operating system has asked
/// already, with the bundle's usage descriptions. False when not supported
bool AllowMediaCapture(void* pWebView);
/// show a text on the application's icon (the dock badge), empty clears it
void SetBadge(KStringView sText);
/// ask for the user's attention while the application is in the background:
/// the dock icon bounces, the taskbar button flashes. Critical keeps asking
/// until the application is activated, else it asks once
/// @return a request id for CancelAttention(), 0 when nothing was requested
int64_t RequestAttention(void* pWindow, bool bCritical);
/// stop asking for attention
void CancelAttention(void* pWindow, int64_t iRequest);

/// what the shell decides for the webview's navigation
struct NavigationPolicy
{
	/// may the window show this URL? A new window is asked for by window.open()
	/// and target="_blank". False when the shell took care of it itself, e.g.
	/// opened it in the system browser
	std::function<bool(KStringView sURL, bool bNewWindow)>    OnNavigate;
	/// the file a download shall be written to, from the name the server or
	/// the page suggested - empty cancels the download
	std::function<KString(KStringView sSuggestedName)>        DownloadPath;
	/// a download has finished, or failed with sError set
	std::function<void(KStringView sPath, KStringView sError)> OnDownload;
};

/// route the webview's navigation through the policy: top-level navigations
/// and new windows are asked for, responses the webview cannot show and
/// attachments become downloads. False when the platform cannot enforce it
bool SetNavigationPolicy(void* pWebView, NavigationPolicy Policy);
/// forget the policy's handlers, before the window goes away
void ClearNavigationPolicy();
/// the close button hides the window instead of closing it
bool SetHideOnClose(void* pWindow, bool bHide);
/// hide the window, ActivateWindow() brings it back
void HideWindow(void* pWindow);
/// is the window on screen (not hidden, not miniaturized into oblivion)?
bool IsWindowVisible(void* pWindow);
/// report when the application is activated: a click on its icon, a switch to
/// it, a second start bringing it to the front. Pass nullptr to stop the reports
void WatchApplication(void* pWindow, std::function<void()> OnActivate);
/// store, load and delete a secret in the system's credential store, under the
/// application's service name. Load reports false when there is none
bool SaveSecret  (KStringView sService, KStringView sKey, KStringView sValue);
bool LoadSecret  (KStringView sService, KStringView sKey, KString& sValue);
bool DeleteSecret(KStringView sService, KStringView sKey);
/// drop the webview's caches: scripts, styles, fetch responses and service
/// worker registrations - not local storage and databases, they hold user
/// data. Done is called when the caches are gone, on the UI thread
void ClearWebCache(std::function<void()> Done);

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW
