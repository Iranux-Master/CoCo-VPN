# Roadmap

Updated: 2026-11-14. Suggested order of work; nothing here is implemented unless `CHANGELOG.md`
says so. Each item is sized for one branch and one PR. P0 is "before any public release", P1 the
next product step, P2 later. Owner decisions are listed in §4 and block the items that name them.

## 1. Where the project is

- A CoCo-branded fork of Amnezia VPN Client 5.0.3.1 with batch account creation, share templates
  (HTML / TXT / QR, Persian guides), Persian and English UI, and a Windows CI installer.
- Tested so far: static review and the Windows CI build; the new account and template features are
  waiting for a hands-on Windows check (`CHANGELOG.md`, 2026-10-08).
- Since 2026-11-14: AI documentation, the code map and the Graphify graph (this PR).

## 2. Stabilize (P0)

| # | Item | Why | Docs |
|---|---|---|---|
| S1 | Windows hands-on test of batch accounts, templates and sharing; fix what it finds | Every CoCo feature is untested in the app | `CHANGELOG.md` |
| S2 | Move the batch off the GUI thread, with progress and Cancel | KI-03 | plan: `docs/plans/S2-batch-off-gui-thread.md` |
| S3 | Extract account groups, template rendering and batch orchestration from `ExportUiController` into `client/core/`, with Qt tests for rendering and file names | KI-04, first CoCo tests | `ARCHITECTURE.md` §7 |
| S4 | Decide and apply the Amnezia Premium / Free policy: hide those pages and the gateway calls in CoCo builds (a CMake option such as `COCO_DISABLE_AMNEZIA_API`), until webpack replaces them | KI-01, decision O1 | |
| S5 | CoCo update feed: a signed manifest and the update controller reading it | KI-02, request 3 in `docs/integration/WEBPACK.md` §7 | |
| S6 | CI: a test job, and `deploy.yml` trimmed to what CoCo ships (or disabled) | KI-06, KI-07 | `docs/ai/TESTING.md` |
| S7 | Keep account groups in step with the server: mark a method revoked when the client is revoked, and offer "check all accounts" | KI-05 | |

## 3. Features

### 3.1 webpack.ir integration (P1)

Phases W0 to W6 in `docs/integration/WEBPACK.md` §6: API client, sign-in (password, Google),
servers bought from webpack with metrics and power actions, "Set up access" into the existing install
wizard, notices and renewal, buying a server, and replacing the Amnezia service pages.

### 3.2 Account management for VPN resellers (P1)

CoCo's users share accounts with other people, so the account tools are the product's core.

| # | Feature | Notes |
|---|---|---|
| A1 | Account list across servers: search, filter by server, protocol, status, created date | builds on account groups |
| A2 | Expiry date and traffic or device notes per account, with a reminder list ("expires this week") | local first; server-side limits later |
| A3 | Per-account traffic from the server (WireGuard / AWG `wg show` transfer counters, XRay stats API) | needs a server script and parsing; read-only |
| A4 | Disable / enable an account without deleting it (remove the peer, keep the keys) | server scripts per protocol |
| A5 | Bulk actions: revoke, renew, re-share a selection | after A2, A4 |
| A6 | Export / import account groups (encrypted file) between devices of the same operator | S7 first |
| A7 | Subscription link per account (one URL that lists the account's configs, for apps that read subscriptions) | needs hosting: webpack request or the operator's server |

### 3.3 Sharing (P1)

| # | Feature | Notes |
|---|---|---|
| H1 | Share page hosted on a link (one-time or expiring), instead of sending an HTML file | webpack-side storage, or a small static host on the VPS |
| H2 | Telegram-friendly output: one message per account with the QR as an image | template variant |
| H3 | Template variables for expiry, traffic and support contact | after A2 |
| H4 | Template gallery: import / export templates as files | |

### 3.4 Server operations (P2)

| # | Feature | Notes |
|---|---|---|
| O1 | Server health card: CPU, memory, disk, traffic (from webpack for webpack servers, over SSH for others) | F4 in the integration doc |
| O2 | Protocol health check: is each container running, ports reachable from outside | uses existing `check_server.sh` patterns |
| O3 | Recommended protocol per network (AWG for UDP-friendly, XRay REALITY when UDP is blocked) shown in the wizard | content plus a reachability probe |
| O4 | Server migration: move all accounts to a new server (recreate, then re-share) | after A4, webpack F3 |
| O5 | Scheduled backup of server-side client tables | |

### 3.5 Client app (P2)

| # | Feature | Notes |
|---|---|---|
| C1 | Android build with the CoCo identity (package id, icons, store listing) | KI-09 |
| C2 | macOS and Linux builds with the CoCo identity | KI-09 |
| C3 | Persian onboarding and in-app help that matches the share-page guides | |
| C4 | Diagnostics bundle (logs without secrets) to send to support | logger already exists |

## 4. Owner decisions

| ID | Question | Blocks |
|---|---|---|
| O1 | Hide Amnezia Premium / Free in CoCo, or keep it as an option? | S4, W6 |
| O2 | Where is the CoCo update manifest hosted (`iranux.site` or webpack.ir), and who signs it? | S5 |
| O3 | Does CoCo ask for the server's login (sudo) password on webpack servers, or does webpack add a device sudo rule? Proposed: CoCo sets the password at purchase and installs a NOPASSWD sudoers file (`docs/plans/W5-buy-vps-in-app.md`) | W3, W5 |
| O4 | Which platforms ship after Windows, and in which order? | C1, C2, S6 |
| O5 | Is "account" in CoCo a local record only, or should it become a webpack entity (synced, with expiry and billing)? | A2, A6, A7, H1 |
