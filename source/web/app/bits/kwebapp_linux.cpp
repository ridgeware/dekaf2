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

// The GTK platforms, with WebKitGTK: GTK 4 with the webkitgtk-6.0 API, or
// GTK 3 with the webkit2gtk-4.1 (or -4.0) API. The header the build found
// decides, through GTK_MAJOR_VERSION, like in webview/webview itself. What the
// desktop offers no API for (the browser, notifications, the secret service)
// goes through the standard command line tools.

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/system/os/ksystem.h>
#include <gtk/gtk.h>
#if GTK_MAJOR_VERSION >= 4
	#include <webkit/webkit.h>
#else
	#include <webkit2/webkit2.h>
#endif
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

DEKAF2_NAMESPACE_BEGIN

namespace kwebapp {

namespace {

// static initialization runs on the thread that later runs the UI loop
const std::thread::id s_MainThread = std::this_thread::get_id();

// what the callbacks of GTK and WebKit reach for - set from the UI thread,
// read from the UI thread, the mutex is for the handlers KWebApp installs
// and clears from other threads
std::mutex                       s_Mutex;
std::vector<KString>             s_MenuActions;
std::function<void(KStringView)> s_OnAction;
std::function<void()>            s_OnQuit;
std::function<void(const WindowFrame&)> s_OnFrame;
NavigationPolicy                 s_Policy;
std::atomic<bool>                s_bHideOnClose { false };
GtkWidget*                       s_WatchedWindow { nullptr };

//-----------------------------------------------------------------------------
GtkWindow* Window(void* pWindow)
//-----------------------------------------------------------------------------
{
	return pWindow ? GTK_WINDOW(pWindow) : nullptr;
}

//-----------------------------------------------------------------------------
// a GTK string, freed when the scope ends
class GString
//-----------------------------------------------------------------------------
{
public:
	explicit GString(gchar* sText = nullptr) : m_sText(sText) {}
	~GString() { g_free(m_sText); }
	GString(const GString&) = delete;
	GString& operator=(const GString&) = delete;
	operator KStringView() const { return m_sText ? KStringView(m_sText) : KStringView{}; }
	KString  str()          const { return KString(operator KStringView()); }

private:
	gchar* m_sText;
};

//-----------------------------------------------------------------------------
// the shell is not async: run the GTK main loop until the dialog answered
class NestedLoop
//-----------------------------------------------------------------------------
{
public:
	NestedLoop()  : m_Loop(g_main_loop_new(nullptr, FALSE)) {}
	~NestedLoop() { g_main_loop_unref(m_Loop); }
	NestedLoop(const NestedLoop&) = delete;
	NestedLoop& operator=(const NestedLoop&) = delete;
	void Run()  { g_main_loop_run(m_Loop);  }
	void Quit() { g_main_loop_quit(m_Loop); }

private:
	GMainLoop* m_Loop;
};

//-----------------------------------------------------------------------------
// GTK's accelerator name for a menu key: a letter, uppercase means shift
KString AcceleratorName(KStringView sKey)
//-----------------------------------------------------------------------------
{
	if (sKey.empty())
	{
		return {};
	}

	KString sName = "<Control>";

	if (sKey.size() == 1 && KASCII::kIsUpper(sKey.front()))
	{
		sName += "<Shift>";
		sName += KASCII::kToLower(sKey.front());
	}
	else
	{
		sName += sKey;
	}

	return sName;

} // AcceleratorName

//-----------------------------------------------------------------------------
// a menu entry was activated: the action's name is "menu<index>"
void OnMenuAction(GSimpleAction* Action, GVariant*, gpointer)
//-----------------------------------------------------------------------------
{
	KStringView sName = g_action_get_name(G_ACTION(Action));
	sName.remove_prefix("menu");
	auto iIndex = sName.UInt64();

	KString                          sAction;
	std::function<void(KStringView)> OnAction;
	{
		std::lock_guard<std::mutex> Lock(s_Mutex);

		if (iIndex < s_MenuActions.size())
		{
			sAction  = s_MenuActions[iIndex];
			OnAction = s_OnAction;
		}
	}

	if (OnAction && !sAction.empty())
	{
		OnAction(sAction);
	}

} // OnMenuAction

//-----------------------------------------------------------------------------
void OnQuitAction(GSimpleAction*, GVariant*, gpointer)
//-----------------------------------------------------------------------------
{
	std::function<void()> OnQuit;
	{
		std::lock_guard<std::mutex> Lock(s_Mutex);
		OnQuit = s_OnQuit;
	}

	if (OnQuit)
	{
		OnQuit();
	}

} // OnQuitAction

//-----------------------------------------------------------------------------
void ReportFrame(GtkWindow* Window)
//-----------------------------------------------------------------------------
{
	std::function<void(const WindowFrame&)> OnFrame;
	{
		std::lock_guard<std::mutex> Lock(s_Mutex);
		OnFrame = s_OnFrame;
	}

	WindowFrame Frame;

	if (Window && OnFrame && GetWindowFrame(Window, Frame))
	{
		OnFrame(Frame);
	}

} // ReportFrame

#if GTK_MAJOR_VERSION >= 4

//-----------------------------------------------------------------------------
void OnDefaultSizeChanged(GObject* Window, GParamSpec*, gpointer)
//-----------------------------------------------------------------------------
{
	ReportFrame(GTK_WINDOW(Window));
}

//-----------------------------------------------------------------------------
// close-request: the last frame, and the hiding instead of closing
gboolean OnCloseRequest(GtkWindow* Window, gpointer)
//-----------------------------------------------------------------------------
{
	ReportFrame(Window);

	if (s_bHideOnClose)
	{
		gtk_widget_set_visible(GTK_WIDGET(Window), FALSE);
		return TRUE;
	}

	return FALSE;

} // OnCloseRequest

#else

//-----------------------------------------------------------------------------
gboolean OnConfigure(GtkWidget* Window, GdkEvent*, gpointer)
//-----------------------------------------------------------------------------
{
	ReportFrame(GTK_WINDOW(Window));
	return FALSE;
}

//-----------------------------------------------------------------------------
// delete-event: the last frame, and the hiding instead of closing
gboolean OnDeleteEvent(GtkWidget* Window, GdkEvent*, gpointer)
//-----------------------------------------------------------------------------
{
	ReportFrame(GTK_WINDOW(Window));

	if (s_bHideOnClose)
	{
		gtk_widget_hide(Window);
		return TRUE;
	}

	return FALSE;

} // OnDeleteEvent

// the menu shortcuts: GTK 3 has no shortcut controller, the window sees the key first
std::vector<std::pair<KString, GAction*>> s_Accelerators;

//-----------------------------------------------------------------------------
gboolean OnKeyPress(GtkWidget*, GdkEventKey* Event, gpointer)
//-----------------------------------------------------------------------------
{
	for (const auto& Accelerator : s_Accelerators)
	{
		guint           iKey   { 0 };
		GdkModifierType Mods   { };
		gtk_accelerator_parse(Accelerator.first.c_str(), &iKey, &Mods);

		if (iKey != 0 && gdk_keyval_to_lower(Event->keyval) == gdk_keyval_to_lower(iKey)
			&& (Event->state & gtk_accelerator_get_default_mod_mask()) == Mods)
		{
			g_action_activate(Accelerator.second, nullptr);
			return TRUE;
		}
	}

	return FALSE;

} // OnKeyPress

#endif

//-----------------------------------------------------------------------------
NavigationPolicy Policy()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_Mutex);
	return s_Policy;
}

//-----------------------------------------------------------------------------
// decide-policy: top level navigations and new windows are asked for, what
// the view cannot show and what comes as attachment becomes a download
gboolean OnDecidePolicy(WebKitWebView*, WebKitPolicyDecision* Decision, WebKitPolicyDecisionType Type, gpointer)
//-----------------------------------------------------------------------------
{
	switch (Type)
	{
		case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
		case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
		{
			auto Action  = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(Decision));
			auto Request = webkit_navigation_action_get_request(Action);
			auto P       = Policy();

			bool bNewWindow = Type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION;

			if (P.OnNavigate && !P.OnNavigate(webkit_uri_request_get_uri(Request), bNewWindow))
			{
				webkit_policy_decision_ignore(Decision);
				return TRUE;
			}

			return FALSE;
		}

		case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
		{
			auto ResponseDecision = WEBKIT_RESPONSE_POLICY_DECISION(Decision);
			bool bDownload        = !webkit_response_policy_decision_is_mime_type_supported(ResponseDecision);

			if (!bDownload)
			{
				auto Headers = webkit_uri_response_get_http_headers(webkit_response_policy_decision_get_response(ResponseDecision));

				if (Headers)
				{
					KString sDisposition(soup_message_headers_get_one(Headers, "Content-Disposition"));
					bDownload = sDisposition.MakeLowerASCII().contains("attachment");
				}
			}

			if (bDownload)
			{
				webkit_policy_decision_download(Decision);
				return TRUE;
			}

			return FALSE;
		}
	}

	return FALSE;

} // OnDecidePolicy

//-----------------------------------------------------------------------------
// create: window.open() asks for a second view, which we do not have
GtkWidget* OnCreate(WebKitWebView*, WebKitNavigationAction* Action, gpointer)
//-----------------------------------------------------------------------------
{
	auto Request = webkit_navigation_action_get_request(Action);
	auto P       = Policy();

	if (Request && P.OnNavigate)
	{
		P.OnNavigate(webkit_uri_request_get_uri(Request), /*bNewWindow*/ true);
	}

	return nullptr;

} // OnCreate

//-----------------------------------------------------------------------------
// decide-destination: the file the download goes to
gboolean OnDecideDestination(WebKitDownload* Download, gchar* sSuggestedName, gpointer)
//-----------------------------------------------------------------------------
{
	auto    P = Policy();
	KString sPath;

	if (P.DownloadPath)
	{
		sPath = P.DownloadPath(sSuggestedName ? KStringView(sSuggestedName) : KStringView{});
	}

	if (sPath.empty())
	{
		kDebug(1, "no destination for download '{}', cancelling it", sSuggestedName ? sSuggestedName : "");
		webkit_download_cancel(Download);
		return TRUE;
	}

#if GTK_MAJOR_VERSION >= 4
	// the 6.0 API takes a path
	webkit_download_set_destination(Download, sPath.c_str());
#else
	// the 4.x API takes a URI
	GString sURI(g_filename_to_uri(sPath.c_str(), nullptr, nullptr));
	webkit_download_set_destination(Download, KString(sURI).c_str());
#endif

	return TRUE;

} // OnDecideDestination

//-----------------------------------------------------------------------------
KString DownloadDestination(WebKitDownload* Download)
//-----------------------------------------------------------------------------
{
	auto sDestination = webkit_download_get_destination(Download);

	if (!sDestination)
	{
		return {};
	}

#if GTK_MAJOR_VERSION >= 4
	return sDestination;
#else
	GString sPath(g_filename_from_uri(sDestination, nullptr, nullptr));
	return sPath.str();
#endif

} // DownloadDestination

//-----------------------------------------------------------------------------
void OnDownloadFinished(WebKitDownload* Download, gpointer)
//-----------------------------------------------------------------------------
{
	auto P = Policy();

	if (P.OnDownload)
	{
		P.OnDownload(DownloadDestination(Download), KStringView{});
	}

} // OnDownloadFinished

//-----------------------------------------------------------------------------
gboolean OnDownloadFailed(WebKitDownload* Download, GError* Error, gpointer)
//-----------------------------------------------------------------------------
{
	auto P = Policy();

	if (P.OnDownload)
	{
		P.OnDownload(DownloadDestination(Download), Error && Error->message ? KStringView(Error->message) : KStringView("download failed"));
	}

	// finished follows failed - it must not report a success then
	g_signal_handlers_disconnect_by_func(Download, reinterpret_cast<gpointer>(OnDownloadFinished), nullptr);
	return FALSE;

} // OnDownloadFailed

//-----------------------------------------------------------------------------
// download-started: from now on the download reports to us
void OnDownloadStarted(gpointer, WebKitDownload* Download, gpointer)
//-----------------------------------------------------------------------------
{
	g_signal_connect(Download, "decide-destination", G_CALLBACK(OnDecideDestination), nullptr);
	g_signal_connect(Download, "finished",           G_CALLBACK(OnDownloadFinished),  nullptr);
	g_signal_connect(Download, "failed",             G_CALLBACK(OnDownloadFailed),    nullptr);

} // OnDownloadStarted

//-----------------------------------------------------------------------------
// permission-request: camera and microphone are granted, the rest is left to WebKit
gboolean OnPermissionRequest(WebKitWebView*, WebKitPermissionRequest* Request, gpointer)
//-----------------------------------------------------------------------------
{
	if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(Request))
	{
		webkit_permission_request_allow(Request);
		return TRUE;
	}

	return FALSE;

} // OnPermissionRequest

//-----------------------------------------------------------------------------
// the filters of a file dialog, from the extensions
GtkFileFilter* FileFilter(const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	auto Filter = gtk_file_filter_new();

	for (const auto& sExtension : Extensions)
	{
		gtk_file_filter_add_pattern(Filter, kFormat("*.{}", sExtension).c_str());
	}

	return Filter;

} // FileFilter

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
bool Notify(KStringView sTitle, KStringView sBody, KStringView /*sTag*/, KStringView sImagePath)
//-----------------------------------------------------------------------------
{
	// the picture becomes the icon, a tag would need the notification id that
	// notify-send prints with --print-id
	KString sCommand = "notify-send";

	if (!sImagePath.empty())
	{
		sCommand += kFormat(" --icon={}", kEscapeForCommands(sImagePath));
	}

	sCommand += kFormat(" {} {}", kEscapeForCommands(sTitle), kEscapeForCommands(sBody));
	std::thread([sCommand] { kSystem(sCommand); }).detach();
	return true;

} // Notify

//-----------------------------------------------------------------------------
bool WatchNotifications(std::function<void(KStringView)> /*OnClick*/)
//-----------------------------------------------------------------------------
{
	// notify-send cannot report clicks, that needs libnotify with a main loop
	return false;

} // WatchNotifications

#if GTK_MAJOR_VERSION >= 4

#if GTK_CHECK_VERSION(4, 10, 0)

namespace {

// the answer of a GtkFileDialog: one file, several, or none
struct DialogAnswer
{
	NestedLoop           Loop;
	std::vector<KString> Files;
	bool                 bMultiple { false };
	bool                 bSave     { false };
	bool                 bFolders  { false };
};

//-----------------------------------------------------------------------------
void OnDialogDone(GObject* Source, GAsyncResult* Result, gpointer pAnswer)
//-----------------------------------------------------------------------------
{
	auto  Dialog  = GTK_FILE_DIALOG(Source);
	auto& Answer  = *static_cast<DialogAnswer*>(pAnswer);

	auto AddFile = [&Answer](GFile* File)
	{
		if (File)
		{
			GString sPath(g_file_get_path(File));
			Answer.Files.push_back(sPath.str());
			g_object_unref(File);
		}
	};

	auto AddFiles = [&AddFile](GListModel* Files)
	{
		if (Files)
		{
			auto iCount = g_list_model_get_n_items(Files);

			for (guint i = 0; i < iCount; ++i)
			{
				AddFile(G_FILE(g_list_model_get_item(Files, i)));
			}

			g_object_unref(Files);
		}
	};

	// a cancelled dialog is an error to GTK, and no file to us
	if (Answer.bSave)
	{
		AddFile(gtk_file_dialog_save_finish(Dialog, Result, nullptr));
	}
	else if (Answer.bFolders && Answer.bMultiple)
	{
		AddFiles(gtk_file_dialog_select_multiple_folders_finish(Dialog, Result, nullptr));
	}
	else if (Answer.bFolders)
	{
		AddFile(gtk_file_dialog_select_folder_finish(Dialog, Result, nullptr));
	}
	else if (Answer.bMultiple)
	{
		AddFiles(gtk_file_dialog_open_multiple_finish(Dialog, Result, nullptr));
	}
	else
	{
		AddFile(gtk_file_dialog_open_finish(Dialog, Result, nullptr));
	}

	Answer.Loop.Quit();

} // OnDialogDone

//-----------------------------------------------------------------------------
GtkFileDialog* NewFileDialog(KStringView sTitle, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	auto Dialog = gtk_file_dialog_new();

	if (!sTitle.empty())
	{
		gtk_file_dialog_set_title(Dialog, KString(sTitle).c_str());
	}

	if (!Extensions.empty())
	{
		auto Filter  = FileFilter(Extensions);
		auto Filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
		g_list_store_append(Filters, Filter);
		gtk_file_dialog_set_filters(Dialog, G_LIST_MODEL(Filters));
		gtk_file_dialog_set_default_filter(Dialog, Filter);
		g_object_unref(Filters);
		g_object_unref(Filter);
	}

	return Dialog;

} // NewFileDialog

} // end of anonymous namespace

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* pWindow, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories)
//-----------------------------------------------------------------------------
{
	DialogAnswer Answer;
	Answer.bMultiple = bMultiple;
	Answer.bFolders  = bDirectories;

	auto Dialog = NewFileDialog(sTitle, bDirectories ? std::vector<KString>{} : Extensions);

	if (bDirectories && bMultiple)
	{
		gtk_file_dialog_select_multiple_folders(Dialog, Window(pWindow), nullptr, OnDialogDone, &Answer);
	}
	else if (bDirectories)
	{
		gtk_file_dialog_select_folder(Dialog, Window(pWindow), nullptr, OnDialogDone, &Answer);
	}
	else if (bMultiple)
	{
		gtk_file_dialog_open_multiple(Dialog, Window(pWindow), nullptr, OnDialogDone, &Answer);
	}
	else
	{
		gtk_file_dialog_open(Dialog, Window(pWindow), nullptr, OnDialogDone, &Answer);
	}

	Answer.Loop.Run();
	g_object_unref(Dialog);

	return Answer.Files;

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* pWindow, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	DialogAnswer Answer;
	Answer.bSave = true;

	auto Dialog = NewFileDialog(sTitle, Extensions);

	if (!sSuggestedName.empty())
	{
		gtk_file_dialog_set_initial_name(Dialog, KString(sSuggestedName).c_str());
	}

	gtk_file_dialog_save(Dialog, Window(pWindow), nullptr, OnDialogDone, &Answer);
	Answer.Loop.Run();
	g_object_unref(Dialog);

	return Answer.Files.empty() ? KString{} : Answer.Files.front();

} // SaveFileDialog

#else // GTK 4 before 4.10

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* /*pWindow*/, KStringView /*sTitle*/, const std::vector<KString>& /*Extensions*/, bool /*bMultiple*/, bool /*bDirectories*/)
//-----------------------------------------------------------------------------
{
	kDebug(1, "file dialogs need GTK 4.10 or newer");
	return {};

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* /*pWindow*/, KStringView /*sTitle*/, KStringView /*sSuggestedName*/, const std::vector<KString>& /*Extensions*/)
//-----------------------------------------------------------------------------
{
	kDebug(1, "file dialogs need GTK 4.10 or newer");
	return {};

} // SaveFileDialog

#endif // GTK_CHECK_VERSION(4, 10, 0)

#else // GTK 3

namespace {

//-----------------------------------------------------------------------------
// runs the native chooser and returns what was chosen
std::vector<KString> RunChooser(GtkFileChooserNative* Chooser)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Files;

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(Chooser)) == GTK_RESPONSE_ACCEPT)
	{
		auto List = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(Chooser));

		for (auto Item = List; Item; Item = Item->next)
		{
			Files.push_back(static_cast<const gchar*>(Item->data));
		}

		g_slist_free_full(List, g_free);
	}

	g_object_unref(Chooser);
	return Files;

} // RunChooser

} // end of anonymous namespace

//-----------------------------------------------------------------------------
std::vector<KString> OpenFileDialog(void* pWindow, KStringView sTitle, const std::vector<KString>& Extensions, bool bMultiple, bool bDirectories)
//-----------------------------------------------------------------------------
{
	auto Chooser = gtk_file_chooser_native_new(sTitle.empty() ? nullptr : KString(sTitle).c_str(), Window(pWindow),
	                                           bDirectories ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN,
	                                           nullptr, nullptr);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(Chooser), bMultiple);

	if (!Extensions.empty() && !bDirectories)
	{
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(Chooser), FileFilter(Extensions));
	}

	return RunChooser(Chooser);

} // OpenFileDialog

//-----------------------------------------------------------------------------
KString SaveFileDialog(void* pWindow, KStringView sTitle, KStringView sSuggestedName, const std::vector<KString>& Extensions)
//-----------------------------------------------------------------------------
{
	auto Chooser = gtk_file_chooser_native_new(sTitle.empty() ? nullptr : KString(sTitle).c_str(), Window(pWindow),
	                                           GTK_FILE_CHOOSER_ACTION_SAVE, nullptr, nullptr);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(Chooser), TRUE);

	if (!sSuggestedName.empty())
	{
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(Chooser), KString(sSuggestedName).c_str());
	}

	if (!Extensions.empty())
	{
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(Chooser), FileFilter(Extensions));
	}

	auto Files = RunChooser(Chooser);
	return Files.empty() ? KString{} : Files.front();

} // SaveFileDialog

#endif // GTK_MAJOR_VERSION

//-----------------------------------------------------------------------------
void SetMenu(void* pWindow, KStringView sAppName, const KJSON& jMenus, std::function<KString(KStringView)> Text,
             std::function<void(KStringView)> OnAction, std::function<void()> OnQuit)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(s_Mutex);
		s_MenuActions.clear();
		s_OnAction = std::move(OnAction);
		s_OnQuit   = std::move(OnQuit);
	}

	// the actions the entries trigger, on the window under the prefix "win"
	auto Actions = g_simple_action_group_new();
	auto Model   = g_menu_new();

	std::vector<std::pair<KString, KString>> Accelerators; // accelerator name, action name

	if (jMenus.is_array())
	{
		for (const auto& jMenu : jMenus)
		{
			auto Menu    = g_menu_new();
			auto Section = g_menu_new();

			for (const auto& jItem : jMenu["items"])
			{
				if (jItem["separator"].Bool())
				{
					// a separator ends a section
					g_menu_append_section(Menu, nullptr, G_MENU_MODEL(Section));
					g_object_unref(Section);
					Section = g_menu_new();
					continue;
				}

				KString sActionName;
				{
					std::lock_guard<std::mutex> Lock(s_Mutex);
					s_MenuActions.push_back(jItem["action"].String());
					sActionName = kFormat("menu{}", s_MenuActions.size() - 1);
				}

				auto Action = g_simple_action_new(sActionName.c_str(), nullptr);
				g_signal_connect(Action, "activate", G_CALLBACK(OnMenuAction), nullptr);
				g_action_map_add_action(G_ACTION_MAP(Actions), G_ACTION(Action));
				g_object_unref(Action);

				auto Item         = g_menu_item_new(jItem["title"].String().c_str(), kFormat("win.{}", sActionName).c_str());
				auto sAccelerator = AcceleratorName(jItem["key"].String());

				if (!sAccelerator.empty())
				{
					g_menu_item_set_attribute(Item, "accel", "s", sAccelerator.c_str());
					Accelerators.emplace_back(sAccelerator, kFormat("win.{}", sActionName));
				}

				g_menu_append_item(Section, Item);
				g_object_unref(Item);
			}

			g_menu_append_section(Menu, nullptr, G_MENU_MODEL(Section));
			g_object_unref(Section);
			g_menu_append_submenu(Model, jMenu["title"].String().c_str(), G_MENU_MODEL(Menu));
			g_object_unref(Menu);
		}
	}

	// the application's own menu: the quit entry, as the last one
	{
		auto Quit = g_simple_action_new("quit", nullptr);
		g_signal_connect(Quit, "activate", G_CALLBACK(OnQuitAction), nullptr);
		g_action_map_add_action(G_ACTION_MAP(Actions), G_ACTION(Quit));
		g_object_unref(Quit);

		auto Menu = g_menu_new();
		auto Item = g_menu_item_new(Text("kwa.menu.quit").c_str(), "win.quit");
		g_menu_item_set_attribute(Item, "accel", "s", "<Control>q");
		Accelerators.emplace_back("<Control>q", "win.quit");
		g_menu_append_item(Menu, Item);
		g_object_unref(Item);
		g_menu_append_submenu(Model, KString(sAppName).c_str(), G_MENU_MODEL(Menu));
		g_object_unref(Menu);
	}

	gtk_widget_insert_action_group(GTK_WIDGET(Window), "win", G_ACTION_GROUP(Actions));
	g_object_unref(Actions);

	// the menu bar above the page: webview/webview made the view the window's
	// only child, we put a box in between. The view keeps its own reference
#if GTK_MAJOR_VERSION >= 4
	auto MenuBar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(Model));
	auto View    = gtk_window_get_child(Window);
	auto Box     = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	if (View)
	{
		g_object_ref(View);
		gtk_window_set_child(Window, nullptr);
	}

	gtk_box_append(GTK_BOX(Box), MenuBar);

	if (View)
	{
		gtk_widget_set_vexpand(View, TRUE);
		gtk_box_append(GTK_BOX(Box), View);
		g_object_unref(View);
	}

	gtk_window_set_child(Window, Box);

	// the shortcuts, before the page sees the keys
	auto Controller = gtk_shortcut_controller_new();
	gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(Controller), GTK_SHORTCUT_SCOPE_GLOBAL);
	gtk_event_controller_set_propagation_phase(Controller, GTK_PHASE_CAPTURE);

	for (const auto& Accelerator : Accelerators)
	{
		gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(Controller),
		                                     gtk_shortcut_new(gtk_shortcut_trigger_parse_string(Accelerator.first.c_str()),
		                                                      gtk_named_action_new(Accelerator.second.c_str())));
	}

	gtk_widget_add_controller(GTK_WIDGET(Window), Controller);
#else
	auto MenuBar = gtk_menu_bar_new_from_model(G_MENU_MODEL(Model));
	auto View    = gtk_bin_get_child(GTK_BIN(Window));
	auto Box     = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	if (View)
	{
		g_object_ref(View);
		gtk_container_remove(GTK_CONTAINER(Window), View);
	}

	gtk_box_pack_start(GTK_BOX(Box), MenuBar, FALSE, FALSE, 0);

	if (View)
	{
		gtk_box_pack_start(GTK_BOX(Box), View, TRUE, TRUE, 0);
		g_object_unref(View);
	}

	gtk_container_add(GTK_CONTAINER(Window), Box);
	gtk_widget_show_all(Box);

	// the shortcuts: the window sees the key before the page does
	s_Accelerators.clear();

	for (const auto& Accelerator : Accelerators)
	{
		// "win.menu0" -> the action "menu0" in our group
		KStringView sName = Accelerator.second;
		sName.remove_prefix("win.");
		s_Accelerators.emplace_back(Accelerator.first, g_action_map_lookup_action(G_ACTION_MAP(Actions), KString(sName).c_str()));
	}

	g_signal_connect(Window, "key-press-event", G_CALLBACK(OnKeyPress), nullptr);
#endif

	g_object_unref(Model);

} // SetMenu

//-----------------------------------------------------------------------------
std::vector<KString> PreferredLanguages()
//-----------------------------------------------------------------------------
{
	// the POSIX locale variables, in their order of precedence: LANGUAGE is a
	// colon separated list, the others one locale like "de_DE.UTF-8"
	std::vector<KString> Languages;

	auto Add = [&Languages](KStringView sLocale)
	{
		// "de_DE.UTF-8@euro" -> "de-DE", and the C locales are no language
		sLocale = sLocale.substr(0, sLocale.find_first_of(".@"));

		if (sLocale.empty() || sLocale == "C" || sLocale == "POSIX")
		{
			return;
		}

		KString sTag(sLocale);
		sTag.Replace('_', '-');

		if (std::find(Languages.begin(), Languages.end(), sTag) == Languages.end())
		{
			Languages.push_back(std::move(sTag));
		}
	};

	for (auto sPart : kGetEnv("LANGUAGE").Split(":"))
	{
		Add(sPart);
	}

	for (auto sVar : { "LC_ALL", "LC_MESSAGES", "LANG" })
	{
		Add(kGetEnv(sVar));
	}

	return Languages;

} // PreferredLanguages

//-----------------------------------------------------------------------------
bool GetWindowFrame(void* pWindow, WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return false;
	}

	int iWidth  { 0 };
	int iHeight { 0 };

#if GTK_MAJOR_VERSION >= 4
	// GTK 4 knows no window positions - the default size follows the window's size
	gtk_window_get_default_size(Window, &iWidth, &iHeight);
	Frame.iX = 0;
	Frame.iY = 0;
#else
	int iX { 0 };
	int iY { 0 };
	gtk_window_get_position(Window, &iX, &iY);
	gtk_window_get_size(Window, &iWidth, &iHeight);
	Frame.iX = iX;
	Frame.iY = iY;
#endif

	Frame.iWidth  = iWidth;
	Frame.iHeight = iHeight;
	return iWidth > 0 && iHeight > 0;

} // GetWindowFrame

//-----------------------------------------------------------------------------
void SetWindowFrame(void* pWindow, const WindowFrame& Frame)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window || Frame.iWidth <= 0 || Frame.iHeight <= 0)
	{
		return;
	}

#if GTK_MAJOR_VERSION >= 4
	gtk_window_set_default_size(Window, Frame.iWidth, Frame.iHeight);
#else
	gtk_window_move(Window, Frame.iX, Frame.iY);
	gtk_window_resize(Window, Frame.iWidth, Frame.iHeight);
#endif

} // SetWindowFrame

//-----------------------------------------------------------------------------
void WatchWindowFrame(void* pWindow, std::function<void(const WindowFrame&)> OnChange)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(s_Mutex);
		s_OnFrame = std::move(OnChange);
	}

	if (s_WatchedWindow == GTK_WIDGET(Window))
	{
		return;
	}

	s_WatchedWindow = GTK_WIDGET(Window);

#if GTK_MAJOR_VERSION >= 4
	g_signal_connect(Window, "notify::default-width",  G_CALLBACK(OnDefaultSizeChanged), nullptr);
	g_signal_connect(Window, "notify::default-height", G_CALLBACK(OnDefaultSizeChanged), nullptr);
#else
	g_signal_connect(Window, "configure-event", G_CALLBACK(OnConfigure), nullptr);
#endif

} // WatchWindowFrame

//-----------------------------------------------------------------------------
void UnwatchWindowFrame()
//-----------------------------------------------------------------------------
{
	// the signals stay connected, the handler is gone
	std::lock_guard<std::mutex> Lock(s_Mutex);
	s_OnFrame = nullptr;

} // UnwatchWindowFrame

//-----------------------------------------------------------------------------
bool ActivateProcess(int64_t /*iPID*/)
//-----------------------------------------------------------------------------
{
	// no way to raise another process' window on Wayland, and none that is
	// worth it on X11 - the second start ends with its hint
	return false;

} // ActivateProcess

//-----------------------------------------------------------------------------
bool ActivateWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return false;
	}

	gtk_window_present(Window);
	return true;

} // ActivateWindow

//-----------------------------------------------------------------------------
bool AllowMediaCapture(void* pWebView)
//-----------------------------------------------------------------------------
{
	if (!pWebView)
	{
		return false;
	}

	g_signal_connect(WEBKIT_WEB_VIEW(pWebView), "permission-request", G_CALLBACK(OnPermissionRequest), nullptr);
	return true;

} // AllowMediaCapture

//-----------------------------------------------------------------------------
void SetBadge(KStringView sText)
//-----------------------------------------------------------------------------
{
	kDebug(2, "no badge on this platform: '{}'", sText);

} // SetBadge

//-----------------------------------------------------------------------------
int64_t RequestAttention(void* pWindow, bool /*bCritical*/)
//-----------------------------------------------------------------------------
{
#if GTK_MAJOR_VERSION >= 4
	// GTK 4 dropped the urgency hint
	kDebug(2, "no attention request on this platform");
	return 0;
#else
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return 0;
	}

	gtk_window_set_urgency_hint(Window, TRUE);
	return 1;
#endif

} // RequestAttention

//-----------------------------------------------------------------------------
void CancelAttention(void* pWindow, int64_t /*iRequest*/)
//-----------------------------------------------------------------------------
{
#if GTK_MAJOR_VERSION < 4
	auto Window = kwebapp::Window(pWindow);

	if (Window)
	{
		gtk_window_set_urgency_hint(Window, FALSE);
	}
#endif

} // CancelAttention

//-----------------------------------------------------------------------------
bool SetNavigationPolicy(void* pWebView, NavigationPolicy Policy)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_Mutex);
		s_Policy = std::move(Policy);
	}

	if (!pWebView)
	{
		return false;
	}

	auto View = WEBKIT_WEB_VIEW(pWebView);

	g_signal_connect(View, "decide-policy", G_CALLBACK(OnDecidePolicy), nullptr);
	g_signal_connect(View, "create",        G_CALLBACK(OnCreate),       nullptr);

	// downloads are announced by the session (6.0) or the context (4.x) the view uses
#if GTK_MAJOR_VERSION >= 4
	g_signal_connect(webkit_web_view_get_network_session(View), "download-started", G_CALLBACK(OnDownloadStarted), nullptr);
#else
	g_signal_connect(webkit_web_view_get_context(View),         "download-started", G_CALLBACK(OnDownloadStarted), nullptr);
#endif

	return true;

} // SetNavigationPolicy

//-----------------------------------------------------------------------------
void ClearNavigationPolicy()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_Mutex);
	s_Policy = NavigationPolicy{};

} // ClearNavigationPolicy

//-----------------------------------------------------------------------------
bool SetHideOnClose(void* pWindow, bool bHide)
//-----------------------------------------------------------------------------
{
	auto Window = kwebapp::Window(pWindow);

	if (!Window)
	{
		return false;
	}

	s_bHideOnClose = bHide;

	// the close handler also reports the last frame - connect it once
	static bool s_bConnected = false;

	if (!s_bConnected)
	{
		s_bConnected = true;
#if GTK_MAJOR_VERSION >= 4
		g_signal_connect(Window, "close-request", G_CALLBACK(OnCloseRequest), nullptr);
#else
		g_signal_connect(Window, "delete-event",  G_CALLBACK(OnDeleteEvent),  nullptr);
#endif
	}

	return true;

} // SetHideOnClose

//-----------------------------------------------------------------------------
void HideWindow(void* pWindow)
//-----------------------------------------------------------------------------
{
	if (pWindow)
	{
#if GTK_MAJOR_VERSION >= 4
		gtk_widget_set_visible(GTK_WIDGET(pWindow), FALSE);
#else
		gtk_widget_hide(GTK_WIDGET(pWindow));
#endif
	}

} // HideWindow

//-----------------------------------------------------------------------------
bool IsWindowVisible(void* pWindow)
//-----------------------------------------------------------------------------
{
	return pWindow && gtk_widget_get_visible(GTK_WIDGET(pWindow)) != FALSE;

} // IsWindowVisible

//-----------------------------------------------------------------------------
void WatchApplication(void* /*pWindow*/, std::function<void()> /*OnActivate*/)
//-----------------------------------------------------------------------------
{
	// no application object to be activated - a hidden window comes back
	// through Show() only

} // WatchApplication

//-----------------------------------------------------------------------------
bool SaveSecret(KStringView sService, KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	// the secret service through its command line tool, the value on stdin
	auto sCommand = kFormat("printf %s {} | secret-tool store --label={} app {} key {}",
	                        kEscapeForCommands(sValue), kEscapeForCommands(sService),
	                        kEscapeForCommands(sService), kEscapeForCommands(sKey));
	return kSystem(sCommand) == 0;

} // SaveSecret

//-----------------------------------------------------------------------------
bool LoadSecret(KStringView sService, KStringView sKey, KString& sValue)
//-----------------------------------------------------------------------------
{
	auto sCommand = kFormat("secret-tool lookup app {} key {}", kEscapeForCommands(sService), kEscapeForCommands(sKey));

	KString sOutput;

	if (kSystem(sCommand, sOutput) != 0)
	{
		return false;
	}

	sValue = std::move(sOutput);
	return true;

} // LoadSecret

//-----------------------------------------------------------------------------
bool DeleteSecret(KStringView sService, KStringView sKey)
//-----------------------------------------------------------------------------
{
	auto sCommand = kFormat("secret-tool clear app {} key {}", kEscapeForCommands(sService), kEscapeForCommands(sKey));
	return kSystem(sCommand) == 0;

} // DeleteSecret

namespace {

//-----------------------------------------------------------------------------
void OnCacheCleared(GObject*, GAsyncResult*, gpointer pDone)
//-----------------------------------------------------------------------------
{
	auto Done = static_cast<std::function<void()>*>(pDone);

	kDebug(2, "web caches cleared");

	if (*Done)
	{
		(*Done)();
	}

	delete Done;

} // OnCacheCleared

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void ClearWebCache(std::function<void()> Done)
//-----------------------------------------------------------------------------
{
	// the caches only - local storage and databases hold user data
	auto Types = static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE
	                                                 | WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS);
#if GTK_MAJOR_VERSION >= 4
	auto Manager = webkit_network_session_get_website_data_manager(webkit_network_session_get_default());
#else
	auto Manager = webkit_web_context_get_website_data_manager(webkit_web_context_get_default());
#endif

	webkit_website_data_manager_clear(Manager, Types, 0, nullptr, OnCacheCleared, new std::function<void()>(std::move(Done)));

} // ClearWebCache

} // namespace kwebapp

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_WEBVIEW && !DEKAF2_IS_MACOS && !DEKAF2_IS_WINDOWS
