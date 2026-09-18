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
#include <unistd.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <pthread.h>
#include <cstring>
#include <algorithm>
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

//-----------------------------------------------------------------------------
// add a method to an object's class, unless the class answers the selector
// already - the way to complete webview/webview's delegates
void AddMethod(id Object, const char* sSelector, IMP Implementation, const char* sTypes)
//-----------------------------------------------------------------------------
{
	if (!Object)
	{
		return;
	}

	auto Cls = object_getClass(Object);
	auto Sel = sel_registerName(sSelector);

	if (!class_respondsToSelector(Cls, Sel))
	{
		class_addMethod(Cls, Sel, Implementation, sTypes);
	}

} // AddMethod

// notifications without tag get a generated identifier with this prefix
constexpr KStringView sGeneratedTagPrefix = "kwebapp-";

// the menu's target: one object of a class made at run time, its methods look up
// the action by the item's tag
std::mutex                       s_MenuMutex;
std::vector<KString>             s_MenuActions;
// the tray symbol: the status item, its entries' actions, the handler
id                               s_StatusItem { nullptr };
std::vector<KString>             s_TrayActions;
std::function<void(KStringView)> s_OnTrayAction;
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
void TrayMenuAction(id, SEL, id Sender)
//-----------------------------------------------------------------------------
{
	auto iTag = Msg<long>(Sender, "tag");

	KString                          sAction;
	std::function<void(KStringView)> OnAction;
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);

		if (iTag >= 0 && static_cast<std::size_t>(iTag) < s_TrayActions.size())
		{
			sAction  = s_TrayActions[static_cast<std::size_t>(iTag)];
			OnAction = s_OnTrayAction;
		}
	}

	if (OnAction && !sAction.empty())
	{
		OnAction(sAction);
	}

} // TrayMenuAction

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
		class_addMethod(Cls, sel_registerName("trayAction:"), reinterpret_cast<IMP>(TrayMenuAction), "v@:@");
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
bool Notify(KStringView sTitle, KStringView sBody, KStringView sTag, KStringView sImagePath)
//-----------------------------------------------------------------------------
{
	if (!IsBundled())
	{
		// the notification center serves application bundles only - a bare
		// binary lets the scripting bridge post the notification, without tag
		// and picture. The texts are escaped for the AppleScript string, the
		// script for the shell
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

	if (!sImagePath.empty())
	{
		// the attachment takes the file over into the notification store
		auto URL        = Msg<id>(Class("NSURL"), "fileURLWithPath:", NSStr(sImagePath));
		id   Error      = nullptr;
		auto Attachment = Msg<id>(Class("UNNotificationAttachment"), "attachmentWithIdentifier:URL:options:error:",
		                          NSStr("image"), URL, static_cast<id>(nullptr), &Error);

		if (Attachment)
		{
			Msg<void>(Content, "setAttachments:", Msg<id>(Class("NSArray"), "arrayWithObject:", Attachment));
		}
		else
		{
			kDebug(1, "cannot attach {}: {}", sImagePath, Str(Msg<id>(Error, "localizedDescription")));
		}
	}

	// the tag is the request's identifier: a second notification with the same
	// tag replaces the first
	static std::atomic<uint64_t> s_iCounter { 0 };
	auto sIdentifier = sTag.empty() ? kFormat("{}{}", sGeneratedTagPrefix, ++s_iCounter) : KString(sTag);
	auto Request     = Msg<id>(Class("UNNotificationRequest"), "requestWithIdentifier:content:trigger:",
	                           NSStr(sIdentifier), Content, static_cast<id>(nullptr));

	Msg<void>(Center, "addNotificationRequest:withCompletionHandler:", Request, ^(id Error)
	{
		if (Error)
		{
			kDebug(1, "notification failed: {}", Str(Msg<id>(Error, "localizedDescription")));
		}
	});

	return true;

} // Notify

namespace {

std::mutex                       s_NotifyMutex;
std::function<void(KStringView)> s_OnNotificationClick;

//-----------------------------------------------------------------------------
// userNotificationCenter:willPresentNotification:withCompletionHandler: - the
// application is active, and the notification shall show nevertheless
void WillPresentNotification(id, SEL, id, id, void (^Completion)(unsigned long))
//-----------------------------------------------------------------------------
{
	// UNNotificationPresentationOptions: sound is 1<<1, alert 1<<2 (until macOS
	// 10.15), list 1<<3 and banner 1<<4 (from macOS 11 on)
	if (__builtin_available(macOS 11.0, *))
	{
		Completion((1 << 4) | (1 << 3) | (1 << 1));
	}
	else
	{
		Completion((1 << 2) | (1 << 1));
	}

} // WillPresentNotification

//-----------------------------------------------------------------------------
// userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:
void DidReceiveNotificationResponse(id, SEL, id, id Response, void (^Completion)(void))
//-----------------------------------------------------------------------------
{
	// the click on the notification itself, not on a button or the close control
	if (Str(Msg<id>(Response, "actionIdentifier")) == "com.apple.UNNotificationDefaultActionIdentifier")
	{
		auto sIdentifier = Str(Msg<id>(Msg<id>(Msg<id>(Response, "notification"), "request"), "identifier"));

		std::function<void(KStringView)> OnClick;
		{
			std::lock_guard<std::mutex> Lock(s_NotifyMutex);
			OnClick = s_OnNotificationClick;
		}

		if (OnClick)
		{
			// a generated identifier is no tag
			OnClick(sIdentifier.starts_with(sGeneratedTagPrefix) ? KStringView{} : KStringView(sIdentifier));
		}
	}

	Completion();

} // DidReceiveNotificationResponse

//-----------------------------------------------------------------------------
id NotificationDelegate()
//-----------------------------------------------------------------------------
{
	static id s_Delegate = []
	{
		auto Cls = objc_allocateClassPair(objc_getClass("NSObject"), "KWebAppNotificationDelegate", 0);

		if (auto Protocol = objc_getProtocol("UNUserNotificationCenterDelegate"))
		{
			class_addProtocol(Cls, Protocol);
		}

		class_addMethod(Cls, sel_registerName("userNotificationCenter:willPresentNotification:withCompletionHandler:"),
		                reinterpret_cast<IMP>(WillPresentNotification), "v@:@@@?");
		class_addMethod(Cls, sel_registerName("userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:"),
		                reinterpret_cast<IMP>(DidReceiveNotificationResponse), "v@:@@@?");
		objc_registerClassPair(Cls);
		return Msg<id>(Msg<id>(reinterpret_cast<id>(Cls), "alloc"), "init");
	}();

	return s_Delegate;

} // NotificationDelegate

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool WatchNotifications(std::function<void(KStringView)> OnClick)
//-----------------------------------------------------------------------------
{
	bool bWatch = OnClick != nullptr;
	{
		std::lock_guard<std::mutex> Lock(s_NotifyMutex);
		s_OnNotificationClick = std::move(OnClick);
	}

	if (!bWatch || !IsBundled())
	{
		return false;
	}

	auto Center = Msg<id>(Class("UNUserNotificationCenter"), "currentNotificationCenter");

	if (!Center)
	{
		return false;
	}

	// the delegate keeps notifications visible while the application is active
	// and hears the clicks - the center holds it weakly, we for the process
	Msg<void>(Center, "setDelegate:", NotificationDelegate());
	return true;

} // WatchNotifications

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
void SetMenu(void* /*pWindow*/, KStringView sAppName, const KJSON& jMenus, bool bPasteShortcut, std::function<KString(KStringView)> Text,
             std::function<void(KStringView)> OnAction, std::function<void()> OnQuit)
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
	AddItem(AppMenu, Text("kwa.menu.about"), "orderFrontStandardAboutPanel:", "");
	AddSeparator(AppMenu);
	AddItem(AppMenu, Text("kwa.menu.hide"), "hide:", "h");
	AddSeparator(AppMenu);
	AddItem(AppMenu, Text("kwa.menu.quit"), "quitAction:", "q", MenuTarget());
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
	auto Edit = NewMenu(Text("kwa.menu.edit"));
	AddItem(Edit, Text("kwa.menu.edit.undo"),      "undo:",      "z");
	AddItem(Edit, Text("kwa.menu.edit.redo"),      "redo:",      "Z");
	AddSeparator(Edit);
	AddItem(Edit, Text("kwa.menu.edit.cut"),       "cut:",       "x");
	AddItem(Edit, Text("kwa.menu.edit.copy"),      "copy:",      "c");
	// with the key equivalent the menu answers Cmd-V before the page gets a
	// keydown - an application that reads the clipboard itself leaves it out
	AddItem(Edit, Text("kwa.menu.edit.paste"),     "paste:",     bPasteShortcut ? "v" : "");
	AddItem(Edit, Text("kwa.menu.edit.selectAll"), "selectAll:", "a");
	AddSeparator(Edit);
	AddItem(Edit, Text("kwa.menu.edit.dictation"), "startDictation:",             "");
	AddItem(Edit, Text("kwa.menu.edit.emoji"),     "orderFrontCharacterPalette:", "");
	AddSubmenu(Main, Edit);

	auto Window = NewMenu(Text("kwa.menu.window"));
	AddItem(Window, Text("kwa.menu.window.minimize"), "performMiniaturize:", "m");
	AddItem(Window, Text("kwa.menu.window.zoom"),     "performZoom:",        "");
	AddSeparator(Window);
	AddItem(Window, Text("kwa.menu.window.front"),    "arrangeInFront:", "");
	AddSubmenu(Main, Window);

	Msg<void>(App, "setMainMenu:",    Main);
	Msg<void>(App, "setWindowsMenu:", Window);

} // SetMenu

namespace {

//-----------------------------------------------------------------------------
// the image for the status item: an SF Symbol, a file, or the application's icon
id TrayImage(KStringView sIcon)
//-----------------------------------------------------------------------------
{
	id Image { nullptr };

	if (sIcon.starts_with("sf:"))
	{
		// a template image, so that it follows the menu bar's light and dark looks
		Image = Msg<id>(Class("NSImage"), "imageWithSystemSymbolName:accessibilityDescription:", NSStr(sIcon.substr(3)), static_cast<id>(nullptr));

		if (Image)
		{
			Msg<void>(Image, "setTemplate:", static_cast<ObjCBool>(1));
		}
	}
	else if (!sIcon.empty())
	{
		Image = Msg<id>(Msg<id>(Msg<id>(Class("NSImage"), "alloc"), "initWithContentsOfFile:", NSStr(sIcon)), "autorelease");

		if (Image)
		{
			// Apple's convention: a file named ...Template is a template image
			auto sBase = kBasename(sIcon);
			Msg<void>(Image, "setTemplate:", static_cast<ObjCBool>(sBase.contains("Template") ? 1 : 0));
			Msg<void>(Image, "setSize:", CGSizeMake(18, 18));
		}
	}

	if (!Image)
	{
		if (!sIcon.empty())
		{
			kDebug(1, "cannot load tray image '{}', using the application's icon", sIcon);
		}

		auto Icon = Msg<id>(Msg<id>(Class("NSApplication"), "sharedApplication"), "applicationIconImage");

		if (Icon)
		{
			Image = Msg<id>(Msg<id>(Icon, "copy"), "autorelease");
			Msg<void>(Image, "setSize:", CGSizeMake(18, 18));
		}
	}

	return Image;

} // TrayImage

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool SetTrayIcon(void* /*pWindow*/, KStringView sAppName, KStringView sIcon, const KJSON& jMenu, std::function<void(KStringView)> OnAction)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_MenuMutex);
		s_TrayActions.clear();
		s_OnTrayAction = std::move(OnAction);
	}

	if (!s_StatusItem)
	{
		// NSVariableStatusItemLength is -1. The status bar holds the item weakly,
		// we keep it for the life of the process
		auto Bar     = Msg<id>(Class("NSStatusBar"), "systemStatusBar");
		s_StatusItem = Msg<id>(Msg<id>(Bar, "statusItemWithLength:", -1.0), "retain");

		if (!s_StatusItem)
		{
			return false;
		}
	}

	auto Button = Msg<id>(s_StatusItem, "button");
	Msg<void>(Button, "setImage:",   TrayImage(sIcon));
	Msg<void>(Button, "setToolTip:", NSStr(sAppName));

	// the menu: a click on the symbol opens it
	auto Menu = NewMenu("");

	if (jMenu.is_array())
	{
		for (const auto& jItem : jMenu)
		{
			if (jItem["separator"].Bool())
			{
				AddSeparator(Menu);
				continue;
			}

			auto Item = AddItem(Menu, jItem["title"].String(), "trayAction:", "", MenuTarget());

			std::lock_guard<std::mutex> Lock(s_MenuMutex);
			s_TrayActions.push_back(jItem["action"].String());
			Msg<void>(Item, "setTag:", static_cast<long>(s_TrayActions.size() - 1));
		}
	}

	Msg<void>(s_StatusItem, "setMenu:", Menu);
	return true;

} // SetTrayIcon

//-----------------------------------------------------------------------------
bool UpdateTrayIcon(KStringView sIcon)
//-----------------------------------------------------------------------------
{
	if (!s_StatusItem)
	{
		return false;
	}

	Msg<void>(Msg<id>(s_StatusItem, "button"), "setImage:", TrayImage(sIcon));
	return true;

} // UpdateTrayIcon

//-----------------------------------------------------------------------------
void RemoveTrayIcon()
//-----------------------------------------------------------------------------
{
	if (s_StatusItem)
	{
		Msg<void>(Msg<id>(Class("NSStatusBar"), "systemStatusBar"), "removeStatusItem:", s_StatusItem);
		Msg<void>(s_StatusItem, "release");
		s_StatusItem = nullptr;
	}

	std::lock_guard<std::mutex> Lock(s_MenuMutex);
	s_TrayActions.clear();
	s_OnTrayAction = nullptr;

} // RemoveTrayIcon

//-----------------------------------------------------------------------------
std::vector<KString> PreferredLanguages()
//-----------------------------------------------------------------------------
{
	// the languages of the system settings, most preferred first, as "de-DE"
	std::vector<KString> Languages;

	auto Array  = Msg<id>(Class("NSLocale"), "preferredLanguages");
	auto iCount = Array ? Msg<unsigned long>(Array, "count") : 0;

	for (unsigned long i = 0; i < iCount; ++i)
	{
		auto sTag = Str(Msg<id>(Array, "objectAtIndex:", i));

		if (!sTag.empty())
		{
			Languages.push_back(std::move(sTag));
		}
	}

	return Languages;

} // PreferredLanguages

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

//-----------------------------------------------------------------------------
bool ActivateWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (pWindow)
	{
		Msg<void>(static_cast<id>(pWindow), "makeKeyAndOrderFront:", static_cast<id>(nullptr));
	}

	Msg<void>(Msg<id>(Class("NSApplication"), "sharedApplication"), "activateIgnoringOtherApps:", static_cast<ObjCBool>(1));
	return true;

} // ActivateWindow

namespace {

//-----------------------------------------------------------------------------
// the method WKWebView asks its UI delegate before getUserMedia() - WKPermissionDecisionGrant is 1
void GrantMediaCapture(id, SEL, id, id, id, long, void (^Decision)(long))
//-----------------------------------------------------------------------------
{
	Decision(1);
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool AllowMediaCapture(void* pWebView)
//-----------------------------------------------------------------------------
{
	if (!pWebView)
	{
		return false;
	}

	// webview/webview's UI delegate has no answer for the request, and WKWebView
	// denies then - we add the method to the delegate's class
	auto Delegate = Msg<id>(static_cast<id>(pWebView), "UIDelegate");

	if (!Delegate)
	{
		return false;
	}

	auto Cls = object_getClass(Delegate);
	auto Sel = sel_registerName("webView:requestMediaCapturePermissionForOrigin:initiatedByFrame:type:decisionHandler:");

	if (class_respondsToSelector(Cls, Sel))
	{
		return true;
	}

	// void, self, selector, three objects, the NSInteger type, the block
	return class_addMethod(Cls, Sel, reinterpret_cast<IMP>(GrantMediaCapture), "v@:@@@q@?") != 0;

} // AllowMediaCapture

//-----------------------------------------------------------------------------
bool SetBackgroundActivity(void* pWebView, bool bKeepRunning)
//-----------------------------------------------------------------------------
{
	if (!pWebView)
	{
		return false;
	}

	// WebKit suppresses the web content process of a page whose view is not
	// visible - occluded, minimized, on another space - and the page stops
	// answering its connections. The switch is a private preference with its
	// own setter, _setPageVisibilityBasedProcessSuppressionEnabled: - not
	// reachable through key-value coding, which would look for set_page...:
	// and raise NSUnknownKeyException. Called only if WebKit still has it.
	auto Preferences = Msg<id>(Msg<id>(static_cast<id>(pWebView), "configuration"), "preferences");

	if (!Preferences)
	{
		return false;
	}

	auto Setter = sel_registerName("_setPageVisibilityBasedProcessSuppressionEnabled:");

	if (class_respondsToSelector(object_getClass(Preferences), Setter))
	{
		reinterpret_cast<void(*)(id, SEL, ObjCBool)>(objc_msgSend)(Preferences, Setter, static_cast<ObjCBool>(bKeepRunning ? 0 : 1));
	}
	else
	{
		kDebug(1, "WebKit has no process suppression switch, the page may go silent in the background");
	}

	// and the application itself must not be napped: an activity that lasts
	// until the process ends, or until the option is taken back
	static id s_Activity { nullptr };
	auto ProcessInfo = Msg<id>(Class("NSProcessInfo"), "processInfo");

	if (bKeepRunning && !s_Activity)
	{
		// NSActivityUserInitiatedAllowingIdleSystemSleep: no App Nap, no automatic
		// termination, the display and the system may still sleep
		constexpr unsigned long long iOptions = 0x00FFFFFFULL & ~(1ULL << 20);
		s_Activity = Msg<id>(Msg<id>(ProcessInfo, "beginActivityWithOptions:reason:", iOptions, NSStr("keeps a live connection")), "retain");
	}
	else if (!bKeepRunning && s_Activity)
	{
		Msg<void>(ProcessInfo, "endActivity:", s_Activity);
		Msg<void>(s_Activity, "release");
		s_Activity = nullptr;
	}

	return true;

} // SetBackgroundActivity

//-----------------------------------------------------------------------------
void SetBadge(KStringView sText)
//-----------------------------------------------------------------------------
{
	auto Tile = Msg<id>(Msg<id>(Class("NSApplication"), "sharedApplication"), "dockTile");

	if (Tile)
	{
		Msg<void>(Tile, "setBadgeLabel:", sText.empty() ? static_cast<id>(nullptr) : NSStr(sText));
	}

} // SetBadge

//-----------------------------------------------------------------------------
int64_t RequestAttention(void* /*pWindow*/, bool bCritical)
//-----------------------------------------------------------------------------
{
	// NSCriticalRequest is 0, NSInformationalRequest is 10
	return Msg<long>(Msg<id>(Class("NSApplication"), "sharedApplication"), "requestUserAttention:", static_cast<long>(bCritical ? 0 : 10));

} // RequestAttention

//-----------------------------------------------------------------------------
void CancelAttention(void* /*pWindow*/, int64_t iRequest)
//-----------------------------------------------------------------------------
{
	Msg<void>(Msg<id>(Class("NSApplication"), "sharedApplication"), "cancelUserAttentionRequest:", static_cast<long>(iRequest));

} // CancelAttention

//-----------------------------------------------------------------------------
bool Confirm(void* pWindow, KStringView sTitle, KStringView sText, KStringView sOK, KStringView sCancel)
//-----------------------------------------------------------------------------
{
	// Record who is in front now, and get our own window out of the way: when the
	// panel closes, AppKit makes the next window key, and a key window on another
	// space takes the user there. With the window ordered out there is no such
	// candidate; afterwards the previous application gets the focus back, and
	// the window returns to its space without becoming key
	auto Window   = static_cast<id>(pWindow);
	auto Previous = Msg<id>(Msg<id>(Class("NSWorkspace"), "sharedWorkspace"), "frontmostApplication");
	bool bWasVisible = Window && Msg<ObjCBool>(Window, "isVisible");

	if (bWasVisible)
	{
		Msg<void>(Window, "orderOut:", static_cast<id>(nullptr));
	}

	auto Alert = Msg<id>(Msg<id>(Class("NSAlert"), "alloc"), "init");
	Msg<void>(Alert, "setMessageText:", NSStr(sTitle));

	if (!sText.empty())
	{
		Msg<void>(Alert, "setInformativeText:", NSStr(sText));
	}

	Msg<id>(Alert, "addButtonWithTitle:", NSStr(sOK));
	Msg<id>(Alert, "addButtonWithTitle:", NSStr(sCancel));

	// the panel joins the active space - where the user is, which the window
	// may not be - before the application comes to the front with it; with a
	// window on the active space, macOS does not switch spaces on activation
	auto Panel = Msg<id>(Alert, "window");
	constexpr unsigned long iMoveToActiveSpace = 1UL << 1;   // NSWindowCollectionBehaviorMoveToActiveSpace
	Msg<void>(Panel, "setCollectionBehavior:", iMoveToActiveSpace);
	Msg<void>(Panel, "orderFrontRegardless");
	Msg<void>(Msg<id>(Class("NSApplication"), "sharedApplication"), "activateIgnoringOtherApps:", static_cast<ObjCBool>(1));

	auto iResponse = Msg<long>(Alert, "runModal");
	Msg<void>(Alert, "release");

	if (Previous && Msg<int>(Previous, "processIdentifier") != getpid())
	{
		constexpr unsigned long iIgnoringOtherApps = 1UL << 1;   // NSApplicationActivateIgnoringOtherApps
		Msg<ObjCBool>(Previous, "activateWithOptions:", iIgnoringOtherApps);
	}

	if (bWasVisible)
	{
		Msg<void>(Window, "orderFront:", static_cast<id>(nullptr));
	}

	return iResponse == 1000;   // NSAlertFirstButtonReturn

} // Confirm

namespace {

// the navigation policy: one delegate object of a class made at run time. It
// is held here, as WKWebView and WKDownload keep only weak references to their
// delegates, and without it downloads are silently dropped
std::mutex            s_NavigationMutex;
NavigationPolicy      s_Policy;
// the downloads in progress: the download object and its destination. A few
// at a time, one lookup when a download ends - a vector is the right size
std::vector<std::pair<id, KString>> s_Downloads;

//-----------------------------------------------------------------------------
NavigationPolicy Policy()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_NavigationMutex);
	return s_Policy;
}

//-----------------------------------------------------------------------------
KString TakeDownload(id Download)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_NavigationMutex);
	auto it = std::find_if(s_Downloads.begin(), s_Downloads.end(), [Download](const auto& Entry) { return Entry.first == Download; });

	if (it == s_Downloads.end())
	{
		return {};
	}

	auto sPath = std::move(it->second);
	s_Downloads.erase(it);
	return sPath;
}

//-----------------------------------------------------------------------------
KString URLOf(id NavigationAction)
//-----------------------------------------------------------------------------
{
	return Str(Msg<id>(Msg<id>(Msg<id>(NavigationAction, "request"), "URL"), "absoluteString"));
}

//-----------------------------------------------------------------------------
// webView:decidePolicyForNavigationAction:decisionHandler: - the policy is 0
// cancel, 1 allow, 2 download (macOS 11.3)
void DecideNavigationAction(id, SEL, id, id Action, void (^Decision)(long))
//-----------------------------------------------------------------------------
{
	if (__builtin_available(macOS 11.3, *))
	{
		// a link with the download attribute
		if (Msg<ObjCBool>(Action, "shouldPerformDownload") != 0)
		{
			Decision(2);
			return;
		}
	}

	// the top level: the main frame, or no frame yet (a new window)
	auto Target = Msg<id>(Action, "targetFrame");
	bool bTop   = !Target || Msg<ObjCBool>(Target, "isMainFrame") != 0;

	if (bTop)
	{
		auto P = Policy();

		if (P.OnNavigate && !P.OnNavigate(URLOf(Action), /*bNewWindow*/ !Target))
		{
			Decision(0);
			return;
		}
	}

	Decision(1);

} // DecideNavigationAction

//-----------------------------------------------------------------------------
// webView:decidePolicyForNavigationResponse:decisionHandler: - what the view
// cannot show, and what the server sends as attachment, becomes a download
void DecideNavigationResponse(id, SEL, id, id Response, void (^Decision)(long))
//-----------------------------------------------------------------------------
{
	if (__builtin_available(macOS 11.3, *))
	{
		bool bDownload = Msg<ObjCBool>(Response, "canShowMIMEType") == 0;

		if (!bDownload)
		{
			auto URLResponse = Msg<id>(Response, "response");

			if (URLResponse && Msg<ObjCBool>(URLResponse, "respondsToSelector:", sel_registerName("valueForHTTPHeaderField:")) != 0)
			{
				auto sDisposition = Str(Msg<id>(URLResponse, "valueForHTTPHeaderField:", NSStr("Content-Disposition")));
				bDownload = sDisposition.ToLowerASCII().contains("attachment");
			}
		}

		if (bDownload)
		{
			Decision(2);
			return;
		}
	}

	Decision(1);

} // DecideNavigationResponse

//-----------------------------------------------------------------------------
// webView:navigationResponse:didBecomeDownload: and webView:navigationAction:didBecomeDownload:
void DidBecomeDownload(id Self, SEL, id, id, id Download)
//-----------------------------------------------------------------------------
{
	Msg<void>(Download, "setDelegate:", Self);

} // DidBecomeDownload

//-----------------------------------------------------------------------------
// download:decideDestinationUsingResponse:suggestedFilename:completionHandler:
void DecideDownloadDestination(id, SEL, id Download, id, id SuggestedName, void (^Completion)(id))
//-----------------------------------------------------------------------------
{
	auto    P = Policy();
	KString sPath;

	if (P.DownloadPath)
	{
		sPath = P.DownloadPath(Str(SuggestedName));
	}

	if (sPath.empty())
	{
		// no destination cancels the download
		kDebug(1, "no destination for download '{}', cancelling it", Str(SuggestedName));
		Completion(nullptr);
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(s_NavigationMutex);
		s_Downloads.emplace_back(Download, sPath);
	}

	Completion(Msg<id>(Class("NSURL"), "fileURLWithPath:", NSStr(sPath)));

} // DecideDownloadDestination

//-----------------------------------------------------------------------------
// downloadDidFinish:
void DownloadDidFinish(id, SEL, id Download)
//-----------------------------------------------------------------------------
{
	auto sPath = TakeDownload(Download);
	auto P     = Policy();

	if (P.OnDownload)
	{
		P.OnDownload(sPath, KStringView{});
	}

} // DownloadDidFinish

//-----------------------------------------------------------------------------
// download:didFailWithError:resumeData:
void DownloadDidFail(id, SEL, id Download, id Error, id)
//-----------------------------------------------------------------------------
{
	auto sPath  = TakeDownload(Download);
	auto sError = Str(Msg<id>(Error, "localizedDescription"));
	auto P      = Policy();

	if (P.OnDownload)
	{
		P.OnDownload(sPath, sError.empty() ? KStringView("download failed") : KStringView(sError));
	}

} // DownloadDidFail

//-----------------------------------------------------------------------------
// webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures: -
// window.open() and target="_blank" ask for a second view, which we do not have
id CreateWebView(id, SEL, id, id, id Action, id)
//-----------------------------------------------------------------------------
{
	auto sURL = URLOf(Action);

	if (!sURL.empty())
	{
		auto P = Policy();

		if (P.OnNavigate)
		{
			P.OnNavigate(sURL, /*bNewWindow*/ true);
		}
	}

	return nullptr;

} // CreateWebView

//-----------------------------------------------------------------------------
id NavigationDelegate()
//-----------------------------------------------------------------------------
{
	static id s_Delegate = []
	{
		auto Cls = objc_allocateClassPair(objc_getClass("NSObject"), "KWebAppNavigationDelegate", 0);

		for (auto sProtocol : { "WKNavigationDelegate", "WKDownloadDelegate" })
		{
			if (auto Protocol = objc_getProtocol(sProtocol))
			{
				class_addProtocol(Cls, Protocol);
			}
		}

		class_addMethod(Cls, sel_registerName("webView:decidePolicyForNavigationAction:decisionHandler:"),
		                reinterpret_cast<IMP>(DecideNavigationAction), "v@:@@@?");
		class_addMethod(Cls, sel_registerName("webView:decidePolicyForNavigationResponse:decisionHandler:"),
		                reinterpret_cast<IMP>(DecideNavigationResponse), "v@:@@@?");
		class_addMethod(Cls, sel_registerName("webView:navigationResponse:didBecomeDownload:"),
		                reinterpret_cast<IMP>(DidBecomeDownload), "v@:@@@");
		class_addMethod(Cls, sel_registerName("webView:navigationAction:didBecomeDownload:"),
		                reinterpret_cast<IMP>(DidBecomeDownload), "v@:@@@");
		class_addMethod(Cls, sel_registerName("download:decideDestinationUsingResponse:suggestedFilename:completionHandler:"),
		                reinterpret_cast<IMP>(DecideDownloadDestination), "v@:@@@@?");
		class_addMethod(Cls, sel_registerName("downloadDidFinish:"),
		                reinterpret_cast<IMP>(DownloadDidFinish), "v@:@");
		class_addMethod(Cls, sel_registerName("download:didFailWithError:resumeData:"),
		                reinterpret_cast<IMP>(DownloadDidFail), "v@:@@@");
		objc_registerClassPair(Cls);
		return Msg<id>(Msg<id>(reinterpret_cast<id>(Cls), "alloc"), "init");
	}();

	return s_Delegate;

} // NavigationDelegate

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool SetNavigationPolicy(void* pWebView, NavigationPolicy Policy)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_NavigationMutex);
		s_Policy = std::move(Policy);
	}

	if (!pWebView)
	{
		return false;
	}

	auto View = static_cast<id>(pWebView);
	Msg<void>(View, "setNavigationDelegate:", NavigationDelegate());

	// new windows are the UI delegate's business - webview/webview's has no
	// answer, so we add ours to its class
	AddMethod(Msg<id>(View, "UIDelegate"), "webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:",
	          reinterpret_cast<IMP>(CreateWebView), "@@:@@@@");

	return true;

} // SetNavigationPolicy

//-----------------------------------------------------------------------------
void ClearNavigationPolicy()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_NavigationMutex);
	s_Policy = NavigationPolicy{};
	s_Downloads.clear();

} // ClearNavigationPolicy

namespace {

std::mutex            s_AppMutex;
std::atomic<bool>     s_bHideOnClose { false };
std::function<void()> s_OnActivate;

//-----------------------------------------------------------------------------
void ApplicationActivated()
//-----------------------------------------------------------------------------
{
	std::function<void()> OnActivate;
	{
		std::lock_guard<std::mutex> Lock(s_AppMutex);
		OnActivate = s_OnActivate;
	}

	if (OnActivate)
	{
		OnActivate();
	}

} // ApplicationActivated

//-----------------------------------------------------------------------------
// windowShouldClose: - the close button asks first, and we hide instead when
// told to. webview/webview's windowWillClose: does not fire then
ObjCBool WindowShouldClose(id, SEL, id Window)
//-----------------------------------------------------------------------------
{
	if (s_bHideOnClose)
	{
		Msg<void>(Window, "orderOut:", static_cast<id>(nullptr));
		return 0;
	}

	return 1;

} // WindowShouldClose

//-----------------------------------------------------------------------------
// applicationShouldHandleReopen:hasVisibleWindows: - the dock icon was clicked
ObjCBool ShouldHandleReopen(id, SEL, id, ObjCBool bHasVisibleWindows)
//-----------------------------------------------------------------------------
{
	if (bHasVisibleWindows)
	{
		// the default: the windows come to the front
		return 1;
	}

	ApplicationActivated();
	return 0;

} // ShouldHandleReopen

//-----------------------------------------------------------------------------
// applicationDidBecomeActive:
void DidBecomeActive(id, SEL, id)
//-----------------------------------------------------------------------------
{
	ApplicationActivated();

} // DidBecomeActive

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool SetHideOnClose(void* pWindow, bool bHide)
//-----------------------------------------------------------------------------
{
	s_bHideOnClose = bHide;

	if (!pWindow)
	{
		return false;
	}

	// webview/webview's window delegate answers windowWillClose: only - we add
	// the question that comes before it
	AddMethod(Msg<id>(static_cast<id>(pWindow), "delegate"), "windowShouldClose:", reinterpret_cast<IMP>(WindowShouldClose), "c@:@");
	return true;

} // SetHideOnClose

//-----------------------------------------------------------------------------
void HideWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (pWindow)
	{
		Msg<void>(static_cast<id>(pWindow), "orderOut:", static_cast<id>(nullptr));
	}

} // HideWindow

//-----------------------------------------------------------------------------
bool IsWindowVisible(void* pWindow)
//-----------------------------------------------------------------------------
{
	return pWindow && Msg<ObjCBool>(static_cast<id>(pWindow), "isVisible") != 0;

} // IsWindowVisible

//-----------------------------------------------------------------------------
void WatchApplication(void* /*pWindow*/, std::function<void()> OnActivate)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_AppMutex);
		s_OnActivate = std::move(OnActivate);
	}

	// webview/webview's application delegate has neither method
	auto Delegate = Msg<id>(Msg<id>(Class("NSApplication"), "sharedApplication"), "delegate");
	AddMethod(Delegate, "applicationShouldHandleReopen:hasVisibleWindows:", reinterpret_cast<IMP>(ShouldHandleReopen), "c@:@c");
	AddMethod(Delegate, "applicationDidBecomeActive:",                      reinterpret_cast<IMP>(DidBecomeActive),    "v@:@");

} // WatchApplication

namespace {

//-----------------------------------------------------------------------------
// a Core Foundation object, released at the end of the scope
template<typename T>
class CFRef
//-----------------------------------------------------------------------------
{
public:
	explicit CFRef(T Ref = nullptr) : m_Ref(Ref) {}
	~CFRef() { if (m_Ref) CFRelease(m_Ref); }
	CFRef(const CFRef&) = delete;
	CFRef& operator=(const CFRef&) = delete;
	operator T() const { return m_Ref; }

private:
	T m_Ref;
};

//-----------------------------------------------------------------------------
CFStringRef CFStr(KStringView sText)
//-----------------------------------------------------------------------------
{
	return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(sText.data()),
	                               static_cast<CFIndex>(sText.size()), kCFStringEncodingUTF8, false);
}

//-----------------------------------------------------------------------------
// the query for one generic password: the service is the application, the
// account the key
CFMutableDictionaryRef SecretQuery(KStringView sService, KStringView sKey)
//-----------------------------------------------------------------------------
{
	auto Query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFRef<CFStringRef> Service(CFStr(sService));
	CFRef<CFStringRef> Account(CFStr(sKey));

	CFDictionarySetValue(Query, kSecClass,       kSecClassGenericPassword);
	CFDictionarySetValue(Query, kSecAttrService, Service);
	CFDictionarySetValue(Query, kSecAttrAccount, Account);

	return Query;
}

//-----------------------------------------------------------------------------
KString SecError(OSStatus Status)
//-----------------------------------------------------------------------------
{
	CFRef<CFStringRef> Message(SecCopyErrorMessageString(Status, nullptr));

	if (Message)
	{
		auto    iSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(Message), kCFStringEncodingUTF8) + 1;
		KString sMessage(static_cast<std::size_t>(iSize), '\0');

		if (CFStringGetCString(Message, sMessage.data(), iSize, kCFStringEncodingUTF8))
		{
			sMessage.resize(std::strlen(sMessage.c_str()));
			return sMessage;
		}
	}

	return kFormat("error {}", Status);
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool SaveSecret(KStringView sService, KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	CFRef<CFMutableDictionaryRef> Query(SecretQuery(sService, sKey));
	CFRef<CFDataRef>              Data(CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(sValue.data()), static_cast<CFIndex>(sValue.size())));

	// update an existing item, else add one
	CFRef<CFMutableDictionaryRef> Update(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFDictionarySetValue(Update, kSecValueData, Data);

	auto Status = SecItemUpdate(Query, Update);

	if (Status == errSecItemNotFound)
	{
		CFDictionarySetValue(Query, kSecValueData, Data);
		Status = SecItemAdd(Query, nullptr);
	}

	if (Status != errSecSuccess)
	{
		kDebug(1, "cannot store secret '{}' for {}: {}", sKey, sService, SecError(Status));
		return false;
	}

	return true;

} // SaveSecret

//-----------------------------------------------------------------------------
bool LoadSecret(KStringView sService, KStringView sKey, KString& sValue)
//-----------------------------------------------------------------------------
{
	CFRef<CFMutableDictionaryRef> Query(SecretQuery(sService, sKey));
	CFDictionarySetValue(Query, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(Query, kSecMatchLimit, kSecMatchLimitOne);

	CFTypeRef Found  = nullptr;
	auto      Status = SecItemCopyMatching(Query, &Found);

	// the result is ours to release
	CFRef<CFTypeRef> Result(Found);

	if (Status != errSecSuccess)
	{
		if (Status == errSecItemNotFound)
		{
			kDebug(2, "no secret '{}' for {}", sKey, sService);
		}
		else
		{
			kDebug(1, "cannot load secret '{}' for {}: {}", sKey, sService, SecError(Status));
		}

		return false;
	}

	auto Data = static_cast<CFDataRef>(Found);
	sValue.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(Data)), static_cast<std::size_t>(CFDataGetLength(Data)));
	return true;

} // LoadSecret

//-----------------------------------------------------------------------------
bool DeleteSecret(KStringView sService, KStringView sKey)
//-----------------------------------------------------------------------------
{
	CFRef<CFMutableDictionaryRef> Query(SecretQuery(sService, sKey));

	auto Status = SecItemDelete(Query);

	if (Status != errSecSuccess && Status != errSecItemNotFound)
	{
		kDebug(1, "cannot delete secret '{}' for {}: {}", sKey, sService, SecError(Status));
		return false;
	}

	return true;

} // DeleteSecret

//-----------------------------------------------------------------------------
void ClearWebCache(std::function<void()> Done)
//-----------------------------------------------------------------------------
{
	// the caches only - local storage and databases hold user data. WebKit
	// defines the type constants as their own names
	auto Types = Msg<id>(Class("NSSet"), "setWithArray:", NSArrayOf({ "WKWebsiteDataTypeDiskCache", "WKWebsiteDataTypeMemoryCache",
	                                                                  "WKWebsiteDataTypeFetchCache", "WKWebsiteDataTypeServiceWorkerRegistrations" }));
	auto Store = Msg<id>(Class("WKWebsiteDataStore"), "defaultDataStore");
	auto Since = Msg<id>(Class("NSDate"), "distantPast");

	Msg<void>(Store, "removeDataOfTypes:modifiedSince:completionHandler:", Types, Since, ^
	{
		kDebug(2, "web caches cleared");

		if (Done)
		{
			Done();
		}
	});

} // ClearWebCache

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW && DEKAF2_IS_MACOS
