# DEKAF2 — Lighter, Faster, Smarter

![C++14/17/20/23](https://img.shields.io/badge/C%2B%2B-14%2F17%2F20%2F23-blue.svg)
![MIT License](https://img.shields.io/badge/license-MIT-green.svg)
![Version 2.1](https://img.shields.io/badge/version-2.1-orange.svg)

**DEKAF2** is a general-purpose C++ application framework: one library that
covers the everyday needs of server-side, command-line, and browser-fronted
software — strings, containers, streams, JSON, XML, CSV, SQL, HTTP client
and server, REST, WebSockets, HTML generation and UI components,
cryptography, compression, threading, logging — behind one consistent API.

It compiles to native code on Linux, macOS, and Windows:

> **Compile once.  Run everywhere.  Sleep at night.**

The resulting binaries start fast, have a small memory footprint, and serve
high connection counts on modest hardware. DEKAF2 is written in modern C++,
builds with any standard from C++14 through C++23, and is released under
the **MIT license**.

## Why (and when) DEKAF2?

C++ has excellent specialized libraries for almost everything. Combining a
dozen of them, however — each with its own string type, error handling,
build peculiarities, and API style — is a project of its own. DEKAF2 takes
the opposite approach: one library, one style, one string type, everything
designed to work together.

DEKAF2 is a good fit when:

- you build networked services, tools, or data processing in C++ and
  prefer batteries included over dependency assembly
- you care about resource usage — native binaries, fast startup,
  predictable memory behavior
- the same code has to run on Linux, macOS, and Windows
- you need SQL, HTTP, and crypto in one program without writing glue
  between three ecosystems
- you want a user interface without leaving C++: pages and components are
  built as typed objects (`html::` elements, `html::ui` components), served
  by the built-in HTTP server, and displayed by any browser — the browser is
  the screen, much as it is for an Electron application, while the
  application itself stays a single native binary

It works best as the foundation of an application rather than as the
fifteenth library on the link line — if you need one isolated feature only,
a specialized library may serve you better. Used as intended, it replaces
most of the usual dependency stack: DEKAF2 itself relies on little more
than OpenSSL and parts of Boost.

DEKAF2 is not an experiment. It has been in continuous development since
2016 and drives large-scale commercial production systems, among them a
large website translation platform. A unit-test suite with nearly one
hundred thousand checks accompanies every build.

## Highlights

### KString — strings without the sharp edges

A wrapper around `std::string` that adds the string operations known from
Python or JavaScript: split, join, trim, replace, case conversion, number
parsing, regular expressions — one include away. Out-of-range access is
handled gracefully instead of being undefined behavior, and searching is
substantially faster than in typical `std::string` implementations (up to
50× in benchmarks).

KString is not a new string type you have to adopt: it is API-compatible with
`std::string` and converts implicitly in both directions. Every DEKAF2
function that takes a string — by value, by reference, or as a
`KStringView` — accepts a `std::string`, including the output parameters
(declared as `KStringRef&`, an alias of `std::string`), and a `KString`
passes wherever a `std::string` is expected. Existing code and third-party
libraries keep their strings.

### KSQL — one API for every major database

**MySQL, MariaDB, PostgreSQL, SQLite, SQL Server (CTLIB / DBLIB)** — all
through a single, unified API, with connection pooling, automatic retries,
and query logging. Query parameters are SQL-injection-safe by construction:
string arguments are escaped at format time, and dynamic format strings are
rejected. Results iterate with a range-for loop.

### A complete HTTP stack — from socket to REST

- **HTTP/1.1, HTTP/2, and HTTP/3 (QUIC)** for clients, HTTP/1.1 for servers
- **REST framework** with declarative routing, JSON / XML / plain-text I/O
- **WebSocket** client and server, with permessage-deflate compression
- **TLS** with automatic certificates: ephemeral self-signed ones for
  development, **Let's Encrypt** (ACME, tls-alpn-01) with automatic renewal
  in production
- Built-in **rate limiting**, connection limiting, and access logging

A production-ready REST service fits in a single source file — see the
example below.

## What's in the box

| Topic          | Contents                                                                                                                 |
|----------------|--------------------------------------------------------------------------------------------------------------------------|
| **Core**       | strings (KString, KStringView), formatting, logging, error handling, initialization                                      |
| **Containers** | associative and sequential containers, caches, memory management                                                         |
| **I/O**        | stream architecture, readers and writers, compression filters (zstd, Brotli, gzip, LZMA), pipes                          |
| **System**     | filesystem, process management and shell execution, shared memory, OS interface                                          |
| **Network**    | TCP, TLS, QUIC, UDP and DTLS, Unix sockets, DNS, IP addressing, geolocation, MQTT client                                 |
| **HTTP**       | protocol, client, server, cookies, WebSocket                                                                             |
| **REST**       | routing framework, file serving, rate limiting                                                                           |
| **Web**        | URLs and MIME types, HTML parser and DOM, typed HTML generation and UI components, push notifications, ACME certificates |
| **Crypto**     | hashing, ciphers, RSA, elliptic curves (incl. Ed25519, X25519), key derivation, OpenID Connect and JWT, AWS SigV4        |
| **Data**       | JSON (DOM and SAX), XML, CSV, SQL, text templates                                                                        |
| **Time**       | clock and date, durations and timers, time series, cron scheduling                                                       |
| **Threading**  | thread pools, parallel execution, pub/sub, synchronization primitives                                                    |
| **Utilities**  | CLI option parsing, mail (SMTP with spooling), tar/zip archives, text processing, identifiers, QR codes                  |

## Getting started

```sh
git clone https://github.com/ridgeware/dekaf2.git
cd dekaf2
./bootstrap go
```

DEKAF2 builds with **CMake** (≥ 3.13) and requires a C++14-capable compiler
(GCC ≥ 6, Clang ≥ 5, MSVC ≥ 2017). `bootstrap` checks the build
environment, sets up what is missing, and builds the library.
`bootstrap -static` (CMake: `DEKAF2_PREFER_STATIC_LIBS`) links the
third-party libraries statically; on a musl system such as Alpine, with the
static variants of the dependency packages installed, this yields fully
static (and hence portable) Linux binaries.

The full API documentation is online at
[ridgeware.github.io/dekaf2](https://ridgeware.github.io/dekaf2/index.html);
the `dekaf2-doc` target in the CMake build directory generates the same
pages locally (requires doxygen and graphviz).

## A taste of DEKAF2

A minimal REST server:

```cpp
#include <dekaf2/krest.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

int main()
{
    KInit(true);

    KRESTRoutes Routes;
    Routes.AddRoute("/hello")
        .Get([](KRESTServer& http)
        {
            http.json.tx["message"] = "Hello, World!";
        });

    KREST::Options Settings;
    Settings.Type = KREST::HTTP;
    Settings.iPort = 8080;
    Settings.bBlocking = true;

    KREST Http;
    Http.Execute(Settings, Routes);
}
```

Connect to any database:

```cpp
#include <dekaf2/ksql.h>

using namespace dekaf2;

KSQL db;
db.SetConnect(KSQL::DBT::MYSQL, "user", "pass", "mydb", "server.local");
db.ExecQuery("SELECT name, email FROM users WHERE active = {}", 1);

for (auto& row : db)
{
    kPrintLine("{}: {}", row["name"], row["email"]);
}
```

## Samples

Complete programs in [`samples/`](samples/) show the library at work. They
build together with the library (`DEKAF2_BUILD_SAMPLES`, on by default):

| Sample                           | What it does                                                                                                                |
|----------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| [`ksql`](samples/ksql.cpp)       | command line client for every SQL database supported by KSQL: runs SQL files or statements, or an interactive console       |
| [`kurl`](samples/kurl.cpp)       | HTTP query tool in the spirit of curl, using the HTTP/1.1, HTTP/2 and HTTP/3 client                                         |
| [`khttp`](samples/khttp.cpp)     | a small HTTP(S) file server: directory listings, uploads, per-user permissions, optional WebDAV, Let's Encrypt certificates |
| [`kssod`](samples/kssod.cpp)     | a persistent OpenID Connect provider (SSO server) with a browser UI built from `html::` objects, Let's Encrypt certificates |
| [`ktunnel`](samples/ktunnel.cpp) | encrypted tunnels through a relay (outlet, relay, inlet roles), with an admin interface                                     |
| [`kgeoip`](samples/kgeoip.cpp)   | resolves IP addresses to locations                                                                                          |
| [`kmqtt`](samples/kmqtt.cpp)     | explores an MQTT broker from the command line                                                                               |

## License

DEKAF2 is © 2017–2026 Ridgeware, Inc. and released under the
[MIT License](https://opensource.org/licenses/MIT).
