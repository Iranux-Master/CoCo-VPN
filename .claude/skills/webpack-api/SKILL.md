---
name: webpack-api
description: Call webpack.ir (Iranux-Bash-Runner) /api/v1 from the CoCo VPN client - device sign-in, servers, SSH access, metrics, orders, wallet, notifications. Use for any work on the webpack integration (docs/integration/WEBPACK.md).
---

# Work on the webpack.ir integration

1. Read `docs/integration/WEBPACK.md` (design, flows F1 to F6, phases W0 to W6, security rules).
2. Check the live contract in the webpack repository, which is **read-only** from CoCo work:
   - attach it read-only and read `docs/ai/API_REFERENCE.md` (§1 conventions, §3.2 VPS, §3.3 wallet,
     §3.4a notifications, §3.4b device sessions);
   - DTO shapes: `BashScriptRunner.Api.Contracts/*.cs`;
   - the reference client to port: `BashScriptRunner.Client/IWebpackApi.cs`, `WebpackApiClient.cs`,
     `GatewayTransport.cs`, `Session/DeviceSessionManager.cs`, `Session/LoopbackReceiver.cs`, and
     `BashScriptRunner.Ui/Services/PlatformServerDirectory.cs` (`SetUpAccessAsync`).
   If the contract differs from `WEBPACK.md`, the webpack code wins: update `WEBPACK.md` in your PR.
3. Code goes in `client/core/controllers/webpack/` (client, session, servers, shop), models in
   `client/core/models/webpack/`, the QML adapter in `client/ui/controllers/webpack/`, settings keys
   under `Webpack/` through `SecureAppSettingsRepository`. Do not extend `GatewayController` (it is
   Amnezia's encrypted gateway).
4. Rules: HTTPS only; send `X-Correlation-ID`; parse the `ApiResult` envelope; refresh once on 401;
   back off on 429; keep only the refresh token and profile (never the password or access token);
   never log tokens, keys or the `Authorization` header; pin host keys from `/access`; keep order
   idempotency keys until a definite answer.
5. Never push to, open PRs on, or file changes in the webpack repository from a CoCo task. Add what
   CoCo needs to `WEBPACK.md` §7.
6. Tests: the envelope parser, the token refresh state machine and the PKCE / loopback parser are
   pure and get Qt tests (`client/tests/`), with a fake `QNetworkAccessManager` reply; no test calls
   the real site.
