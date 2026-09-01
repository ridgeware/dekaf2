# Installing ktunnel as an *Outlet* on Linux

This guide describes installing the dekaf2 sample app **ktunnel** on a Linux machine
that sits **behind a firewall** (no inbound traffic possible, as in mobile networks)
and, on its own, opens a connection to a **Relay** at any IP/DNS *outside*. The
Relay is almost always another Linux machine. An outlet runs inside the target
network and opens the final connections to the targets the relay asks for.

The Outlet on Linux is installed as a **systemd** unit. KService writes the
unit file, runs `daemon-reload`, and enables it for boot.

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
  the forwarding targets. Runs *elsewhere*.
* **Outlet** – *this Linux host*. Opens an **outbound-only** HTTPS/WebSocket
  control connection to the Relay and, on its request, opens data streams to
  the targets. **It does not listen on any port.**

The switch that turns a ktunnel into an Outlet is `-relay <host>` (formerly
`-e`/`-exposed`, still accepted):
> *"the relay to keep an ongoing control connection to. If not defined, then
> this ktunnel is the relay itself."*

---

## 2. Prerequisites

* A `ktunnel` binary built for this Linux distro (or a dekaf2 install that ships it).
* A running **Relay** with an **enabled outlet account** matching your
  `-n <name>` / `-s <secret>`. Created on the relay side, e.g. with
  `ktunnel -add-outlet -outlet-name linux-outlet -pass-file secret.txt` or via the
  admin UI (`/Configure/`, page *Outlets*). The **forwarding targets are configured
  on the relay side**, not here.
* Optional but recommended: the **Ed25519 fingerprint** of the Relay for
  identity pinning. On the relay, print it with:
  ```
  ktunnel -fingerprint
  ```
  Output is lowercase hex with colons (e.g. `a1:b2:c3:…`).
* **root** for a system-wide (boot-persistent) install. Without root, `-install`
  automatically falls back to a **user-scope** unit (see §6).

---

## 3. Place the binary

Put the binary in a machine-wide location that is on root's `PATH` and readable by
the service account, e.g.:

```
sudo install -m 0755 ktunnel /usr/local/bin/ktunnel
```

Avoid `/home/<user>/…` for a system service: on SELinux-enforcing distros
(RHEL/Fedora) systemd may be denied execution of binaries under a home directory,
and the path is not guaranteed to be mounted early at boot. `/usr/local/bin` or
`/opt/ktunnel/` are safe. Make sure the executable bit is set (`chmod +x`).

> There is no "quarantine" concept as on Windows. If SELinux is enforcing and you
> deliberately keep the binary outside a standard bin directory, restore the default
> context with `restorecon -v /usr/local/bin/ktunnel`, or label a custom `/opt`
> location as `bin_t`.

---

## 4. Test interactively first (recommended)

Before registering the service, run the **exact runtime command line** once in a
shell (without `-install`):

```
ktunnel -relay tunnel.example.com -p 443 -n linux-outlet -s "S3cr3t!" -aes -trust-fingerprint "a1:b2:c3:...:ff"
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
> `-trust-on-first-use`: the latter needs an interactive TTY and fails under
> systemd. With `-trust-fingerprint` the Outlet also writes **no** files
> (no `known_servers`).
>
> `-m/-maxtunnels`, `-f`, `-t`, `-cert`, `-key`, `-db` apply to the Relay
> only and are not set here. `-L` selects the *inlet* role, not the outlet role.

---

## 5. Install as a systemd service (system-wide)

First put the outlet secret in a file readable only by the service account — this
keeps it **out of** the systemd unit (see §10):

```
sudo install -d -m 0750 /etc/ktunnel
printf '%s' 'S3cr3t!' | sudo tee /etc/ktunnel/secret >/dev/null
sudo chmod 0600 /etc/ktunnel/secret
```

Then run the same command line as the test, prefixed with `-install`, using
`-secret-file` instead of `-s`, as root:

```
sudo /usr/local/bin/ktunnel -install -relay tunnel.example.com -p 443 -n linux-outlet -secret-file /etc/ktunnel/secret -aes -trust-fingerprint "a1:b2:c3:...:ff"
```

What this does:

* Because `-relay` is present, `-install` recognizes the outlet role and **skips
  the admin bootstrap** (message: *"remote-role install (-relay given) — skipping
  admin bootstrap"*). No admin password is requested and no `ktunnel.db` /
  `ktunnel_ed25519.pem` is created — those are only needed by the Relay.
* Writes the unit file to `/etc/systemd/system/ktunnel.service`, with the arguments
  after `-install` baked verbatim into `ExecStart=`, then runs
  `systemctl daemon-reload` and `systemctl enable ktunnel` (auto-start at boot).
* `-secret-file` is a **runtime** flag: it is replayed on every start and the secret
  is read from the file each time, so the secret itself never enters the unit. (`-s`
  is still accepted but would land in `ExecStart=` — prefer `-secret-file`.)
* `Install` only *registers* the service — it does **not** start it. Start it in §7.

The generated unit looks like this (`Type=notify`; KService sends `sd_notify
READY=1` once the tunnel loop is entered):

```ini
# generated by dekaf2 KService — do not edit by hand
[Unit]
Description=Secure reverse tunnel
After=network.target

[Service]
Type=notify
NotifyAccess=main
ExecStart=/usr/local/bin/ktunnel -relay tunnel.example.com -p 443 -n linux-outlet -secret-file /etc/ktunnel/secret -aes -trust-fingerprint a1:b2:c3:...:ff --service
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

If you run the service under a dedicated user (below), make the secret file readable
by that user: `sudo chown ktunnel /etc/ktunnel/secret`.

### Optional: run under a dedicated unprivileged user

The service runs as **root** by default (there is no CLI flag for a run-as user).
For least privilege, create a system user and add `User=` to the unit after install:

```
sudo useradd --system --no-create-home --shell /usr/sbin/nologin ktunnel
sudo systemctl edit ktunnel        # add:  [Service]\n  User=ktunnel
sudo systemctl daemon-reload
```

An outlet only makes outbound connections and (with `-trust-fingerprint`)
writes nothing, so an unprivileged account is sufficient.

---

## 6. User-scope install (no root)

If you cannot use root, run `-install` as your normal user. KService automatically
falls back to a **user-scope** unit and prints a hint on stderr:

```
ktunnel -install -relay tunnel.example.com -p 443 -n linux-outlet -secret-file ~/.config/ktunnel/secret -aes -trust-fingerprint "a1:b2:c3:...:ff"
```

* Unit path: `~/.config/systemd/user/ktunnel.service` (`WantedBy=default.target`).
* Manage it with `systemctl --user …` instead of `sudo systemctl …`.
* `-install` also enables **lingering** for your account, so the service survives
  logout and starts at boot without a login session. Should that fail (hardened
  polkit), it prints the manual command: `sudo loginctl enable-linger $USER`.
  Note that lingering is a per-user setting — it affects all enabled user services
  of the account; revert with `loginctl disable-linger`.
* The generated unit is a base — add hardening or other overrides with
  `systemctl --user edit ktunnel`; drop-ins survive `-install` / `-uninstall` cycles.

---

## 7. Start and verify the service

System scope:
```
sudo ktunnel -start                 # or: sudo systemctl start ktunnel
sudo ktunnel -status                # or: systemctl status ktunnel
sudo journalctl -u ktunnel -f       # follow the tunnel's log output
```

User scope:
```
ktunnel -start                      # or: systemctl --user start ktunnel
systemctl --user status ktunnel
journalctl --user -u ktunnel -f
```

On connection loss the Outlet retries at the `-timeout` interval; if the
process itself exits non-zero, systemd restarts it after `RestartSec=5`.

---

## 8. Firewall

The Outlet connects **outbound only** and does not listen on any port.
Most host firewalls permit outbound traffic by default → usually **no inbound rule
is needed**. Only if egress is filtered, allow outbound TCP to the relay,
e.g.:

```
# firewalld
sudo firewall-cmd --permanent --direct --add-rule ipv4 filter OUTPUT 0 \
  -p tcp --dport 443 -j ACCEPT
sudo firewall-cmd --reload

# nftables (in your output chain)
tcp dport 443 accept
```

---

## 9. Operation / maintenance

| Task      | Command (system scope)                                   |
| --------- | -------------------------------------------------------- |
| Stop      | `sudo ktunnel -stop`  or `sudo systemctl stop ktunnel`   |
| Status    | `sudo ktunnel -status`  or `systemctl status ktunnel`    |
| Logs      | `sudo journalctl -u ktunnel -f`                          |
| Uninstall | `sudo ktunnel -uninstall`  (disables + removes the unit) |
| Update    | stop → replace binary → start                            |

If the command line changes (relay, port, name, secret, fingerprint), either
`ktunnel -uninstall` and reinstall with the new arguments, or edit `ExecStart=` in
the unit file and run `systemctl daemon-reload` + restart. `systemctl stop ktunnel`
triggers a clean disconnect within milliseconds (SIGTERM is routed to the tunnel's
shutdown handler).

---

## 10. Security notes

* **Keep the secret out of the unit:** prefer `-secret-file /etc/ktunnel/secret`
  (mode `0600`, owned by the service account) over `-s <secret>`. With `-s` the
  secret is baked into `ExecStart=` in `/etc/systemd/system/ktunnel.service`, which
  is world-readable by default. Either way it is the **outlet password**, not the
  admin password; rotate it on the relay side if compromised (then update the
  file and `systemctl restart ktunnel`).
* **Identity pinning:** `-aes -trust-fingerprint` adds protection, on top of TLS,
  against an active TLS-intercepting middlebox — even where the local TLS trust store
  itself cannot be trusted (the Ed25519 pin does not rely on the certificate PKI).
  Note that the outlet does **not** verify the relay's TLS certificate
  by default, so with a self-signed relay cert the fingerprint pin is what actually
  authenticates the peer — use `-aes` on any untrusted path.
* **Least privilege:** run under a dedicated system user (§5) rather than root.
