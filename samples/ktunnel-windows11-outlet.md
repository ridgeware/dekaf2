# Installing ktunnel as an *Outlet* on Windows 11

This guide describes installing the dekaf2 sample app **ktunnel** on a Windows 11
machine that sits **behind a firewall** (no inbound traffic possible, as in mobile
networks) and, on its own, opens a connection to a **Relay** at any IP/DNS
*outside*. An outlet runs inside the target network and opens the final connections
to the targets the relay asks for.

---

## 1. Roles in brief

```
       e.g. port 1234           e.g. port 443
                                      |
 app   >>--TCP/TLS-->> ktunnel <<--TLS-WS--<< ktunnel >>--TCP/TLS-->> target(s)
                       (relay)        |       (outlet)
                             firewall |
```

* **Relay** – listens (publicly reachable), manages outlets/inlets/tunnels, decides
  the forwarding targets. Runs *elsewhere*, not on this PC.
* **Outlet** – *this Windows PC*. Opens an **outbound-only** HTTPS/WebSocket
  control connection to the Relay and, on its request, opens data streams to
  the targets. **It does not listen on any port.**

The switch that turns a ktunnel into an Outlet is `-relay <host>` (formerly
`-e`/`-exposed`, still accepted):
> *"the relay to keep an ongoing control connection to. If not defined, then
> this ktunnel is the relay itself."*

---

## 2. Prerequisites

* `ktunnel.exe` (release build of dekaf2 for Windows).
* A running **Relay** with an **enabled outlet account** matching your
  `-n <name>` / `-s <secret>`. Created on the relay side, e.g. with
  `ktunnel -add-outlet -outlet-name win11-outlet -pass-file secret.txt` or via the
  admin UI (`/Configure/`, page *Outlets*). The **forwarding targets are configured
  on the relay side**, not here.
* Optional but recommended: the **Ed25519 fingerprint** of the Relay for
  identity pinning. On the relay, print it with:
  ```
  ktunnel -fingerprint
  ```
  Output is lowercase hex with colons (e.g. `a1:b2:c3:…`).
* **Administrator rights** on the Windows PC (all service and Defender commands
  require an **elevated** Command Prompt / PowerShell → *"Run as administrator"*).
  Without elevation, `-install` fails with *"install requires administrator
  privileges"*.

---

## 3. Place the binary correctly — **not** in the user profile

Copy `ktunnel.exe` to:

```
C:\Program Files\ktunnel\ktunnel.exe
```

> ⚠️ **Important:** **Never** install the service from a profile path
> (`C:\Users\<name>\…`). The SCM records the absolute path and starts the service
> under *LocalSystem*, independent of the interactive session. When the user logs
> off (including an RDP disconnect), Windows unloads their profile and may kill the
> running service image with exit code **1067** (`ERROR_PROCESS_ABORTED`) or
> fast-fail **0xc0000409**. Therefore: machine-wide path under `C:\Program
> Files\…`, runtime state under `C:\ProgramData\…`.

---

## 4. Clear quarantine / SmartScreen / Defender (as Administrator)

Because `ktunnel.exe` is typically unsigned and may have come from the network,
three things otherwise get in the way: **Mark-of-the-Web** (execution block),
**SmartScreen**, and **Microsoft Defender Antivirus** (quarantine). Run these steps
**before** the first launch/install, in an **elevated PowerShell**:

**a) Remove the Mark-of-the-Web (“unblock” the file)**

```powershell
Unblock-File -Path "C:\Program Files\ktunnel\ktunnel.exe"
```
(GUI equivalent: right-click the file → *Properties* → *General* tab → check
*Unblock* → *Apply*.)

**b) Add a Defender exclusion so the EXE does not get quarantined**

```powershell
Add-MpPreference -ExclusionPath "C:\Program Files\ktunnel\ktunnel.exe"
```
Alternatively the whole folder (`-ExclusionPath "C:\Program Files\ktunnel"`) or a
process exclusion (`-ExclusionProcess "ktunnel.exe"`). Verify:

```powershell
Get-MpPreference | Select-Object -ExpandProperty ExclusionPath
```

**c) If already quarantined** — set the exclusion first (step b), then restore:

```powershell
& "$env:ProgramFiles\Windows Defender\MpCmdRun.exe" -Restore -ListAll
& "$env:ProgramFiles\Windows Defender\MpCmdRun.exe" -Restore -Name <ThreatName>
```
GUI path: *Windows Security* → *Virus & threat protection* → *Protection history* →
find the entry → *Allow* / *Restore*.

**d) SmartScreen dialog "Windows protected your PC"** (only relevant on an
interactive double-click): *More info* → *Run anyway*. For service operation, a) + b)
are sufficient.

> **Cleanly permanent:** **Authenticode-sign** the EXE (ideally with an EV
> certificate) — then MOTW/SmartScreen warnings disappear for good and
> organization-wide, without per-machine exclusions.

---

## 5. Test interactively first (recommended)

Before registering the service, test the **exact runtime command line** once in an
elevated Command Prompt (without `-install`, without `--service`):

```
"C:\Program Files\ktunnel\ktunnel.exe" -relay tunnel.example.com -p 443 -n win11-outlet -s "S3cr3t!" -aes -trust-fingerprint "a1:b2:c3:...:ff"
```

You should see `connecting …`, then `control stream opened - now waiting for data
streams`. Stop with `Ctrl-C`. Only once that works, set it up as a service.

### Relevant options for the Outlet

| Option                    | Meaning                                                                                         |
| ------------------------- | ----------------------------------------------------------------------------------------------- |
| `-relay <host>`           | DNS/IP of the Relay. **Enables** outlet mode. Any external address. (`-e`/`-exposed` = aliases.)|
| `-p, --port <n>`          | Port on the Relay to connect to (default **443**).                                              |
| `-n, --name <name>`       | Outlet name to log in with (must be an enabled outlet account on the relay). (`-node` = alias.) |
| `-s, --secret <pw>`       | Password of the outlet account (bcrypt-checked). Required unless `-secret-file`.                |
| `-secret-file <path>`     | Runtime alternative to `-s`: read the secret from a file, kept out of the persisted service.    |
| `-aes`                    | Optional: X25519+Ed25519+HKDF handshake, identity pinning + forward secrecy on top of TLS.      |
| `-trust-fingerprint <fp>` | With `-aes`: accept exactly this server fingerprint (from `ktunnel -fingerprint`).              |
| `-to, --timeout <s>`      | Timeout / reconnect interval in seconds (default 30).                                           |

> For a **headless service** always use `-trust-fingerprint`, **not**
> `-trust-on-first-use`: the latter needs an interactive TTY and fails in a service
> context. With `-trust-fingerprint` the Outlet also writes **no** files
> (no `known_servers`) → no conflict with *Controlled Folder Access*.
>
> `-m/-maxtunnels`, `-f`, `-t`, `-cert`, `-key`, `-db` apply to the Relay
> only and are not set here. `-L` selects the *inlet* role, not the outlet role.

---

## 6. Install as a Windows service

First put the outlet secret in a file so it stays **out of** the SCM ImagePath (see
§10). In an **elevated** PowerShell, create it and lock it down to SYSTEM +
Administrators:

```powershell
New-Item -ItemType Directory -Force "C:\ProgramData\ktunnel" | Out-Null
Set-Content -NoNewline -Path "C:\ProgramData\ktunnel\secret.txt" -Value "S3cr3t!"
icacls "C:\ProgramData\ktunnel\secret.txt" /inheritance:r /grant "SYSTEM:R" "Administrators:R"
```

Then install — the same command line as the interactive test, prefixed with
`-install` and using `-secret-file` instead of `-s`:

```
"C:\Program Files\ktunnel\ktunnel.exe" -install -relay tunnel.example.com -p 443 -n win11-outlet -secret-file "C:\ProgramData\ktunnel\secret.txt" -aes -trust-fingerprint "a1:b2:c3:...:ff"
```

* Because `-relay` is present, `-install` recognizes the outlet role and
  **skips the admin bootstrap** (message: *"remote-role install (-relay given) —
  skipping admin bootstrap"*). No admin password is requested and no
  `ktunnel.db` / `ktunnel_ed25519.pem` is created — those are only needed by the
  Relay.
* `-secret-file` is a **runtime** flag: it is replayed on every start and the secret
  is read from the file each time, so the secret itself never enters the ImagePath.
  The service runs as LocalSystem, which can read the file above. (`-s` is still
  accepted but would land in the ImagePath — prefer `-secret-file`.)
* Everything else after `-install` is "baked into" the service and replayed unchanged
  on **every** start; internally the SCM also appends `--service`.
* The service is registered as **SERVICE_AUTO_START** (start at boot) under
  **LocalSystem** (internal name `ktunnel`, display name `KTunnel`, description
  "Secure reverse tunnel").

---

## 7. Start and verify the service

```
"C:\Program Files\ktunnel\ktunnel.exe" -start
"C:\Program Files\ktunnel\ktunnel.exe" -status
```
or with built-in tools:
```
sc start ktunnel
sc query ktunnel
```
`services.msc` shows the service under the display name **KTunnel**. On connection
loss, the Outlet automatically retries a reconnect at the `-timeout`
interval.

---

## 8. Firewall

The Outlet connects **outbound only** and does not listen on any port.
Windows Defender Firewall allows outbound traffic by default → usually **no inbound
rule is needed**. Only if outbound traffic is filtered by policy, add an outbound
allow rule (elevated PowerShell):

```powershell
New-NetFirewallRule -DisplayName "ktunnel outbound" -Direction Outbound `
  -Program "C:\Program Files\ktunnel\ktunnel.exe" -Action Allow `
  -Protocol TCP -RemotePort 443
```

---

## 9. Operation / maintenance

| Task      | Command (elevated)                                |
| --------- | ------------------------------------------------- |
| Stop      | `ktunnel -stop`  or `sc stop ktunnel`             |
| Status    | `ktunnel -status`  or `sc query ktunnel`          |
| Uninstall | `ktunnel -uninstall`                              |
| Update    | stop → replace EXE → `Unblock-File` again → start |

If the command line changes (relay, port, name, secret, fingerprint),
**uninstall and reinstall with the new arguments** — the SCM stores the arguments
statically in the ImagePath.

---

## 10. Security notes

* **Keep the secret out of the ImagePath:** prefer `-secret-file` (see §6) over
  `-s <secret>`. With `-s`, the secret is baked into the service ImagePath
  (`HKLM\SYSTEM\CurrentControlSet\Services\ktunnel\ImagePath`, readable by
  administrators via `sc qc ktunnel`). Either way it is the **outlet password**, not
  the admin password; rotate it on the relay side if compromised (then update the
  file and restart the service).
* **Identity pinning:** `-aes -trust-fingerprint` adds protection, on top of TLS,
  against an active TLS-intercepting middlebox — even where the local TLS trust store
  itself cannot be trusted (the Ed25519 pin does not rely on the certificate PKI).
  The outlet does not verify the relay's TLS certificate by default,
  so on any untrusted path the fingerprint pin is what authenticates the peer — use
  `-aes`.
* **Signing** the EXE avoids Defender exclusions and MOTW special handling.
