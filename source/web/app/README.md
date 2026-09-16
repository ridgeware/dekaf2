# KWebApp — desktop applications on the system webview

`KWebApp` runs a dekaf2 REST application as a desktop application. A native
window shows pages served by a `KREST` server in the same process. The page
calls C++ through a JavaScript bridge. The same pages can be served to browsers
on the network at the same time. The window is the operating system's webview:
WKWebView on macOS, WebKitGTK on Linux, WebView2 on Windows, wrapped by the
vendored [webview/webview](../../../from/webview/) header. No browser engine is
bundled. The implementation is C++ on all platforms; on macOS it uses the
Objective-C runtime API, there are no `.mm` files.

The API is documented in [kwebapp.h](kwebapp.h). This file describes the
architecture, the platform differences, the capability categories for
application design, and the known pitfalls. [ktail](../../../samples/ktail/ktail.cpp)
is the reference application.

## Architecture

```
                     +----------------------------------------------------+
                     |  your process                                      |
  native window      |                                                    |
  (system webview) <----- KREST on 127.0.0.1:<random port>, token guard   |
     |   ^           |         |                                          |
     |   | bridge    |         |  one KRESTRoutes table                   |
     v   |           |         |                                          |
  window.kNative.*  ---> Bind() handlers (C++)                            |
  window.kLive      <--> /_kwa/live  (websocket, Broadcast/Send/OnMessage)|
                     |         |                                          |
  browsers on the  <----- KREST with TLS on <addr>:<port>, login, session |
  network            |                                                    |
                     +----------------------------------------------------+
```

**Two servers, one route table.** The window uses plain HTTP to a `KREST`
bound to 127.0.0.1, because webviews do not accept self-signed certificates.
Browsers on the network use a second `KREST` with TLS and a login. Both serve
the same `KRESTRoutes`. `IsFromWindow(HTTP)` tells a handler which server
received the request.

**Loopback guard.** Any local process or browser tab can connect to
`127.0.0.1:<port>`. `KWebApp` therefore requires a token: 32 random bytes per
start, passed to the window in its first navigation
(`/_kwa/enter?token=...&next=...`), returned as an `HttpOnly; SameSite=Strict`
cookie, and required on every request. Without it the answer is 403, also for
static files. The `Host` header must be `127.0.0.1:<port>`, `Origin` (if
present) must be `http://127.0.0.1:<port>`, `Sec-Fetch-Site` (if present) must
be `same-origin` or `none`, and a websocket upgrade without `Origin` is
refused.

**Bridge.** `Bind(name, handler)` publishes `window.kNative.<name>(arg)`. The
handler receives one `KJSON` argument and returns `KJSON`; the page receives a
`Promise` of the result. Types are preserved (a C++ `true` is a JavaScript
`true`). An exception in the handler rejects the promise with
`{ "error": "<what>" }`. `window.kNative.platform()` and `.version()` are
synchronous. `typeof window.kNative !== 'undefined'` is the test for running
in the window rather than in a browser.

**Live channel.** `<script src="/_kwa/live.js">` defines `window.kLive` with
`send(obj)`, `on(fn)` and `onopen(fn)`; it reconnects automatically. On the C++
side `Broadcast(json)` sends to every connection, `Send(client, json)` to one,
`OnMessage(handler)` receives, `OnConnect(handler)` reports new connections.
Window and browser pages use the same channel. Use it for server-to-page
updates; use `Bind()` only for functions that exist in the window alone.

**Platform layer.** [bits/kwebapp_platform.h](bits/kwebapp_platform.h)
declares the OS functions; `bits/kwebapp_macos.cpp`, `bits/kwebapp_linux.cpp`
and `bits/kwebapp_windows.cpp` implement them. `KWebApp` calls them on the UI
thread.

### Threads

`Run()` runs the UI loop and must be called on the main thread. Route handlers
run in `KREST` threads. `Eval()`, `Navigate()`, `SetTitle()`, `Broadcast()`
and the desktop functions can be called from any thread; they dispatch to the
UI thread. `Bind()` handlers, `OnDownload` and `OnNotificationClick` run on the
UI thread and must return quickly. Long work belongs in a separate thread that
reports with `Eval()`. After `Quit()` all calls into the window are no-ops.
SIGINT and SIGTERM end `Run()` if the program was started with `KInit(true)`.

## Modes

| Mode     | Window | Loopback server | Network server  | Options                                                                                                                           |
|----------|--------|-----------------|-----------------|-----------------------------------------------------------------------------------------------------------------------------------|
| Desktop  | yes    | yes, token      | no              | defaults                                                                                                                          |
| Server   | no     | yes, token      | yes, TLS, login | `bWindow = false`, `bNetwork = true`                                                                                              |
| Combined | yes    | yes, token      | yes, TLS, login | `bNetwork = true`, plus `bQuitOnWindowClose = false` or `bHideOnClose = true` to keep the servers running after the window closes |

The network server always uses TLS: `Network.sCert` and `Network.sKey`, or an
ephemeral certificate that `KREST` stores under `~/.config/<program>/tls/`
(browsers warn about it; use a real certificate in production). It serves a
login page at `/login`, verifies credentials with `Options.Authenticate`, stores
the session in the cookie `__Host-session` (`Secure; HttpOnly; SameSite=Strict`)
and in `Options.SessionStore` (default: in memory; pass a
`KSessionSQLiteStore` for persistence), ends it at `/logout`, and answers
`/healthz` without login. All other paths require the session. Paths in
`Options.WindowOnlyPaths` are answered with 403 regardless of the session.

## Getting started

```cpp
#include <dekaf2/web/app/kwebapp.h>

KRESTRoutes Routes;

// the root is the empty route
Routes.AddRoute({ KHTTPMethod::GET, false, "", [](KRESTServer& HTTP)
{
	html::Page Page("Hello");
	Page.Add<html::Heading>(1, "Hello from KREST");
	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Print());
}});

KWebApp::Options Options;
Options.sTitle   = "Hello";
Options.sAppName = "hello";              // ~/.config/hello/ holds the window geometry and the instance lock
Options.jMenus   = kjson::Parse(R"([ { "title": "File", "items": [ { "title": "Save", "key": "s", "action": "save" } ] } ])");

KWebApp App(std::move(Options), Routes); // KREST::Options is move-only

// in the page: const ok = await window.kNative.save({ name: "x" });
App.Bind("save", [](const KJSON& jArg) -> KJSON { return jArg["name"].String() == "x"; });

return App.Run();
```

The CMake option `DEKAF2_WITH_WEBVIEW` (default ON) controls the build. If the
platform libraries are not found, the option is set to OFF with a status
message, `KWebApp` is not built, and `DEKAF2_HAS_WEBVIEW` is 0 in
`kconfiguration.h`. Application code can use the same macro.

| Platform                           | Build requirements                                                                                                                                                                                                                                                                                                    |
|------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| macOS                              | frameworks WebKit, AppKit, UserNotifications, Security (part of the system)                                                                                                                                                                                                                                           |
| Debian, Ubuntu                     | `libwebkitgtk-6.0-dev libgtk-4-dev` (Debian 12+, Ubuntu 24.04+); older releases `libwebkit2gtk-4.1-dev libgtk-3-dev`. Run time: `libnotify-bin` (notifications), `libsecret-tools` (secrets)                                                                                                                          |
| Fedora                             | `webkitgtk6.0-devel gtk4-devel`; run time `libnotify libsecret`                                                                                                                                                                                                                                                       |
| RHEL 9, Rocky Linux 9, AlmaLinux 9 | `webkit2gtk3-devel gtk3-devel` (API 4.1 on GTK 3)                                                                                                                                                                                                                                                                     |
| Alpine                             | `webkit2gtk-6.0-dev gtk4.0-dev`                                                                                                                                                                                                                                                                                       |
| Windows                            | WebView2 SDK headers: `vcpkg install webview2`, or unpack the NuGet package `Microsoft.Web.WebView2` (`https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/<version>`, a zip) and set `WEBVIEW2_SDK_DIR`. Run time: the WebView2 runtime (included in Windows 11 and Edge, otherwise the Evergreen installer) |

`scripts/buildsetup` installs the Linux packages as optional dependencies.

## API overview

| Area      | C++                                                                                                                                                                                     | JavaScript                                                                                                                                                                                                                                              |
|-----------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Pages     | `Navigate(path or allowed URL)`, `SetTitle()`, `Eval(js)`                                                                                                                               |                                                                                                                                                                                                                                                         |
| Bridge    | `Bind(name, KJSON(const KJSON&))`                                                                                                                                                       | `await window.kNative.<name>(arg)`, `kNative.platform()`, `kNative.version()`                                                                                                                                                                           |
| Live data | `Broadcast(json)`, `Send(client, json)`, `OnConnect()`, `OnMessage()`, `GetClientCount()`                                                                                               | `kLive.send(obj)`, `kLive.on(fn)`, `kLive.onopen(fn)`, after `<script src="/_kwa/live.js">`                                                                                                                                                             |
| Window    | `Show()`, `Hide()`, `IsWindowVisible()`, `Activate()`, `Quit()`, `IsQuitting()`                                                                                                         | `kNative.activate()`                                                                                                                                                                                                                                    |
| Desktop   | `Notify(title, body, tag, imageDataURL)`, `OnNotificationClick()`, `OpenExternal(url)`, `OpenFileDialog()`, `SaveFileDialog()`, `SetBadge()`, `RequestAttention()`, `CancelAttention()` | `kNative.notify({title, body, tag, image})`, `.openExternal({url})`, `.openFile({title, extensions, multiple, directories})`, `.saveFile({title, name, extensions})`, `.setBadge(text or count)`, `.requestAttention({critical})`, `.cancelAttention()` |
| Menus     | `Options.jMenus`; an entry's `action` calls the bound handler of that name if there is one                                                                                              | otherwise `window.addEventListener('kwa-menu', ev => ev.detail)` receives the action                                                                                                                                                                    |
| Downloads | `Options.sDownloadDir`, `OnDownload(path, error)`                                                                                                                                       | a link to a `Content-Disposition: attachment` response, `<a download>`, or a MIME type the view cannot display                                                                                                                                          |
| Secrets   | `SaveSecret()`, `LoadSecret()`, `DeleteSecret()`                                                                                                                                        | `kNative.saveSecret({key, value})`, `.loadSecret({key})` (string or `null`), `.deleteSecret({key})`                                                                                                                                                     |
| Network   | `Options.bNetwork`, `Network`, `Authenticate`, `SessionStore`, `Session`, `WindowOnlyPaths`, `sContentSecurityPolicy`; `IsFromWindow(HTTP)`, `GetSession()`                             | login page at `/login`                                                                                                                                                                                                                                  |
| Caches    | `ClearWebCache()` before `Run()`                                                                                                                                                        |                                                                                                                                                                                                                                                         |
| Origins   | `Options.AllowedOrigins`, `AddAllowedOrigin(origin)` at run time, `IsAllowedURL(url)`                                                                                                   |                                                                                                                                                                                                                                                         |

The names of the built-in bridge functions are reserved; `Bind()` rejects them.
The secret functions are published to the page only on platforms where the
navigation rules (next section) are enforced.

## Languages

`KWebApp` carries its own texts (login page, standard menus, download
notification) as a `KStringCatalog` in English, German, French, Italian,
Spanish, Japanese, Korean, Simplified and Traditional Chinese (`en`, `de`,
`fr`, `it`, `es`, `ja`, `ko`, `zh-Hans`, `zh-Hant`), embedded from
`source/web/app/strings/`. `Options.Catalog` adds the application's catalog and
may override the `kwa.*` identifiers. A request for `zh-TW` or `zh-HK` is
served by `zh-Hant`, one for `zh` or `zh-CN` by `zh-Hans`: the script a region
uses comes from the CLDR data in `from/cldr/` (`KStringCatalog::LikelyScript()`).

| What | Language source |
|---|---|
| a request (page, login) | cookie `lang`, else `Accept-Language`, else the default language - `GetLanguage(HTTP)`, `Text(HTTP)` |
| menus, notifications, dialogs | `Options.sLanguage`, else the user's choice from the last run (`settings.json`), else the system languages (`PreferredLanguages()`) - `GetLanguage()` |
| the user's choice | `POST /_kwa/lang` with `lang` and `next`: sets the cookie, from the window also the setting |

The webviews send `Accept-Language` from the system settings, so the window
needs no other path than a browser. A page builder uses
`auto T = App.Text(HTTP)` and `T("list.name")`, `T.Format("tail.lines", { { "count", 3 } })`,
and `App.AddStrings(Page, HTTP, "tail")` for the script side, which gets
`window.kText = { lang, messages }` with the raw messages for intl-messageformat.
A menu title in `Options.jMenus` that starts with `@` is a catalog identifier.
Catalogs are embedded with `dekaf2_embed_strings(<target> <dir>)` in CMake.

## Navigation rules

The window loads pages from the loopback server and from the origins listed in
`Options.AllowedOrigins`. Any other top-level navigation is cancelled and the
URL is opened in the system browser. `window.open()` and `target="_blank"` are
handled the same way; if the target is a loopback URL, the window navigates to
it instead. `"*"` allows all http(s) origins. `Navigate()` with a URL outside
the list is rejected with a debug message. The bridge is injected as a
main-frame user script, so restricting main-frame navigation keeps
`window.kNative` off foreign pages.

With an allowed origin the window acts as a shell for a hosted web
application: `Navigate("https://app.example.com/")` loads that site with
`window.kNative` available. `Options.sInitScript` runs before every document,
for example to pass configuration or to alias the bridge under another name.
`Options.bAllowMediaCapture` grants `getUserMedia()` without a webview prompt.
The loopback server remains available for the shell's own pages, e.g. a
first-run page; requests from the hosted site to it are cross-origin and get
403.

Downloads: a response the view cannot display, a `Content-Disposition:
attachment` response, or a link with the `download` attribute is written to
`Options.sDownloadDir` (default: the user's Downloads folder) under a unique
name (`report.txt`, `report 2.txt`, ...). A notification is posted and
`OnDownload(path, error)` is called. The page does not receive the path.

## Capability matrix

The page is the same in the window and in the browser. It checks
`window.kNative` and hides or replaces controls that need the window. The
server refuses window-only routes for network sessions. Assign every function
to one of four categories:

| Category                                                                   | Window                                                         | Browser                               | Implementation                                                                               |
|----------------------------------------------------------------------------|----------------------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------|
| 1. Interface: lists, forms, live data                                      | identical                                                      | identical                             | none needed                                                                                  |
| 2. Local files                                                             | native dialog, then the path (`kNative.openFile`, `.saveFile`) | upload or download through the server | page: dialog or form depending on `window.kNative`                                           |
| 3. OS integration: notifications, badge, menu, attention                   | `kNative.*`                                                    | web notifications, or omitted         | page: `if (window.kNative)`                                                                  |
| 4. Native only: reveal in file manager, screens, input injection, hardware | `Bind()` handler, control with class `native-only`             | not offered; the route answers 403    | page: hide the control. Server: `Options.WindowOnlyPaths` or `IsFromWindow()` in the handler |

ktail's assignment:

| Function                                    | Category | Window                                                                   | Browser                           |
|---------------------------------------------|----------|--------------------------------------------------------------------------|-----------------------------------|
| directory listing, live tail, note per file | 1        | page and websocket                                                       | same                              |
| download a file                             | 2        | link to `/download?file=` (attachment); saved to Downloads, notification | same link; the browser saves it   |
| notify on new lines                         | 3        | `kNative.notify()` when the tab is hidden                                | control hidden                    |
| show in file manager                        | 4        | `Bind("reveal")`, button with class `native-only`                        | button hidden; no route reachable |
| sign out                                    | -        | control hidden (`browser-only`); the window has no session               | form posting to `/logout`         |

Page-side mechanics: the init script does not touch the DOM. The page adds a
class to the root element when `window.kNative` exists
(`root.classList.add('native')`) and styles `.native-only` and `.browser-only`
by that class. For category 4 hiding the control is not sufficient: the route
must also be guarded with `WindowOnlyPaths` or `IsFromWindow()`, because a
network client can call any route directly.

## Platform differences

| Feature                                                         | macOS                                                                                                           | Linux (GTK 4 / GTK 3)                                                                         | Windows                                                                                     |
|-----------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| Menus                                                           | application menu bar with the standard application, Edit and Window menus                                       | menu bar inside the window above the page; shortcuts Ctrl+key; GTK 3 shows no shortcut labels | menu bar inside the window; shortcuts Ctrl+key via WebView2 `AcceleratorKeyPressed`         |
| Notifications                                                   | notification center when running from a bundle (tag replacement, picture, click reports); `osascript` otherwise | `notify-send` (picture as icon; no tag, no click reports)                                     | balloon on a tray icon, click reports; the tray icon is created with the first notification |
| File dialogs                                                    | NSOpenPanel, NSSavePanel                                                                                        | GtkFileDialog (GTK 4.10+), GtkFileChooserNative (GTK 3); none on GTK 4 before 4.10            | IFileOpenDialog, IFileSaveDialog                                                            |
| Window geometry                                                 | position and size                                                                                               | size only (Wayland has no window positions)                                                   | position and size                                                                           |
| Single instance                                                 | the first instance's window is activated                                                                        | the second start reports the error; the first window is not activated                         | the first instance's window is activated, also when hidden                                  |
| Hide on close                                                   | dock icon click and Cmd-Tab show the window again                                                               | only `Show()` shows it again                                                                  | tray icon click shows it again                                                              |
| Badge                                                           | dock badge                                                                                                      | not available                                                                                 | not implemented                                                                             |
| Attention                                                       | dock icon bounce                                                                                                | urgency hint on GTK 3; not available on GTK 4                                                 | taskbar flash                                                                               |
| Downloads, navigation rules, camera and microphone, cache clear | yes                                                                                                             | yes                                                                                           | yes                                                                                         |
| Secrets                                                         | keychain (Security framework)                                                                                   | `secret-tool`; requires a running secret service                                              | credential manager                                                                          |
| Open in browser                                                 | NSWorkspace                                                                                                     | `xdg-open`                                                                                    | ShellExecute                                                                                |

Status: macOS and Linux with GTK 4 are tested. The GTK 3 path and the Windows
layer compile against their headers and have not been run yet.

Rendering differs between the three engines. Use conservative HTML and CSS.
Set `color-scheme: light dark` and define colors as CSS variables; without
that, dialogs and menus are white on white in dark mode on macOS. Test the
pages in a browser too, e.g. through the network mode.

### Packaging

- **macOS.** Notifications and permissions (camera, microphone, local network)
  require an application bundle. The `ktail-app` target shows the minimum:
  copy the binary to `Contents/MacOS`, configure `Info.plist` (see
  [ktail-Info.plist.in](../../../samples/ktail/ktail-Info.plist.in)) with the usage
  description keys for every capability the program uses
  (`NSLocalNetworkUsageDescription`, `NSCameraUsageDescription`,
  `NSMicrophoneUsageDescription`, ...), sign the bundle. TCC permissions are
  bound to the code signature; an ad hoc signature changes with every build and
  loses granted permissions. Create a self-signed code signing identity once
  (Keychain Access, Certificate Assistant, "Code Signing") and use it for all
  builds, including debug builds (`KTAIL_CODESIGN_IDENTITY` for ktail). Plain
  `http://127.0.0.1` needs no App Transport Security exception (measured on
  macOS 26 in a bundle).
- **Linux.** dekaf2 provides no packaging. `.desktop` file and icon are the
  application's. Notifications and secrets require a notification server and a
  secret service; containers and servers usually have neither.
- **Windows.** Programs are console applications by default. A GUI subsystem
  program needs `WinMain` or `/ENTRY:mainCRTStartup`; `__argc` and `__argv`
  provide the arguments for `KOptions`. Link libraries are set by the CMake
  option. Notifications use a tray icon, because toasts require an
  AppUserModelID that only an installer registers.

## Security

Provided by `KWebApp`:

- loopback server: random token per start, cookie only, 403 for every request
  without it; `Host`, `Origin` and `Sec-Fetch-Site` checks; no unauthenticated
  path (`/healthz` is 404 there);
- network server: TLS always; login required for everything except `/healthz`,
  `/login` and `/logout`; session cookie `Secure; HttpOnly; SameSite=Strict`;
  login attempts limited per client address (one per 30 seconds sustained,
  burst of ten, then 429); 20 concurrent connections per address; 16 MiB
  request body limit unless the application sets its own; response headers
  `Strict-Transport-Security`, `X-Content-Type-Options: nosniff`,
  `X-Frame-Options: DENY`, `Referrer-Policy: same-origin` and
  `Options.sContentSecurityPolicy`;
- window: navigation rules restrict the bridge to the application's pages;
  `OpenExternal()` accepts http, https and mailto only; the secret bridge
  functions are published only where the rules are enforced.

Left to the application:

- the Content Security Policy of its pages. The default allows inline scripts
  and styles of the page and no external sources. In WKWebView the bridge is a
  user script and not subject to the page's CSP. For WebView2 this is not
  verified; test before setting a strict `script-src` on Windows;
- length limits on user input; rate limits on resource-creating requests; a
  persistent session store if sessions must survive a restart;
- certificate verification in its own HTTP clients (`kSetTLSDefaults` with
  `VerifyCert` at start); https with a fixed host for anything it downloads
  and executes;
- account management on the command line, no self-registration (ktail:
  `-user`, `-password-file`).

## Pitfalls

Observed in the desktop application `KWebApp` was derived from, or during its
implementation. The first column names who handles it.

| Who     | Pitfall                                                                                         | Handling                                                                              |
|---------|-------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------|
| KWebApp | webview requires the main thread; calls from other threads must be dispatched                   | `Run()` on the main thread; all other calls from any thread                           |
| KWebApp | bindings must be registered before the first document loads                                     | `Bind()` before `Run()` is queued; later calls register immediately                   |
| KWebApp | `eval` after the window is destroyed crashes                                                    | calls after `Quit()` are no-ops                                                       |
| KWebApp | the raw webview bridge passes strings: `"true" === true` fails, JSON gets parsed twice          | `Bind()` uses `KJSON` in and out; the shim parses once                                |
| KWebApp | the loopback server is reachable by every local process                                         | token guard, 403 without exception                                                    |
| KWebApp | a link to a foreign site exposes the bridge to that site                                        | navigation rules                                                                      |
| KWebApp | WKWebView drops downloads without a navigation delegate                                         | navigation and download delegates                                                     |
| KWebApp | notifications are not shown while the application is active; a click does not unhide the window | notification delegate; `Show()` on click                                              |
| KWebApp | long-running websocket handlers block server shutdown                                           | end the loop when `IsQuitting()` is true (see ktail's tail loop)                      |
| KWebApp | destroying the window while holding a lock that a callback takes deadlocks                      | the window is destroyed outside the lock                                              |
| you     | macOS permissions are bound to the code signature                                               | one fixed signing identity for all builds                                             |
| you     | missing `Info.plist` usage descriptions terminate the process on first access                   | declare every capability the program uses                                             |
| you     | webviews reject self-signed certificates                                                        | the window uses loopback HTTP; TLS is for the network server                          |
| you     | a service worker intercepting navigations blocked the application on the login page             | no service worker for local pages; on the network none handling `mode === 'navigate'` |
| you     | dialogs and menus without own colors are white on white in dark mode                            | `color-scheme: light dark` and color tokens from the start                            |
| you     | screen capture in WebKit returns the logical resolution and ignores content hints               | category 4, native only                                                               |
| you     | HTTP clients without certificate verification                                                   | `kSetTLSDefaults(VerifyCert)` at start; https with a fixed host for updates           |
| you     | the default request body limit is 256 MiB                                                       | `KWebApp` sets 16 MiB on the network server; override if needed                       |
| you     | CSP and the bridge on Windows                                                                   | verify before a strict `script-src` (see Security)                                    |
| you     | GTK 4 before 4.10 has no file dialogs; `secret-tool` and `notify-send` need daemons             | check the target systems; fall back to category 2                                     |
| you     | on Linux a hidden window can only be shown again with `Show()`                                  | leave `bHideOnClose` off unless the application provides a way to call `Show()`       |
| you     | with Cmd-V bound in the Edit menu the page gets no `keydown` for it                             | handle the DOM `paste` event and `clipboardData`; or `Options.bPasteShortcut = false` when the page must see the key |

## ktail

[ktail](../../../samples/ktail/ktail.cpp) browses a directory, follows a file live
over a websocket, keeps a note per file, offers the file as a download, reveals
it in the file manager, and notifies on new lines. It uses menus, the live
channel (notes saved in one view appear in all), the network mode with a
bcrypt-hashed password, and hide-on-close when serving the network.

```
ktail [-dir <path>] [-lang <tag>] [-inspector]
ktail -listen [addr:]port -user <name> -password-file <file> [-cert <pem> -key <pem>] [-headless]
```

Without arguments it shows the current directory, or the home directory when
started from the Finder. `cmake --build . --target ktail-app` builds the macOS
bundle. Its texts are the catalogs in `samples/ktail/strings/` (`en`, `de`),
embedded with `dekaf2_embed_strings()`: the page takes the language of the
request and offers a selector that posts to `KWebApp::LanguagePath`, the
menus take the language of the window (`-lang`, else the last choice, else
the system's). The password file holds the password in its first line, as
UTF-8 or as UTF-16/UTF-32 with a byte order mark, which is what PowerShell's
`>` writes; it is converted to UTF-8, the encoding a browser sends.

## Implementation notes

- Platform functions are declared in `bits/kwebapp_platform.h` and implemented
  in `bits/kwebapp_<os>.cpp`. All three files are always in the file list and
  guard themselves with `DEKAF2_HAS_WEBVIEW` and the platform macro.
- webview/webview's delegates lack several methods. On macOS the missing
  methods are added to its runtime classes with `class_addMethod` (media
  permission, new windows, `windowShouldClose:`, the application delegate's
  reopen and activation); the navigation and notification delegates are own
  runtime classes, held in statics because WebKit holds delegates weakly. On
  Linux the view is reparented into a `GtkBox` below the menu bar; the signals
  are connected on the view and on its network session (6.0) or context (4.x).
  On Windows the window procedure is subclassed and COM event handlers are
  registered on the WebView2 controller.
- On Linux webview.h includes X11 headers; their macros (`Bool`, `None`,
  `Status`, ...) are undefined in `kwebapp.cpp` after the include.
- `utests/kwebapp_tests.cpp` covers the guards, the origin rules, the secrets
  round trip (when a store is available) and the no-ops without a window.
