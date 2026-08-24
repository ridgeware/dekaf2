# Installing ktunnel as a *Relay* on Linux

This guide describes installing the dekaf2 sample app **ktunnel** on a
publicly-reachable Linux machine acting as the **Relay**: it listens for the
outbound control connections from one or more **Outlets** (which run inside the
target networks, behind firewalls), listens on one or more forward ports — and,
optionally, accepts **Inlets** — and multiplexes that traffic through the
outlets to the targets they can reach.

The Relay is installed as a **systemd** unit and runs in **Stateful mode**: a
small SQLite database holds the admin login, the outlet and inlet accounts, and
the tunnel definitions, all managed through a built-in web admin UI.

---

## 1. Roles in brief

```
       e.g. port 1234           e.g. port 443
                                      |
 app   >>--TCP/TLS-->> ktunnel <<--TLS-WS--<< ktunnel >>--TCP/TLS-->> target(s)
                       (relay)        |       (outlet)
                             firewall |
```

* **Relay** – *this Linux host*. Publicly reachable. Terminates the tunnel
  control connections, serves the admin UI, and either listens on forward ports
  itself or hands traffic to a tunnel that an inlet asked for by name.
* **Outlet** – runs elsewhere inside the target network, connects **outbound**
  to this host and dials the actual targets.
* **Inlet** – optional, operator-side: connects outbound to this host too and
  offers local ports that map to named tunnels (the `ssh -L` equivalent), so a
  tunnel needs no publicly reachable forward port at all.

A ktunnel is a Relay precisely when `-relay` (the former `-e`/`-exposed`) is
**absent** — it then listens instead of connecting out.

---

## 2. Ports and modes

* **Control / admin HTTPS port** (`-p`, default **443**): outlets and inlets
  connect here (`/Tunnel` resp. `/Inlet`, upgraded to WebSocket), and the admin
  UI lives here (`/Configure/`). Must be reachable from the outlets/inlets (and
  from wherever you administer it).
* **Forward ports**: one per tunnel, defined in the admin UI (listen port →
  target host:port, owned by an outlet). These are where downstream *clients*
  connect directly. Each needs an inbound firewall rule — unless the tunnel is
  reached exclusively through an inlet, in which case it can be bound to
  `127.0.0.1` and needs no rule.
* **Stateful vs AdHoc**: launched by systemd, ktunnel always runs **Stateful**
  (DB-backed, admin UI). `-f`/`-t`/`-s` are rejected in service mode — tunnels
  and outlet/inlet accounts are configured through the DB / admin UI, not the
  CLI. AdHoc mode (`-f`/`-t` on an interactive CLI) exists only for recovery
  (see §9).

---

## 3. Prerequisites

* A `ktunnel` binary built for this Linux distro (or a dekaf2 install that ships it),
  built with Ed25519 support if you want `-aes` identity pinning.
* A public DNS name / IP reachable on the control port and the forward ports.
* **root** (a public listener on 443 and machine-wide state belong to a system
  service). Non-root falls back to a user-scope unit, but then the state lives under
  `~/.config/ktunnel/` and the service stops with your session unless you enable
  lingering — not recommended for a public endpoint.

---

## 4. Place the binary

```
sudo install -m 0755 ktunnel /usr/local/bin/ktunnel
```

Use a machine-wide path (`/usr/local/bin` or `/opt/ktunnel/`), not `/home/...`: on
SELinux-enforcing distros systemd may be denied execution from a home directory, and
the path is not guaranteed to be mounted early at boot. If you keep it under a custom
`/opt` path, restore/label its SELinux context (`restorecon -v` or a `bin_t` rule).

---

## 5. Install as a systemd service

`-install` on a Relay does two things in one step: it **bootstraps the config
DB** (admin login + Ed25519 server identity), then **registers the systemd
unit**.

### Interactive install (prompts for the admin password)

```
sudo /usr/local/bin/ktunnel -install -p 443 -aes -persist
```

You will be prompted for the admin **name** (default `admin`) and **password**. On
success it prints the server's Ed25519 **fingerprint** — copy it; the outlets and
inlets need it for `-aes -trust-fingerprint`.

### Non-interactive install (for provisioning)

```
printf '%s' 'AdminPassw0rd' | sudo tee /root/ktunnel-admin.pw >/dev/null
sudo /usr/local/bin/ktunnel -install -p 443 -aes -persist \
     -admin-user admin -pass-file /root/ktunnel-admin.pw
sudo shred -u /root/ktunnel-admin.pw
```

What the install does:

* **Config DB**: `/var/lib/ktunnel/ktunnel.db` (mode 0700, created for root). Holds
  the admin, outlet and inlet accounts, the tunnel definitions, and an `events`
  audit log. (For backward compatibility the outlet table is still named `nodes`
  on disk, and outlet event kinds are still `node_*`.)
* **Server identity**: `/var/lib/ktunnel/ktunnel_ed25519.pem` — the long-term
  Ed25519 key the server signs the `-aes` v2 handshake with. Created here so the
  fingerprint is available before first start.
* `-admin-user` / `-pass-file` are **bootstrap-only** and are stripped from the
  persisted unit; they never appear in `ExecStart=`.
* Writes `/etc/systemd/system/ktunnel.service` (arguments after `-install` baked into
  `ExecStart=`), then `systemctl daemon-reload` + `systemctl enable ktunnel`.
* `-install` only *registers* the service — it does **not** start it (see §7).

The generated unit (`Type=notify`; KService sends `sd_notify READY=1` once the server
is up):

```ini
# generated by dekaf2 KService — do not edit by hand
[Unit]
Description=Secure reverse tunnel
After=network.target

[Service]
Type=notify
NotifyAccess=main
ExecStart=/usr/local/bin/ktunnel -p 443 -aes -persist --service
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

> Re-running `-install` is idempotent for the admin row: if the admin table already
> has an entry it is left untouched (rotate later with `ktunnel -set-admin`).

---

## 6. TLS certificate

The control port is TLS. Three options:

* **Self-signed (default, with `-persist`)**: ktunnel auto-creates a self-signed cert
  and — with `-persist` — reuses it across restarts. This is fine for the tunnel
  itself, because **the outlet does not verify the relay's TLS certificate**; peer
  authentication is done cryptographically by `-aes` fingerprint pinning. Browsers
  hitting the admin UI will show a cert warning.
* **Automatic via ACME** (recommended if admins use the web UI over the internet):
  ktunnel obtains and renews a Let's Encrypt certificate itself, proving domain
  ownership with the built-in tls-alpn-01 challenge on the control port — no
  certbot, no port 80, no restart on renewal:
  ```
  sudo ktunnel -install -p 443 -aes -persist \
       -acme tunnel.example.com -acme-contact mailto:admin@example.com
  ```
  Port 443 of the domain(s) must reach this host directly. The server starts with
  the self-signed cert and switches as soon as the certificate is issued; account
  key and certificate persist in the TLS config directory, and renewal runs in
  the background 30 days before expiry (or earlier when the CA requests it via
  ACME Renewal Information). Use `-acme-dir` to test against the Let's Encrypt
  staging directory first.
* **Externally managed certificate**: supply PEM files and add them to the
  install command:
  ```
  sudo ktunnel -install -p 443 -aes -persist \
       -cert /etc/ktunnel/fullchain.pem -key /etc/ktunnel/privkey.pem
  ```
  With a certbot-managed cert, point `-cert`/`-key` at the live files and restart
  the service after each renewal (e.g. a certbot deploy hook running
  `systemctl restart ktunnel`).

Either way, **use `-aes`** on any untrusted network path — it authenticates the
relay to the connecting peers and adds forward secrecy on top of TLS.

---

## 7. Start and verify

```
sudo ktunnel -start                 # or: sudo systemctl start ktunnel
sudo ktunnel -status                # or: systemctl status ktunnel
sudo journalctl -u ktunnel -f       # follow the log
```

Confirm the admin UI answers (self-signed → `-k`):

```
curl -k https://localhost:443/Configure/
```

Read back the fingerprint any time for handing to outlets and inlets:

```
sudo ktunnel -fingerprint
```

---

## 8. Post-install configuration

A fresh Relay has an admin login but no outlet accounts and no tunnels yet.

**a) Create an outlet account** for each target-network ktunnel (its `-n` / `-s`
credentials). From the CLI:

```
printf '%s' 'S3cr3t!' | sudo tee /root/outlet.pw >/dev/null
sudo ktunnel -add-outlet -outlet-name linux-outlet -pass-file /root/outlet.pw
sudo shred -u /root/outlet.pw
```

or add it in the admin UI under *Outlets*. The outlet then logs in with
`-n linux-outlet -secret-file <file>` (or `-s`). The `-add-node` / `-node-name`
spellings still work as aliases.

**b) Define the tunnels** in the admin UI at `https://<host>:443/Configure/`:
each tunnel is *listen port → target host:port*, owned by an outlet. Example: listen
on `8443`, target `10.0.0.5:443`, owner `linux-outlet` — a client connecting to
`<relay>:8443` is forwarded through that outlet's tunnel to `10.0.0.5:443` on the
target network. A tunnel can additionally be bound to a single interface and
restricted with an allow-list of source networks; bind it to `127.0.0.1` to make it
reachable only through an inlet. Changes are applied live (listeners are reconciled
without a restart); a port that collides with the admin port or another tunnel is
reported on the Tunnels page instead of crashing the service.

**c) Optionally create inlet accounts** under *Inlets* for operators who should
reach a tunnel from their own machine with `-L`. An inlet may be restricted to a
list of tunnel names.

**d) Hand each outlet**: the relay's DNS/IP, the control port, its outlet name +
secret, and the fingerprint from §7. The admin UI's *Outlets → Install* page
generates a ready-to-run setup script for exactly this.

---

## 9. AdHoc mode (recovery only)

For a quick tunnel without the DB / admin UI (e.g. over an SSH session when the UI is
unreachable), run interactively with a forward port, a target, and a shared secret:

```
ktunnel -p 443 -f 8443 -t 10.0.0.5:443 -s "S3cr3t!" -aes -persist
```

No SQLite file is opened, no admin UI, peer auth is the `-s` secret only. This is not
a service configuration — `-f`/`-t`/`-s` are rejected when launched by systemd.

---

## 10. Firewall

Unlike an outlet, the Relay **listens** and needs inbound rules:

* the control / admin port (`-p`, e.g. 443),
* every forward port you define in the admin UI (e.g. 8443) — except tunnels bound
  to `127.0.0.1` and reached only through inlets.

```
# firewalld
sudo firewall-cmd --permanent --add-port=443/tcp
sudo firewall-cmd --permanent --add-port=8443/tcp
sudo firewall-cmd --reload

# ufw
sudo ufw allow 443/tcp
sudo ufw allow 8443/tcp
```

Restrict the admin/control port with source filtering if only known outlets, inlets
and admins should reach it. A forward port can also be narrowed per tunnel with its
allow-list in the admin UI, without a firewall rule.

---

## 11. Operation / maintenance

| Task              | Command                                                           |
| ----------------- | ----------------------------------------------------------------- |
| Stop              | `sudo ktunnel -stop`  or `sudo systemctl stop ktunnel`           |
| Status            | `sudo ktunnel -status`  or `systemctl status ktunnel`             |
| Logs              | `sudo journalctl -u ktunnel -f`                                   |
| Rotate admin pw   | `sudo ktunnel -set-admin [-admin-user <name>]`                    |
| Add/rotate outlet | `sudo ktunnel -add-outlet -outlet-name <n> -pass-file <f>`        |
| Add/rotate inlet  | `sudo ktunnel -add-inlet -inlet-name <n> -pass-file <f>`          |
| Fingerprint       | `sudo ktunnel -fingerprint`                                       |
| Uninstall         | `sudo ktunnel -uninstall`  (leaves the DB in `/var/lib/ktunnel/`) |
| Update            | stop → replace binary → start                                     |

`-uninstall` removes the unit only; the DB and identity in `/var/lib/ktunnel/` are
kept so a reinstall keeps the same admins, outlets, inlets, tunnels, and fingerprint.
Back up `/var/lib/ktunnel/` to preserve both.

If you change the install-time arguments (`-p`, `-aes`, `-cert`, …), `-uninstall` and
reinstall, or edit `ExecStart=` and run `systemctl daemon-reload` + restart. Runtime
tunnel/outlet/inlet changes go through the admin UI, not the unit.

---

## 12. Security notes

* **Least privilege**: the service runs as **root** by default (needed to bind 443
  and own `/var/lib/ktunnel`). To run as a dedicated user, create one, `chown` the
  state dir, add `User=` via `systemctl edit ktunnel`, and grant the port
  (`AmbientCapabilities=CAP_NET_BIND_SERVICE` or a port ≥ 1024).
* **Admin UI exposure**: it is reachable on the control port. Put it behind source
  filtering and use a real TLS cert if admins reach it over the internet.
* **Identity key** `/var/lib/ktunnel/ktunnel_ed25519.pem` is the server's long-term
  identity — back it up and guard it (mode 0600). Rotating it changes the fingerprint
  and forces every outlet and inlet to update `-trust-fingerprint`.
* **Audit log**: the `events` table records logins, handshake failures, and tunnel
  lifecycle — inspect it via the admin UI or directly with `sqlite3`.
