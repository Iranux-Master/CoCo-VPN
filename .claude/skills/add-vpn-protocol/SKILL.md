---
name: add-vpn-protocol
description: Add or change a VPN protocol / server container in CoCo VPN end to end - container enum, installer, configurator, server scripts, device-side protocol, settings model and page, sharing. Use when touching client/server_scripts, core/installers, core/configurators or core/protocols.
---

# Add or change a protocol / container

A protocol has two halves: the **server side** (a Docker container on the VPS, installed over SSH)
and the **device side** (a `VpnProtocol` driver that connects). Study the closest existing one first:
AWG (`awg`) for WireGuard-like UDP protocols, XRay (`xray`) for TLS / REALITY style ones. Use the
Graphify graph to see what the existing one touches:
`graphify affected "AwgConfigurator"` (see `tools/graphify/README.md`).

## Server side

1. `client/core/utils/containerEnum.h` (`DockerContainer`) and `protocolEnum.h` (`Proto`), plus the
   string and default-port mappings in `client/core/utils/containers/containerUtils.cpp` (names,
   descriptions, detailed descriptions; CoCo uses neutral names such as "AWG", not "AmneziaWG").
2. `client/server_scripts/<name>/`: `Dockerfile`, `configure_container.sh`, `run_container.sh`,
   `start.sh`, templates. Add them to `client/server_scripts/serverScripts.qrc`. Values come in as
   `$VAR` placeholders; define them in `client/core/utils/selfhosted/scriptsRegistry.cpp`
   (`genBaseVars` or a new `gen<Name>Vars`). Never concatenate user input into a command.
3. `client/core/installers/<name>Installer.{h,cpp}` (derive from `InstallerBase`) and add a case to
   `InstallController::createInstaller` (`installController.cpp`).
4. `client/core/configurators/<name>Configurator.{h,cpp}` (derive from `ConfiguratorBase`, add it to
   `ConfiguratorBase::create`): creates a client on the server and returns the client config; keep the server's `clientsTable` in step via
   `usersController`.
5. `bash -n` every edited script.

## Device side

6. `client/core/protocols/<name>Protocol.{h,cpp}` (derive from `VpnProtocol`), created in
   `VpnProtocol::factory` (`vpnProtocol.cpp`). Desktop privileged steps go through `IpcInterface` (`ipc/ipc_interface.rep`,
   implemented in `ipc/ipcserver.cpp`); Android in `client/android/`, iOS / macOS NE in `client/ios`.
7. Settings model `client/ui/models/protocols/<name>ConfigModel` registered in `coreController.cpp`,
   and a settings page (skill `add-qml-page`).

## CoCo sharing

8. Batch accounts and templates: `ExportUiController::createNextBatchAccount` chooses the export call
   per container; `protocolKeyForMethod` / `protocolDisplayName` map it to a template protocol; add a
   guide part in `client/ui/share/<name>.html` and its alias in `client/images/images.qrc`, and a
   system template in `addSystemShareTemplates`.
9. Import: links and files in `core/controllers/selfhosted/importController.cpp` and
   `core/utils/serialization/` with a test in `client/tests/`.

## Finish

- New sources in `client/cmake/sources.cmake`; Persian strings; `python3 -I tools/codemap/build_codemap.py`.
- `CHANGELOG.md` entry stating what was and was not tested (a real server install needs the
  `SELF_HOSTED_SERVER_*` test variables, `docs/ai/TESTING.md`).
