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


// the tray icon's version 4 messages and the item dialogs need Windows 7 or newer
#if defined(_WIN32) && !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0601
#endif

#include <dekaf2/web/app/bits/kwebapp_platform.h>

#if DEKAF2_HAS_WEBVIEW && DEKAF2_IS_WINDOWS

// Windows, with WebView2. The window is webview/webview's, we subclass its
// procedure for the menu, the close button, the frame and the tray icon, and
// hook into the WebView2 controller for navigation, downloads, permissions and
// the menu shortcuts. Notifications are balloons on a tray icon: the toast
// center wants an AppUserModelID from an installer, which a plain executable
// has not - the tray icon appears with the first notification, or when the
// close button hides the window, and a click on it brings the window back.

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kutf.h>
#include <dekaf2/system/os/ksystem.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wincred.h>
#include <WebView2.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace kwebapp {

namespace {

// static initialization runs on the thread that later runs the UI loop
const std::thread::id s_MainThread = std::this_thread::get_id();

constexpr UINT     WM_APP_TRAY     = WM_APP + 0x100; // the tray icon's callback message
constexpr UINT     ID_MENU_BASE    = 0x1000;         // command ids of the menu entries
constexpr UINT     ID_MENU_QUIT    = 0x0FFF;
constexpr UINT_PTR SUBCLASS_ID     = 0x4B57;         // "KW"

//-----------------------------------------------------------------------------
// a string WebView2 or the shell handed out, freed when the scope ends
class CoString
//-----------------------------------------------------------------------------
{
public:
	CoString() = default;
	~CoString() { ::CoTaskMemFree(m_sText); }
	CoString(const CoString&) = delete;
	CoString& operator=(const CoString&) = delete;
	LPWSTR* operator&() { return &m_sText; }
	KString str() const { return kutf::Convert<KString>(m_sText); }

private:
	LPWSTR m_sText { nullptr };
};

//-----------------------------------------------------------------------------
// a COM pointer, released when the scope ends
template<typename T>
class ComPtr
//-----------------------------------------------------------------------------
{
public:
	ComPtr() = default;
	explicit ComPtr(T* p) : m_p(p) {}
	~ComPtr() { if (m_p) m_p->Release(); }
	ComPtr(const ComPtr&) = delete;
	ComPtr& operator=(const ComPtr&) = delete;
	T** operator&()       { return &m_p; }
	T*  operator->() const { return m_p; }
	T*  get()        const { return m_p; }
	operator bool()  const { return m_p != nullptr; }

private:
	T* m_p { nullptr };
};

//-----------------------------------------------------------------------------
// a WebView2 event handler: one COM object per registration, the callback does the work
template<typename Interface, typename Sender, typename Args>
class EventHandler : public Interface
//-----------------------------------------------------------------------------
{
public:
	using Callback = std::function<void(Sender*, Args*)>;

	explicit EventHandler(Callback Call) : m_Call(std::move(Call)) {}
	virtual ~EventHandler() = default;

	ULONG STDMETHODCALLTYPE AddRef() override { return ++m_iRefs; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		auto iRefs = --m_iRefs;

		if (iRefs == 0)
		{
			delete this;
		}

		return iRefs;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
	{
		if (!ppv)
		{
			return E_POINTER;
		}

		if (riid == IID_IUnknown || riid == __uuidof(Interface))
		{
			*ppv = static_cast<Interface*>(this);
			AddRef();
			return S_OK;
		}

		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Invoke(Sender* pSender, Args* pArgs) override
	{
		m_Call(pSender, pArgs);
		return S_OK;
	}

private:
	Callback           m_Call;
	std::atomic<ULONG> m_iRefs { 1 };

}; // EventHandler

//-----------------------------------------------------------------------------
// registers an event handler and returns its token
template<typename Interface, typename Sender, typename Args, typename Source, typename Adder>
EventRegistrationToken AddHandler(Source* pSource, Adder Add, std::function<void(Sender*, Args*)> Call)
//-----------------------------------------------------------------------------
{
	EventRegistrationToken Token {};
	auto pHandler = new EventHandler<Interface, Sender, Args>(std::move(Call));
	(pSource->*Add)(pHandler, &Token);
	// the source holds its own reference now
	pHandler->Release();
	return Token;
}

// a menu shortcut: the key with control, and shift for an uppercase key
struct Accelerator
{
	UINT iVirtualKey { 0 };
	bool bShift      { false };
	UINT iCommand    { 0 };
};

// the state of the one window this process has
struct Shell
{
	std::mutex                              Mutex;   // for the handlers, set from other threads
	HWND                                    hWindow  { nullptr };
	ICoreWebView2Controller*                pController { nullptr };
	ICoreWebView2*                          pWebView    { nullptr };
	std::vector<KString>                    MenuActions;
	std::vector<Accelerator>                Accelerators;
	std::function<void(KStringView)>        OnAction;
	std::function<void()>                   OnQuit;
	std::function<void()>                   OnActivate;
	std::function<void(const WindowFrame&)> OnFrame;
	std::function<void(KStringView)>        OnNotificationClick;
	NavigationPolicy                        Policy;
	KString                                 sLastNotificationTag;
	std::atomic<bool>                       bHideOnClose { false };
	bool                                    bSubclassed  { false };
	bool                                    bTray        { false };
	NOTIFYICONDATAW                         Tray {};
};

Shell s_Shell;

//-----------------------------------------------------------------------------
template<typename Handler>
Handler CopyHandler(const Handler& Source)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
	return Source;
}

//-----------------------------------------------------------------------------
void ReportFrame(HWND hWnd)
//-----------------------------------------------------------------------------
{
	auto OnFrame = CopyHandler(s_Shell.OnFrame);

	WindowFrame Frame;

	if (OnFrame && GetWindowFrame(hWnd, Frame))
	{
		OnFrame(Frame);
	}

} // ReportFrame

//-----------------------------------------------------------------------------
void RemoveTray()
//-----------------------------------------------------------------------------
{
	if (s_Shell.bTray)
	{
		::Shell_NotifyIconW(NIM_DELETE, &s_Shell.Tray);
		s_Shell.bTray = false;
	}

} // RemoveTray

//-----------------------------------------------------------------------------
// the tray icon: the window's icon, its title as tip
bool EnsureTray()
//-----------------------------------------------------------------------------
{
	if (s_Shell.bTray)
	{
		return true;
	}

	if (!s_Shell.hWindow)
	{
		return false;
	}

	auto& Tray = s_Shell.Tray;
	Tray = NOTIFYICONDATAW {};
	Tray.cbSize           = sizeof(Tray);
	Tray.hWnd             = s_Shell.hWindow;
	Tray.uID              = 1;
	Tray.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	Tray.uCallbackMessage = WM_APP_TRAY;
	Tray.hIcon            = reinterpret_cast<HICON>(::SendMessageW(s_Shell.hWindow, WM_GETICON, ICON_SMALL, 0));

	if (!Tray.hIcon)
	{
		Tray.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
	}

	::GetWindowTextW(s_Shell.hWindow, Tray.szTip, static_cast<int>(sizeof(Tray.szTip) / sizeof(Tray.szTip[0])));

	if (!::Shell_NotifyIconW(NIM_ADD, &Tray))
	{
		return false;
	}

	Tray.uVersion = NOTIFYICON_VERSION_4;
	::Shell_NotifyIconW(NIM_SETVERSION, &Tray);
	s_Shell.bTray = true;
	return true;

} // EnsureTray

//-----------------------------------------------------------------------------
// the window procedure in front of webview/webview's
LRESULT CALLBACK SubclassProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
//-----------------------------------------------------------------------------
{
	switch (iMessage)
	{
		case WM_COMMAND:
		{
			auto iCommand = LOWORD(wParam);

			if (iCommand == ID_MENU_QUIT)
			{
				if (auto OnQuit = CopyHandler(s_Shell.OnQuit))
				{
					OnQuit();
				}

				return 0;
			}

			if (iCommand >= ID_MENU_BASE)
			{
				KString                          sAction;
				std::function<void(KStringView)> OnAction;
				{
					std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
					auto iIndex = static_cast<std::size_t>(iCommand - ID_MENU_BASE);

					if (iIndex < s_Shell.MenuActions.size())
					{
						sAction  = s_Shell.MenuActions[iIndex];
						OnAction = s_Shell.OnAction;
					}
				}

				if (OnAction && !sAction.empty())
				{
					OnAction(sAction);
				}

				return 0;
			}

			break;
		}

		case WM_CLOSE:
			ReportFrame(hWnd);

			if (s_Shell.bHideOnClose)
			{
				// the tray icon is the way back
				EnsureTray();
				::ShowWindow(hWnd, SW_HIDE);
				return 0;
			}

			break;

		case WM_EXITSIZEMOVE:
			ReportFrame(hWnd);
			break;

		case WM_SIZE:
			if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
			{
				ReportFrame(hWnd);
			}
			break;

		case WM_ACTIVATE:
			if (LOWORD(wParam) != WA_INACTIVE)
			{
				if (auto OnActivate = CopyHandler(s_Shell.OnActivate))
				{
					OnActivate();
				}
			}
			break;

		case WM_APP_TRAY:
			switch (LOWORD(lParam))
			{
				case WM_LBUTTONUP:
				case NIN_SELECT:
				case NIN_KEYSELECT:
					if (auto OnActivate = CopyHandler(s_Shell.OnActivate))
					{
						OnActivate();
					}
					else
					{
						ActivateWindow(hWnd);
					}
					break;

				case NIN_BALLOONUSERCLICK:
					if (auto OnClick = CopyHandler(s_Shell.OnNotificationClick))
					{
						OnClick(CopyHandler(s_Shell.sLastNotificationTag));
					}
					break;
			}
			return 0;

		case WM_DESTROY:
			RemoveTray();
			::RemoveWindowSubclass(hWnd, SubclassProc, SUBCLASS_ID);
			s_Shell.bSubclassed = false;
			s_Shell.hWindow     = nullptr;
			break;
	}

	return ::DefSubclassProc(hWnd, iMessage, wParam, lParam);

} // SubclassProc

//-----------------------------------------------------------------------------
// remembers the window and puts our procedure in front of its own, once
bool Attach(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return false;
	}

	auto hWnd = static_cast<HWND>(pWindow);
	s_Shell.hWindow = hWnd;

	if (!s_Shell.bSubclassed)
	{
		s_Shell.bSubclassed = ::SetWindowSubclass(hWnd, SubclassProc, SUBCLASS_ID, 0) != FALSE;
	}

	return s_Shell.bSubclassed;

} // Attach

//-----------------------------------------------------------------------------
// COM for the dialogs - webview/webview initialized the apartment already,
// the second call only has to be balanced
class ComScope
//-----------------------------------------------------------------------------
{
public:
	ComScope()  { m_Result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
	~ComScope() { if (SUCCEEDED(m_Result)) ::CoUninitialize(); }

private:
	HRESULT m_Result;
};

//-----------------------------------------------------------------------------
// the file types of a dialog: one entry with all extensions
std::wstring FilterSpec(const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	std::wstring sSpec;

	for (const auto& sExtension : Extensions)
	{
		if (!sSpec.empty())
		{
			sSpec += L';';
		}

		sSpec += L"*.";
		sSpec += kutf::Convert<std::wstring>(sExtension);
	}

	return sSpec;
}

//-----------------------------------------------------------------------------
KString ItemPath(IShellItem* pItem)
//-----------------------------------------------------------------------------
{
	CoString sPath;

	if (pItem && SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &sPath)))
	{
		return sPath.str();
	}

	return {};
}

//-----------------------------------------------------------------------------
// the shortcut for a menu key: a letter, uppercase means shift
bool ParseKey(KStringView sKey, Accelerator& Accel, std::wstring& sLabel)
//-----------------------------------------------------------------------------
{
	if (sKey.size() != 1 || !KASCII::kIsAlNum(sKey.front()))
	{
		return false;
	}

	auto ch = sKey.front();
	Accel.bShift      = KASCII::kIsUpper(ch);
	Accel.iVirtualKey = static_cast<UINT>(KASCII::kToUpper(ch)); // virtual key codes of letters and digits are their ASCII codes

	sLabel  = Accel.bShift ? L"\tCtrl+Shift+" : L"\tCtrl+";
	sLabel += static_cast<wchar_t>(KASCII::kToUpper(ch));
	return true;
}

// a top level window of another process, by its class - the one webview/webview registers
struct WindowSearch
{
	DWORD iPID  { 0 };
	HWND  hWnd  { nullptr };
};

//-----------------------------------------------------------------------------
BOOL CALLBACK FindProcessWindow(HWND hWnd, LPARAM lParam)
//-----------------------------------------------------------------------------
{
	auto& Search = *reinterpret_cast<WindowSearch*>(lParam);
	DWORD iPID { 0 };
	::GetWindowThreadProcessId(hWnd, &iPID);

	if (iPID == Search.iPID)
	{
		wchar_t sClass[32] {};
		::GetClassNameW(hWnd, sClass, 31);

		if (std::wstring(sClass) == L"webview")
		{
			Search.hWnd = hWnd;
			return FALSE;
		}
	}

	return TRUE;
}

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
	return reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr, L"open", kutf::Convert<std::wstring>(sURL).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;

} // OpenExternal

//-----------------------------------------------------------------------------
bool Notify(KStringView sTitle, KStringView sBody, KStringView sTag, KStringView /*sImagePath*/)
//-----------------------------------------------------------------------------
{
	// a balloon on the tray icon - no picture, and a tag only for the click report
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.sLastNotificationTag = sTag;
	}

	if (!EnsureTray())
	{
		kDebug(1, "no window for a tray icon, dropping notification: {} - {}", sTitle, sBody);
		return false;
	}

	auto Tray        = s_Shell.Tray;
	Tray.uFlags      = NIF_INFO;
	Tray.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
	::wcsncpy_s(Tray.szInfo,      kutf::Convert<std::wstring>(sBody).c_str(),  _TRUNCATE);
	::wcsncpy_s(Tray.szInfoTitle, kutf::Convert<std::wstring>(sTitle).c_str(), _TRUNCATE);

	return ::Shell_NotifyIconW(NIM_MODIFY, &Tray) != FALSE;

} // Notify

//-----------------------------------------------------------------------------
bool WatchNotifications(std::function<void(KStringView)> OnClick)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
	s_Shell.OnNotificationClick = std::move(OnClick);
	return s_Shell.hWindow != nullptr;

} // WatchNotifications

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* pWindow, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Files;

	ComScope Com;
	ComPtr<IFileOpenDialog> Dialog;

	if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))))
	{
		kDebug(1, "cannot create the open dialog");
		return Files;
	}

	DWORD iOptions { 0 };
	Dialog->GetOptions(&iOptions);
	iOptions |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
	if (bMultiple)    iOptions |= FOS_ALLOWMULTISELECT;
	if (bDirectories) iOptions |= FOS_PICKFOLDERS;
	Dialog->SetOptions(iOptions);

	if (!sTitle.empty())
	{
		Dialog->SetTitle(kutf::Convert<std::wstring>(sTitle).c_str());
	}

	auto sSpec = FilterSpec(Extensions);

	if (!sSpec.empty() && !bDirectories)
	{
		COMDLG_FILTERSPEC Spec[] = { { L"Files", sSpec.c_str() } };
		Dialog->SetFileTypes(1, Spec);
	}

	if (FAILED(Dialog->Show(static_cast<HWND>(pWindow))))
	{
		// cancelled
		return Files;
	}

	ComPtr<IShellItemArray> Items;

	if (SUCCEEDED(Dialog->GetResults(&Items)))
	{
		DWORD iCount { 0 };
		Items->GetCount(&iCount);

		for (DWORD i = 0; i < iCount; ++i)
		{
			ComPtr<IShellItem> Item;

			if (SUCCEEDED(Items->GetItemAt(i, &Item)))
			{
				Files.push_back(ItemPath(Item.get()));
			}
		}
	}

	return Files;

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* pWindow, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	ComScope Com;
	ComPtr<IFileSaveDialog> Dialog;

	if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))))
	{
		kDebug(1, "cannot create the save dialog");
		return {};
	}

	DWORD iOptions { 0 };
	Dialog->GetOptions(&iOptions);
	Dialog->SetOptions(iOptions | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);

	if (!sTitle.empty())
	{
		Dialog->SetTitle(kutf::Convert<std::wstring>(sTitle).c_str());
	}

	if (!sSuggestedName.empty())
	{
		Dialog->SetFileName(kutf::Convert<std::wstring>(sSuggestedName).c_str());
	}

	auto sSpec = FilterSpec(Extensions);

	if (!sSpec.empty())
	{
		COMDLG_FILTERSPEC Spec[] = { { L"Files", sSpec.c_str() } };
		Dialog->SetFileTypes(1, Spec);
		Dialog->SetDefaultExtension(kutf::Convert<std::wstring>(Extensions.front()).c_str());
	}

	if (FAILED(Dialog->Show(static_cast<HWND>(pWindow))))
	{
		return {};
	}

	ComPtr<IShellItem> Item;

	if (FAILED(Dialog->GetResult(&Item)))
	{
		return {};
	}

	return ItemPath(Item.get());

} // SaveFileDialog

//-----------------------------------------------------------------------------
void SetMenu(void* pWindow, KStringView sAppName, const KJSON& jMenus, std::function<void(KStringView)> OnAction, std::function<void()> OnQuit)
//-----------------------------------------------------------------------------
{
	if (!Attach(pWindow))
	{
		return;
	}

	auto hWnd = static_cast<HWND>(pWindow);

	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.MenuActions.clear();
		s_Shell.Accelerators.clear();
		s_Shell.OnAction = std::move(OnAction);
		s_Shell.OnQuit   = std::move(OnQuit);
	}

	auto hBar = ::CreateMenu();

	if (jMenus.is_array())
	{
		for (const auto& jMenu : jMenus)
		{
			auto hPopup = ::CreatePopupMenu();

			for (const auto& jItem : jMenu["items"])
			{
				if (jItem["separator"].Bool())
				{
					::AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
					continue;
				}

				UINT iCommand { 0 };
				{
					std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
					s_Shell.MenuActions.push_back(jItem["action"].String());
					iCommand = ID_MENU_BASE + static_cast<UINT>(s_Shell.MenuActions.size() - 1);
				}

				auto         sLabel = kutf::Convert<std::wstring>(jItem["title"].String());
				Accelerator  Accel;
				std::wstring sKeyLabel;

				if (ParseKey(jItem["key"].String(), Accel, sKeyLabel))
				{
					Accel.iCommand = iCommand;
					sLabel += sKeyLabel;
					std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
					s_Shell.Accelerators.push_back(Accel);
				}

				::AppendMenuW(hPopup, MF_STRING, iCommand, sLabel.c_str());
			}

			::AppendMenuW(hBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hPopup), kutf::Convert<std::wstring>(jMenu["title"].String()).c_str());
		}
	}

	// the application's own menu: the quit entry, as the last one
	{
		auto hPopup = ::CreatePopupMenu();
		::AppendMenuW(hPopup, MF_STRING, ID_MENU_QUIT, kutf::Convert<std::wstring>(kFormat("Quit {}\tCtrl+Q", sAppName)).c_str());
		::AppendMenuW(hBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hPopup), kutf::Convert<std::wstring>(sAppName).c_str());

		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.Accelerators.push_back(Accelerator { 'Q', false, ID_MENU_QUIT });
	}

	auto hOld = ::GetMenu(hWnd);
	::SetMenu(hWnd, hBar);

	if (hOld)
	{
		::DestroyMenu(hOld);
	}

	// the client area shrank by the menu bar - the view has to follow
	RECT Client {};
	::GetClientRect(hWnd, &Client);
	::SendMessageW(hWnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(Client.right - Client.left, Client.bottom - Client.top));
	::DrawMenuBar(hWnd);

} // SetMenu

//-----------------------------------------------------------------------------
bool GetWindowFrame(void* pWindow, WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	auto hWnd = static_cast<HWND>(pWindow);

	if (!hWnd || ::IsIconic(hWnd))
	{
		return false;
	}

	RECT Rect {};

	if (!::GetWindowRect(hWnd, &Rect))
	{
		return false;
	}

	Frame.iX      = Rect.left;
	Frame.iY      = Rect.top;
	Frame.iWidth  = Rect.right  - Rect.left;
	Frame.iHeight = Rect.bottom - Rect.top;
	return Frame.iWidth > 0 && Frame.iHeight > 0;

} // GetWindowFrame

//-----------------------------------------------------------------------------
void SetWindowFrame(void* pWindow, const WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	if (pWindow && Frame.iWidth > 0 && Frame.iHeight > 0)
	{
		::SetWindowPos(static_cast<HWND>(pWindow), nullptr, Frame.iX, Frame.iY, Frame.iWidth, Frame.iHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	}

} // SetWindowFrame

//-----------------------------------------------------------------------------
void WatchWindowFrame(void* pWindow, std::function<void(const WindowFrame&)> OnChange)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.OnFrame = std::move(OnChange);
	}

	Attach(pWindow);

} // WatchWindowFrame

//-----------------------------------------------------------------------------
void UnwatchWindowFrame()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
	s_Shell.OnFrame = nullptr;

} // UnwatchWindowFrame

//-----------------------------------------------------------------------------
bool ActivateProcess(int64_t iPID)
//-----------------------------------------------------------------------------
{
	WindowSearch Search;
	Search.iPID = static_cast<DWORD>(iPID);
	::EnumWindows(FindProcessWindow, reinterpret_cast<LPARAM>(&Search));

	if (!Search.hWnd)
	{
		return false;
	}

	// a window hidden into the tray comes back as well
	::ShowWindow(Search.hWnd, ::IsIconic(Search.hWnd) ? SW_RESTORE : SW_SHOW);
	return ::SetForegroundWindow(Search.hWnd) != FALSE;

} // ActivateProcess

//-----------------------------------------------------------------------------
bool ActivateWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return false;
	}

	auto hWnd = static_cast<HWND>(pWindow);
	::ShowWindow(hWnd, ::IsIconic(hWnd) ? SW_RESTORE : SW_SHOW);
	return ::SetForegroundWindow(hWnd) != FALSE;

} // ActivateWindow

//-----------------------------------------------------------------------------
bool AllowMediaCapture(void* pController)
//-----------------------------------------------------------------------------
{
	if (!pController)
	{
		return false;
	}

	ComPtr<ICoreWebView2> WebView;

	if (FAILED(static_cast<ICoreWebView2Controller*>(pController)->get_CoreWebView2(&WebView)) || !WebView)
	{
		return false;
	}

	AddHandler<ICoreWebView2PermissionRequestedEventHandler, ICoreWebView2, ICoreWebView2PermissionRequestedEventArgs>(
		WebView.get(), &ICoreWebView2::add_PermissionRequested,
		[](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* pArgs)
		{
			COREWEBVIEW2_PERMISSION_KIND Kind {};
			pArgs->get_PermissionKind(&Kind);

			if (Kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA || Kind == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE)
			{
				pArgs->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
			}
		});

	return true;

} // AllowMediaCapture

//-----------------------------------------------------------------------------
void SetBadge(KStringView sText)
//-----------------------------------------------------------------------------
{
	// ITaskbarList3::SetOverlayIcon wants an icon drawn with the text - later
	kDebug(2, "no badge on this platform yet: '{}'", sText);

} // SetBadge

//-----------------------------------------------------------------------------
int64_t RequestAttention(void* pWindow, bool bCritical)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return 0;
	}

	// the taskbar button flashes - critical until the window comes to the front
	FLASHWINFO Info {};
	Info.cbSize    = sizeof(Info);
	Info.hwnd      = static_cast<HWND>(pWindow);
	Info.dwFlags   = FLASHW_TRAY | (bCritical ? FLASHW_TIMERNOFG : static_cast<DWORD>(0));
	Info.uCount    = bCritical ? 0 : 3;
	Info.dwTimeout = 0;
	::FlashWindowEx(&Info);
	return 1;

} // RequestAttention

//-----------------------------------------------------------------------------
void CancelAttention(void* pWindow, int64_t /*iRequest*/)
//-----------------------------------------------------------------------------
{
	if (!pWindow)
	{
		return;
	}

	FLASHWINFO Info {};
	Info.cbSize  = sizeof(Info);
	Info.hwnd    = static_cast<HWND>(pWindow);
	Info.dwFlags = FLASHW_STOP;
	::FlashWindowEx(&Info);

} // CancelAttention

//-----------------------------------------------------------------------------
bool SetNavigationPolicy(void* pController, NavigationPolicy Policy)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.Policy = std::move(Policy);
	}

	if (!pController)
	{
		return false;
	}

	auto pCtl = static_cast<ICoreWebView2Controller*>(pController);
	ICoreWebView2* pWebView { nullptr };

	if (FAILED(pCtl->get_CoreWebView2(&pWebView)) || !pWebView)
	{
		return false;
	}

	// kept for ClearWebCache() and released with the policy
	pCtl->AddRef();
	s_Shell.pController = pCtl;
	s_Shell.pWebView    = pWebView;

	auto CurrentPolicy = []
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		return s_Shell.Policy;
	};

	// top level navigations
	AddHandler<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2, ICoreWebView2NavigationStartingEventArgs>(
		pWebView, &ICoreWebView2::add_NavigationStarting,
		[CurrentPolicy](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* pArgs)
		{
			CoString sURI;
			pArgs->get_Uri(&sURI);
			auto P = CurrentPolicy();

			if (P.OnNavigate && !P.OnNavigate(sURI.str(), /*bNewWindow*/ false))
			{
				pArgs->put_Cancel(TRUE);
			}
		});

	// window.open() and target="_blank"
	AddHandler<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2, ICoreWebView2NewWindowRequestedEventArgs>(
		pWebView, &ICoreWebView2::add_NewWindowRequested,
		[CurrentPolicy](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* pArgs)
		{
			CoString sURI;
			pArgs->get_Uri(&sURI);
			auto P = CurrentPolicy();

			if (P.OnNavigate)
			{
				P.OnNavigate(sURI.str(), /*bNewWindow*/ true);
			}

			// no second window, whatever the shell did with it
			pArgs->put_Handled(TRUE);
		});

	// downloads: WebView2 decides itself what is one, we say where it goes
#ifdef __ICoreWebView2_4_INTERFACE_DEFINED__
	{
		ComPtr<ICoreWebView2_4> WebView4;

		if (SUCCEEDED(pWebView->QueryInterface(IID_PPV_ARGS(&WebView4))) && WebView4)
		{
			AddHandler<ICoreWebView2DownloadStartingEventHandler, ICoreWebView2, ICoreWebView2DownloadStartingEventArgs>(
				WebView4.get(), &ICoreWebView2_4::add_DownloadStarting,
				[CurrentPolicy](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* pArgs)
				{
					ComPtr<ICoreWebView2DownloadOperation> Operation;
					pArgs->get_DownloadOperation(&Operation);

					// the name WebView2 suggests is the last part of its default path
					CoString sDefaultPath;
					if (Operation) Operation->get_ResultFilePath(&sDefaultPath);
					auto sDefault = sDefaultPath.str();
					auto iSlash   = sDefault.find_last_of("\\/");
					auto sName    = iSlash == KString::npos ? sDefault : KString(sDefault.substr(iSlash + 1));

					auto    P = CurrentPolicy();
					KString sPath;

					if (P.DownloadPath)
					{
						sPath = P.DownloadPath(sName);
					}

					// no default download dialog
					pArgs->put_Handled(TRUE);

					if (sPath.empty() || !Operation)
					{
						kDebug(1, "no destination for download '{}', cancelling it", sName);
						pArgs->put_Cancel(TRUE);
						return;
					}

					pArgs->put_ResultFilePath(kutf::Convert<std::wstring>(sPath).c_str());

					// the end of the download, good or bad
					auto pOperation = Operation.get();
					pOperation->AddRef();

					AddHandler<ICoreWebView2StateChangedEventHandler, ICoreWebView2DownloadOperation, IUnknown>(
						pOperation, &ICoreWebView2DownloadOperation::add_StateChanged,
						[CurrentPolicy, sPath, pOperation](ICoreWebView2DownloadOperation* pSender, IUnknown*)
						{
							COREWEBVIEW2_DOWNLOAD_STATE State {};
							pSender->get_State(&State);

							if (State == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS)
							{
								return;
							}

							auto P = CurrentPolicy();

							if (P.OnDownload)
							{
								if (State == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED)
								{
									P.OnDownload(sPath, KStringView{});
								}
								else
								{
									COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON Reason {};
									pSender->get_InterruptReason(&Reason);
									P.OnDownload(sPath, kFormat("download interrupted, reason {}", static_cast<int>(Reason)));
								}
							}

							pOperation->Release();
						});
				});
		}
	}
#else
	kDebug(1, "the WebView2 SDK is too old for download handling (ICoreWebView2_4), downloads use the default dialog");
#endif

	// the menu shortcuts: the view has the keyboard, and hands them over
	AddHandler<ICoreWebView2AcceleratorKeyPressedEventHandler, ICoreWebView2Controller, ICoreWebView2AcceleratorKeyPressedEventArgs>(
		pCtl, &ICoreWebView2Controller::add_AcceleratorKeyPressed,
		[](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* pArgs)
		{
			COREWEBVIEW2_KEY_EVENT_KIND Kind {};
			pArgs->get_KeyEventKind(&Kind);

			if (Kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN && Kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)
			{
				return;
			}

			if ((::GetKeyState(VK_CONTROL) & 0x8000) == 0)
			{
				return;
			}

			UINT iKey { 0 };
			pArgs->get_VirtualKey(&iKey);
			bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

			std::vector<Accelerator> Accelerators;
			HWND                     hWnd;
			{
				std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
				Accelerators = s_Shell.Accelerators;
				hWnd         = s_Shell.hWindow;
			}

			for (const auto& Accel : Accelerators)
			{
				if (Accel.iVirtualKey == iKey && Accel.bShift == bShift)
				{
					pArgs->put_Handled(TRUE);

					if (hWnd)
					{
						::PostMessageW(hWnd, WM_COMMAND, Accel.iCommand, 0);
					}

					return;
				}
			}
		});

	return true;

} // SetNavigationPolicy

//-----------------------------------------------------------------------------
void ClearNavigationPolicy()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.Policy = NavigationPolicy{};
	}

	if (s_Shell.pWebView)
	{
		s_Shell.pWebView->Release();
		s_Shell.pWebView = nullptr;
	}

	if (s_Shell.pController)
	{
		s_Shell.pController->Release();
		s_Shell.pController = nullptr;
	}

} // ClearNavigationPolicy

//-----------------------------------------------------------------------------
bool SetHideOnClose(void* pWindow, bool bHide)
//-----------------------------------------------------------------------------
{
	s_Shell.bHideOnClose = bHide;
	return Attach(pWindow);

} // SetHideOnClose

//-----------------------------------------------------------------------------
void HideWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (pWindow)
	{
		// the tray icon is the way back
		if (Attach(pWindow))
		{
			EnsureTray();
		}

		::ShowWindow(static_cast<HWND>(pWindow), SW_HIDE);
	}

} // HideWindow

//-----------------------------------------------------------------------------
bool IsWindowVisible(void* pWindow)
//-----------------------------------------------------------------------------
{
	return pWindow && ::IsWindowVisible(static_cast<HWND>(pWindow)) != FALSE;

} // IsWindowVisible

//-----------------------------------------------------------------------------
void WatchApplication(void* pWindow, std::function<void()> OnActivate)
//-----------------------------------------------------------------------------
{
	bool bWatch = OnActivate != nullptr;
	{
		std::lock_guard<std::mutex> Lock(s_Shell.Mutex);
		s_Shell.OnActivate = std::move(OnActivate);
	}

	if (bWatch)
	{
		Attach(pWindow);
	}
	else
	{
		// the window goes away: no tray icon without it
		RemoveTray();
	}

} // WatchApplication

namespace {

//-----------------------------------------------------------------------------
// the credential's name: the application and the key
std::wstring TargetName(KStringView sService, KStringView sKey)
//-----------------------------------------------------------------------------
{
	return kutf::Convert<std::wstring>(kFormat("{}/{}", sService, sKey));
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool SaveSecret(KStringView sService, KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	auto sTarget = TargetName(sService, sKey);
	auto sUser   = kutf::Convert<std::wstring>(sKey);

	CREDENTIALW Credential {};
	Credential.Type               = CRED_TYPE_GENERIC;
	Credential.TargetName         = sTarget.data();
	Credential.UserName           = sUser.data();
	Credential.CredentialBlobSize = static_cast<DWORD>(sValue.size());
	Credential.CredentialBlob     = reinterpret_cast<LPBYTE>(const_cast<char*>(sValue.data()));
	Credential.Persist            = CRED_PERSIST_LOCAL_MACHINE;

	if (!::CredWriteW(&Credential, 0))
	{
		kDebug(1, "cannot store secret '{}' for {}: error {}", sKey, sService, ::GetLastError());
		return false;
	}

	return true;

} // SaveSecret

//-----------------------------------------------------------------------------
bool LoadSecret(KStringView sService, KStringView sKey, KString& sValue)
//-----------------------------------------------------------------------------
{
	auto sTarget = TargetName(sService, sKey);

	PCREDENTIALW pCredential = nullptr;

	if (!::CredReadW(sTarget.data(), CRED_TYPE_GENERIC, 0, &pCredential))
	{
		return false;
	}

	sValue.assign(reinterpret_cast<const char*>(pCredential->CredentialBlob), pCredential->CredentialBlobSize);
	::CredFree(pCredential);
	return true;

} // LoadSecret

//-----------------------------------------------------------------------------
bool DeleteSecret(KStringView sService, KStringView sKey)
//-----------------------------------------------------------------------------
{
	auto sTarget = TargetName(sService, sKey);

	if (!::CredDeleteW(sTarget.data(), CRED_TYPE_GENERIC, 0))
	{
		return ::GetLastError() == ERROR_NOT_FOUND;
	}

	return true;

} // DeleteSecret

#ifdef __ICoreWebView2Profile2_INTERFACE_DEFINED__

namespace {

//-----------------------------------------------------------------------------
// the completion of ClearBrowsingDataAsync - one argument, unlike the events
class ClearCompletedHandler : public ICoreWebView2ClearBrowsingDataCompletedHandler
//-----------------------------------------------------------------------------
{
public:
	explicit ClearCompletedHandler(std::function<void()> Done) : m_Done(std::move(Done)) {}
	virtual ~ClearCompletedHandler() = default;

	ULONG STDMETHODCALLTYPE AddRef() override { return ++m_iRefs; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		auto iRefs = --m_iRefs;

		if (iRefs == 0)
		{
			delete this;
		}

		return iRefs;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
	{
		if (!ppv)
		{
			return E_POINTER;
		}

		if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2ClearBrowsingDataCompletedHandler))
		{
			*ppv = static_cast<ICoreWebView2ClearBrowsingDataCompletedHandler*>(this);
			AddRef();
			return S_OK;
		}

		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Invoke(HRESULT) override
	{
		kDebug(2, "web caches cleared");

		if (m_Done)
		{
			m_Done();
		}

		return S_OK;
	}

private:
	std::function<void()> m_Done;
	std::atomic<ULONG>    m_iRefs { 1 };

}; // ClearCompletedHandler

} // end of anonymous namespace

#endif // __ICoreWebView2Profile2_INTERFACE_DEFINED__

//-----------------------------------------------------------------------------
void ClearWebCache(std::function<void()> Done)
//-----------------------------------------------------------------------------
{
#ifdef __ICoreWebView2Profile2_INTERFACE_DEFINED__
	// the caches only - local storage and databases hold user data. The view
	// is the one SetNavigationPolicy() saw
	if (s_Shell.pWebView)
	{
		ComPtr<ICoreWebView2_13> WebView13;
		ComPtr<ICoreWebView2Profile> Profile;
		ComPtr<ICoreWebView2Profile2> Profile2;

		if (SUCCEEDED(s_Shell.pWebView->QueryInterface(IID_PPV_ARGS(&WebView13))) && WebView13
			&& SUCCEEDED(WebView13->get_Profile(&Profile)) && Profile
			&& SUCCEEDED(Profile->QueryInterface(IID_PPV_ARGS(&Profile2))) && Profile2)
		{
			auto Kinds    = static_cast<COREWEBVIEW2_BROWSING_DATA_KINDS>(COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE
			                                                             | COREWEBVIEW2_BROWSING_DATA_KINDS_CACHE_STORAGE
			                                                             | COREWEBVIEW2_BROWSING_DATA_KINDS_SERVICE_WORKERS);
			auto pHandler = new ClearCompletedHandler(std::move(Done));

			if (SUCCEEDED(Profile2->ClearBrowsingData(Kinds, pHandler)))
			{
				pHandler->Release();
				return;
			}

			pHandler->Release();
		}
	}

	kDebug(1, "cannot clear the web caches: no view, or the WebView2 runtime is too old");
#else
	kDebug(1, "the WebView2 SDK is too old to clear the web caches (ICoreWebView2Profile2)");
#endif

	if (Done)
	{
		Done();
	}

} // ClearWebCache

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW && DEKAF2_IS_WINDOWS
