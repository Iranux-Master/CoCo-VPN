# Build, Test and CI

## Build

| Target | Command | Notes |
|---|---|---|
| Windows installer | `deploy\build.bat --installer ifw` | What CI runs. Needs MSVC 2022 x64 (`ilammy/msvc-dev-cmd`), Qt 6.10.1 `win64_msvc2022_64` with `qtremoteobjects qt5compat qtshadertools` and `tools_ifw`, Conan 2.28 (`pip install "conan==2.28.0"`), `QT_INSTALL_DIR` set. Output `deploy/build/CoCoVPN_*_windows_x64.exe`. |
| Linux / macOS | `deploy/build.sh` | `-b <dir>` build folder (default `deploy/build`), `-t <target>`, `-f` clean, `--installer <name>` |
| Android APK | `deploy/build.sh -t android --sign --abi arm64-v8a` | What `android-build.yml` runs on `ubuntu-latest`: Qt 6.10.1 `linux_gcc_64` + `android_arm64_v8a`, NDK 27.0.11718014, Java 17, Conan 2.28, `QT_ANDROID_KEYSTORE_*` set. Output `deploy/build/client/android-build/*.apk`, uploaded as `CoCoVPN_<version>_android_arm64-v8a_<release|test>-signed.apk`. |
| iOS / macOS NE | upstream scripts (`deploy.yml` jobs) | Not exercised by CoCo yet. |

The first build compiles the Conan recipes in `recipes/` (OpenSSL, libssh, OpenVPN, awg, xray
bindings, ...), which takes long; later builds reuse the Conan cache. Run
`git submodule update --init --recursive` first.

## Tests

Qt Test executables under `client/tests/`, registered with CTest when the build is configured with
`AMNEZIA_BUILD_TESTS=ON` (environment variable read by both build scripts):

```bash
AMNEZIA_BUILD_TESTS=ON deploy/build.sh
ctest --test-dir deploy/build --output-on-failure            # all
ctest --test-dir deploy/build -R XraySerializationTest       # one
```

| Test (CTest name) | Covers |
|---|---|
| `MultipleImportsTest`, `ConfigFormatVersionTest`, `XraySerializationTest`, `BackupExportImportTest` | Import of keys, files and links; config format versions; vless / vmess / ss serialization; settings backup |
| `ServerEditTest`, `ServerRenameTest`, `DefaultServerChangeTest`, `ServerEdgeCasesTest`, `ServersModelSyncTest`, `SignalOrderTest`, `SettingsSignalsTest` | `SecureServersRepository`, `ServersModel`, signal ordering |
| `Ui*ModelAndControllerTest` (servers, language, IP and app split tunnelling, allowed DNS, API services, news) | UI controller + model pairs |
| `ConnectionTest`, `OpenSSLLibraryTest` | Connection state machine, OpenSSL linkage |
| `AdminSelfHostedExportTest`, `SelfHostedServerSetupTest`, `MultipleExportsTest` (`client/tests/selfHostedAdmin/`) | Real SSH install and export against a live VPS. Need `SELF_HOSTED_SERVER_HOST_NAME`, `SELF_HOSTED_SERVER_SSH_PORT`, `SELF_HOSTED_SERVER_USER_NAME`, `SELF_HOSTED_SERVER_PASSWORD` (and `SELF_HOSTED_ADMIN_EXPORT_ROUND_TRIP_VPN_KEY` for the round trip). Never put these values in the repository. |
| `WebpackApiTest`, `WebpackKeysTest`, `WebpackShopTest` (`client/tests/webpack/`) | webpack.ir client without network (`fakeNetwork.h`): envelope parsing, base-URL rules, token refresh, PKCE and the loopback parser, the device key (compared with `ssh-keygen` when installed), plans, shortfall rounding, checkout, balance polling, the order key kept until a definite answer, build tracking. The sudo setup and host-key pin need a real sshd and are not in CTest. |
| `DnsttUtilsTest` (`client/tests/dnstt/`) | DNS tunnel rules (`core/utils/dnstt/dnsttUtils`): tunnel-domain validation and the NS / glue records, MTU and key checks, resolver specs (`udp:` / `doh:` / `dot:`, IP only), dnstt-client arguments and the XRay config rewrite to the local dnstt port. The container, `DnsttProtocol` and a real tunnel are not covered. |
| Android (`run_android_tests.sh`) | OpenSSL and backup restore on a device or emulator; not registered with CTest |

**Blind spots.** No test covers the webpack pages and `WebpackUiController`, or the share code: batch account creation, account groups, share
templates and their rendering (`ExportUiController`). The tag-replacement and file-name logic is pure
and is the first candidate for a test once it moves out of the UI controller (KNOWN_ISSUES KI-04).

## CI

| Workflow | Trigger | What it does |
|---|---|---|
| `windows-build.yml` | push to `main`, manual | Builds the Windows IFW installer and uploads it as an artifact (1 day). Tests are off (`AMNEZIA_BUILD_TESTS: OFF`). |
| `android-build.yml` | push to `main` and pull requests touching Android, UI or core code; manual | Builds a signed arm64-v8a APK (7 days). Signs with `ANDROID_RELEASE_KEYSTORE_BASE64` / `_KEY_ALIAS` / `_KEY_PASS` when set, otherwise with a throwaway key made in the run. |
| `deploy.yml` | push to any branch except `main` / `graphify`, unless only docs or AI tooling changed | Upstream's full multi-platform build and test matrix. Fails on CoCo branches today (missing secrets and self-hosted runners such as `android-runner`); see KNOWN_ISSUES KI-06. |
| `tag-deploy.yml`, `tag-upload.yml` | tags / manual | Upstream release jobs, not adapted to CoCo. |
| `ai-docs.yml` | pull requests touching `client/` or the code map | `tools/codemap/build_codemap.py --check` |
| `graphify.yml` | push to `main` touching source | Rebuilds the Graphify graph and publishes it to the `graphify` branch |

## What an agent can check without a build

- `python3 -I tools/codemap/build_codemap.py --check`
- `bash -n client/server_scripts/<container>/*.sh` for edited server scripts
- That a new QML file is listed in `client/ui/qml/qml.qrc` and a new HTML part in `client/images/images.qrc`
- That new strings have a `fa_IR` entry
