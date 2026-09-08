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

#if DEKAF2_HAS_WEBVIEW && DEKAF2_IS_MACOS

// the macOS side, through the Objective-C runtime - no Objective-C source needed

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/system/os/ksystem.h>
#include <objc/objc-runtime.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <pthread.h>
#include <atomic>
#include <mutex>
#include <thread>

DEKAF2_NAMESPACE_BEGIN

namespace kwebapp {

namespace {

// BOOL travels as one byte in a register on both architectures
using ObjCBool = signed char;

//-----------------------------------------------------------------------------
// objc_msgSend with the signature of the method called
template<typename Result, typename... Args>
Result Msg(id Self, const char* sSelector, Args... args)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<Result(*)(id, SEL, Args...)>(objc_msgSend)(Self, sel_registerName(sSelector), args...);
}

//-----------------------------------------------------------------------------
// a method returning a rect - x86_64 returns structs through a different entry point
CGRect MsgRect(id Self, const char* sSelector)
//-----------------------------------------------------------------------------
{
#if defined(__x86_64__)
	return reinterpret_cast<CGRect(*)(id, SEL)>(objc_msgSend_stret)(Self, sel_registerName(sSelector));
#else
	return reinterpret_cast<CGRect(*)(id, SEL)>(objc_msgSend)(Self, sel_registerName(sSelector));
#endif
}

//-----------------------------------------------------------------------------
id Class(const char* sName)
//-----------------------------------------------------------------------------
{
	return reinterpret_cast<id>(objc_getClass(sName));
}

//-----------------------------------------------------------------------------
id NSStr(KStringView sText)
//-----------------------------------------------------------------------------
{
	KString sCopy(sText);
	return Msg<id>(Class("NSString"), "stringWithUTF8String:", sCopy.c_str());
}

//-----------------------------------------------------------------------------
KString Str(id NSString)
//-----------------------------------------------------------------------------
{
	if (!NSString)
	{
		return {};
	}

	auto sUTF8 = Msg<const char*>(NSString, "UTF8String");
	return sUTF8 ? KString(sUTF8) : KString{};
}

//-----------------------------------------------------------------------------
id NSArrayOf(const std::vector<KString>& Strings)
//-----------------------------------------------------------------------------
{
	auto Array = Msg<id>(Class("NSMutableArray"), "array");

	for (const auto& sString : Strings)
	{
		Msg<void>(Array, "addObject:", NSStr(sString));
	}

	return Array;
}

// the menu's target: one object of a class made at run time, its methods look up
// the action by the item's tag
std::mutex                       s_MenuMutex;
std::vector<KString>             s_MenuActions;
std::function<void(KStringView)> s_OnAction;
std::function<void()>            s_OnQuit;
std::function<void(const WindowFrame&)> s_OnFrame;

CGRect MsgRect(id Self, const char* sSelector);

//-----------------------------------------------------------------------------
void WindowChanged(id, SEL, id Notification)
//-----------------------------------------------------------------------------
{
	auto Window = Msg<id>(Notification, "object");

	std::function<void(const WindowFrame&)> OnFrame;
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		OnFrame = s_OnFrame;
	}

	if (Window && OnFrame)
	{
		auto Rect = MsgRect(Window, "frame");
		OnFrame(WindowFrame { static_cast<int32_t>(Rect.origin.x), static_cast<int32_t>(Rect.origin.y),
		                      static_cast<int32_t>(Rect.size.width), static_cast<int32_t>(Rect.size.height) });
	}

} // WindowChanged

//-----------------------------------------------------------------------------
void MenuAction(id, SEL, id Sender)
//-----------------------------------------------------------------------------
{
	auto iTag = Msg<long>(Sender, "tag");

	KString                          sAction;
	std::function<void(KStringView)> OnAction;
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);

		if (iTag >= 0 && static_cast<std::size_t>(iTag) < s_MenuActions.size())
		{
			sAction  = s_MenuActions[static_cast<std::size_t>(iTag)];
			OnAction = s_OnAction;
		}
	}

	if (OnAction && !sAction.empty())
	{
		OnAction(sAction);
	}

} // MenuAction

//-----------------------------------------------------------------------------
void QuitAction(id, SEL, id)
//-----------------------------------------------------------------------------
{
	std::function<void()> OnQuit;
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		OnQuit = s_OnQuit;
	}

	if (OnQuit)
	{
		OnQuit();
	}

} // QuitAction

//-----------------------------------------------------------------------------
id MenuTarget()
//-----------------------------------------------------------------------------
{
	static id s_Target = []
	{
		auto Cls = objc_allocateClassPair(objc_getClass("NSObject"), "KWebAppMenuTarget", 0);
		class_addMethod(Cls, sel_registerName("menuAction:"), reinterpret_cast<IMP>(MenuAction), "v@:@");
		class_addMethod(Cls, sel_registerName("quitAction:"), reinterpret_cast<IMP>(QuitAction), "v@:@");
		class_addMethod(Cls, sel_registerName("windowChanged:"), reinterpret_cast<IMP>(WindowChanged), "v@:@");
		objc_registerClassPair(Cls);
		return Msg<id>(Msg<id>(reinterpret_cast<id>(Cls), "alloc"), "init");
	}();

	return s_Target;

} // MenuTarget

//-----------------------------------------------------------------------------
id NewMenu(KStringView sTitle)
//-----------------------------------------------------------------------------
{
	return Msg<id>(Msg<id>(Class("NSMenu"), "alloc"), "initWithTitle:", NSStr(sTitle));
}

//-----------------------------------------------------------------------------
// an item - without target the first responder answers the selector
id AddItem(id Menu, KStringView sTitle, const char* sSelector, KStringView sKey, id Target = nullptr)
//-----------------------------------------------------------------------------
{
	auto Item = Msg<id>(Msg<id>(Class("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:",
	                    NSStr(sTitle), sSelector ? sel_registerName(sSelector) : static_cast<SEL>(nullptr), NSStr(sKey));

	if (Target)
	{
		Msg<void>(Item, "setTarget:", Target);
	}

	Msg<void>(Menu, "addItem:", Item);
	return Item;
}

//-----------------------------------------------------------------------------
void AddSeparator(id Menu)
//-----------------------------------------------------------------------------
{
	Msg<void>(Menu, "addItem:", Msg<id>(Class("NSMenuItem"), "separatorItem"));
}

//-----------------------------------------------------------------------------
void AddSubmenu(id MainMenu, id Submenu)
//-----------------------------------------------------------------------------
{
	auto Item = Msg<id>(Msg<id>(Class("NSMenuItem"), "alloc"), "init");
	Msg<void>(Item, "setSubmenu:", Submenu);
	Msg<void>(MainMenu, "addItem:", Item);
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool IsMainThread()
//-----------------------------------------------------------------------------
{
	return pthread_main_np() != 0;
}

//-----------------------------------------------------------------------------
bool IsBundled()
//-----------------------------------------------------------------------------
{
	return Msg<id>(Msg<id>(Class("NSBundle"), "mainBundle"), "bundleIdentifier") != nullptr;
}

//-----------------------------------------------------------------------------
bool OpenExternal(KStringView sURL)
//-----------------------------------------------------------------------------
{
	auto URL = Msg<id>(Class("NSURL"), "URLWithString:", NSStr(sURL));

	if (!URL)
	{
		return false;
	}

	return Msg<ObjCBool>(Msg<id>(Class("NSWorkspace"), "sharedWorkspace"), "openURL:", URL) != 0;

} // OpenExternal

//-----------------------------------------------------------------------------
bool Notify(KStringView sTitle, KStringView sBody)
//-----------------------------------------------------------------------------
{
	if (!IsBundled())
	{
		// the notification center serves application bundles only - a bare
		// binary lets the scripting bridge post the notification. The texts are
		// escaped for the AppleScript string, the script for the shell
		auto sScript  = kFormat("display notification \"{}\" with title \"{}\"",
		                        kEscapeChars(sBody, "\"\\", '\\'), kEscapeChars(sTitle, "\"\\", '\\'));
		auto sCommand = kFormat("osascript -e {}", kEscapeForCommands(sScript));
		std::thread([sCommand] { kSystem(sCommand); }).detach();
		return true;
	}

	auto Center = Msg<id>(Class("UNUserNotificationCenter"), "currentNotificationCenter");

	if (!Center)
	{
		return false;
	}

	// alert and sound - the system asks the user once
	Msg<void>(Center, "requestAuthorizationWithOptions:completionHandler:", static_cast<unsigned long>((1 << 1) | (1 << 2)),
	          ^(ObjCBool, id) {});

	auto Content = Msg<id>(Msg<id>(Class("UNMutableNotificationContent"), "alloc"), "init");
	Msg<void>(Content, "setTitle:", NSStr(sTitle));
	Msg<void>(Content, "setBody:",  NSStr(sBody));

	static std::atomic<uint64_t> s_iCounter { 0 };
	auto Request = Msg<id>(Class("UNNotificationRequest"), "requestWithIdentifier:content:trigger:",
	                       NSStr(kFormat("kwebapp-{}", ++s_iCounter)), Content, static_cast<id>(nullptr));

	Msg<void>(Center, "addNotificationRequest:withCompletionHandler:", Request, ^(id Error)
	{
		if (Error)
		{
			kDebug(1, "notification failed: {}", Str(Msg<id>(Error, "localizedDescription")));
		}
	});

	return true;

} // Notify

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* /*pWindow*/, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Files;

	auto Panel = Msg<id>(Class("NSOpenPanel"), "openPanel");
	Msg<void>(Panel, "setCanChooseFiles:",          static_cast<ObjCBool>(!bDirectories));
	Msg<void>(Panel, "setCanChooseDirectories:",    static_cast<ObjCBool>(bDirectories));
	Msg<void>(Panel, "setAllowsMultipleSelection:", static_cast<ObjCBool>(bMultiple));

	if (!sTitle.empty())
	{
		Msg<void>(Panel, "setMessage:", NSStr(sTitle));
	}

	if (!Extensions.empty())
	{
		Msg<void>(Panel, "setAllowedFileTypes:", NSArrayOf(Extensions));
	}

	// NSModalResponseOK
	if (Msg<long>(Panel, "runModal") == 1)
	{
		auto URLs   = Msg<id>(Panel, "URLs");
		auto iCount = Msg<unsigned long>(URLs, "count");

		for (unsigned long i = 0; i < iCount; ++i)
		{
			Files.push_back(Str(Msg<id>(Msg<id>(URLs, "objectAtIndex:", i), "path")));
		}
	}

	return Files;

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* /*pWindow*/, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	auto Panel = Msg<id>(Class("NSSavePanel"), "savePanel");

	if (!sTitle.empty())
	{
		Msg<void>(Panel, "setMessage:", NSStr(sTitle));
	}

	if (!sSuggestedName.empty())
	{
		Msg<void>(Panel, "setNameFieldStringValue:", NSStr(sSuggestedName));
	}

	if (!Extensions.empty())
	{
		Msg<void>(Panel, "setAllowedFileTypes:", NSArrayOf(Extensions));
	}

	if (Msg<long>(Panel, "runModal") != 1)
	{
		return {};
	}

	return Str(Msg<id>(Msg<id>(Panel, "URL"), "path"));

} // SaveFileDialog

//-----------------------------------------------------------------------------
void SetMenu(KStringView sAppName, const KJSON& jMenus, std::function<void(KStringView)> OnAction, std::function<void()> OnQuit)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		s_MenuActions.clear();
		s_OnAction = std::move(OnAction);
		s_OnQuit   = std::move(OnQuit);
	}

	// AppKit adds dictation and emoji entries to the Edit menu on its own, and
	// more than once - we tell it not to (in the volatile registration domain
	// only) and add the two entries ourselves
	{
		auto Yes   = Msg<id>(Class("NSNumber"), "numberWithBool:", static_cast<ObjCBool>(1));
		auto Keys  = NSArrayOf({ "NSDisabledDictationMenuItem", "NSDisabledCharacterPaletteMenuItem" });
		auto Vals  = Msg<id>(Class("NSMutableArray"), "array");
		Msg<void>(Vals, "addObject:", Yes);
		Msg<void>(Vals, "addObject:", Yes);
		auto Dict  = Msg<id>(Class("NSDictionary"), "dictionaryWithObjects:forKeys:", Vals, Keys);
		Msg<void>(Msg<id>(Class("NSUserDefaults"), "standardUserDefaults"), "registerDefaults:", Dict);
	}

	auto App  = Msg<id>(Class("NSApplication"), "sharedApplication");
	auto Main = NewMenu("");

	// the application menu - Quit ends the run loop through KWebApp, not the process
	auto AppMenu = NewMenu(sAppName);
	AddItem(AppMenu, kFormat("About {}", sAppName), "orderFrontStandardAboutPanel:", "");
	AddSeparator(AppMenu);
	AddItem(AppMenu, kFormat("Hide {}", sAppName), "hide:", "h");
	AddSeparator(AppMenu);
	AddItem(AppMenu, kFormat("Quit {}", sAppName), "quitAction:", "q", MenuTarget());
	AddSubmenu(Main, AppMenu);

	// the application's own menus
	if (jMenus.is_array())
	{
		for (const auto& jMenu : jMenus)
		{
			auto Menu = NewMenu(jMenu["title"].String());

			for (const auto& jItem : jMenu["items"])
			{
				if (jItem["separator"].Bool())
				{
					AddSeparator(Menu);
					continue;
				}

				auto Item = AddItem(Menu, jItem["title"].String(), "menuAction:", jItem["key"].String(), MenuTarget());

				std::lock_guard<std::mutex> Lock(s_MenuMutex);
				s_MenuActions.push_back(jItem["action"].String());
				Msg<void>(Item, "setTag:", static_cast<long>(s_MenuActions.size() - 1));
			}

			AddSubmenu(Main, Menu);
		}
	}

	// Edit: the first responder answers, and without these items the webview
	// has no keyboard shortcuts for copy and paste
	auto Edit = NewMenu("Edit");
	AddItem(Edit, "Undo",       "undo:",      "z");
	AddItem(Edit, "Redo",       "redo:",      "Z");
	AddSeparator(Edit);
	AddItem(Edit, "Cut",        "cut:",       "x");
	AddItem(Edit, "Copy",       "copy:",      "c");
	AddItem(Edit, "Paste",      "paste:",     "v");
	AddItem(Edit, "Select All", "selectAll:", "a");
	AddSeparator(Edit);
	AddItem(Edit, "Start Dictation\xE2\x80\xA6", "startDictation:",             "");
	AddItem(Edit, "Emoji & Symbols",               "orderFrontCharacterPalette:", "");
	AddSubmenu(Main, Edit);

	auto Window = NewMenu("Window");
	AddItem(Window, "Minimize", "performMiniaturize:", "m");
	AddItem(Window, "Zoom",     "performZoom:",        "");
	AddSeparator(Window);
	AddItem(Window, "Bring All to Front", "arrangeInFront:", "");
	AddSubmenu(Main, Window);

	Msg<void>(App, "setMainMenu:",    Main);
	Msg<void>(App, "setWindowsMenu:", Window);

} // SetMenu

//-----------------------------------------------------------------------------
bool GetWindowFrame(void* pWindow, WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return false;
	}

	auto Rect     = MsgRect(static_cast<id>(pWindow), "frame");
	Frame.iX      = static_cast<int32_t>(Rect.origin.x);
	Frame.iY      = static_cast<int32_t>(Rect.origin.y);
	Frame.iWidth  = static_cast<int32_t>(Rect.size.width);
	Frame.iHeight = static_cast<int32_t>(Rect.size.height);
	return true;

} // GetWindowFrame

//-----------------------------------------------------------------------------
void SetWindowFrame(void* pWindow, const WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	if (pWindow && Frame.iWidth > 0 && Frame.iHeight > 0)
	{
		Msg<void>(static_cast<id>(pWindow), "setFrame:display:",
		          CGRectMake(Frame.iX, Frame.iY, Frame.iWidth, Frame.iHeight), static_cast<ObjCBool>(1));
	}

} // SetWindowFrame

//-----------------------------------------------------------------------------
void WatchWindowFrame(void* pWindow, std::function<void(const WindowFrame&)> OnChange)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		s_OnFrame = std::move(OnChange);
	}

	auto Center = Msg<id>(Class("NSNotificationCenter"), "defaultCenter");

	for (auto sName : { "NSWindowDidMoveNotification", "NSWindowDidEndLiveResizeNotification", "NSWindowWillCloseNotification" })
	{
		Msg<void>(Center, "addObserver:selector:name:object:", MenuTarget(), sel_registerName("windowChanged:"), NSStr(sName), static_cast<id>(pWindow));
	}

} // WatchWindowFrame

//-----------------------------------------------------------------------------
void UnwatchWindowFrame()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		s_OnFrame = nullptr;
	}

	// the target observes nothing but the window
	Msg<void>(Msg<id>(Class("NSNotificationCenter"), "defaultCenter"), "removeObserver:", MenuTarget());

} // UnwatchWindowFrame

//-----------------------------------------------------------------------------
bool ActivateProcess(int64_t iPID)
//-----------------------------------------------------------------------------
{
	auto Running = Msg<id>(Class("NSRunningApplication"), "runningApplicationWithProcessIdentifier:", static_cast<int>(iPID));

	if (!Running)
	{
		return false;
	}

	// all windows, and in front of the caller
	return Msg<ObjCBool>(Running, "activateWithOptions:", static_cast<unsigned long>(1 | 2)) != 0;

} // ActivateProcess

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW && DEKAF2_IS_MACOS
