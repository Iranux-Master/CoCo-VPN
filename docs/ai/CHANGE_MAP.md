# Change Map: where each kind of change goes

Each row lists the files that change together. Paths are under `client/` unless they start with
another top-level folder. After any row that touches QML or controller registration, run
`python3 -I tools/codemap/build_codemap.py`.

| Change | Files |
|---|---|
| New page | `ui/utils/pageEnum.h` (add `Page<Name>` to `PageEnum`), `ui/qml/Pages2/Page<Name>.qml`, `ui/qml/qml.qrc`, the caller's `PageController.goToPage(PageEnum.Page<Name>)`; strings in `translations/amneziavpn_fa_IR.ts`. Skill: `add-qml-page`. |
| New QML-facing controller | `core/controllers/<name>Controller.{h,cpp}` (logic), `ui/controllers/<name>UiController.{h,cpp}` (adapter), construct and `setQmlContextProperty` in `core/controllers/coreController.{h,cpp}`, signals in `core/controllers/coreSignalHandlers.cpp`, sources in `client/cmake/sources.cmake`. |
| New reusable control | `ui/qml/Controls2/<Name>Type.qml` + `qml.qrc`; colours from `ui/qml/Modules/Style/AmneziaStyle.qml`. |
| New protocol or server container | `core/utils/containerEnum.h` / `protocolEnum.h`, `core/utils/containers/containerUtils.cpp` (names, descriptions, ports), `core/installers/<name>Installer`, sources in `cmake/sources.cmake`, `core/configurators/<name>Configurator`, `core/protocols/<name>Protocol` (device side), `server_scripts/<name>/` + `server_scripts/serverScripts.qrc`, `core/utils/selfhosted/scriptsRegistry.cpp`, a `ui/models/protocols/<name>ConfigModel` and settings page, platform code (`platforms/*`, `android/`, `ios/`) when the device side is new. Skill: `add-vpn-protocol`. |
| Client config creation, revoke, rename | `core/controllers/selfhosted/exportController.cpp`, `usersController.cpp` (server `clientsTable`), the protocol's configurator; UI in `ui/controllers/selfhosted/exportUiController.cpp`, `Pages2/PageShare.qml`. |
| Batch accounts / account groups (CoCo) | `ui/controllers/selfhosted/exportUiController.{h,cpp}` (`startAccountBatch`, `createNextBatchAccount`, `persistBatchGroup`, `Sharing/accountGroups`), `Pages2/PageShare.qml`. |
| Share templates (CoCo) | `ui/controllers/selfhosted/exportUiController.cpp` (`addSystemShareTemplates`, `render*`, tag replacement, file names), `ui/share/*.html` + its entry in `images/images.qrc`, `Pages2/PageSettingsShareTemplates.qml`, `Pages2/PageShare.qml`. Download / guide links point to `https://iranux.site/cocoVPN/`. |
| Import formats | `core/controllers/selfhosted/importController.cpp`, `core/utils/serialization/*`, `ui/controllers/importUiController.cpp`, `Pages2/PageSetupWizardTextKey.qml` / `PageSetupWizardConfigSource.qml`; test `tests/testMultipleImports.cpp`. |
| Server install flow | `core/controllers/selfhosted/installController.cpp`, `core/utils/selfhosted/sshClient.cpp` / `sshSession.cpp`, `server_scripts/*.sh`, `Pages2/PageSetupWizard*.qml`. |
| Settings value | `core/repositories/secureAppSettingsRepository.{h,cpp}` (key and accessors), `core/controllers/settingsController`, `ui/controllers/settingsUiController`, a migration in `core/utils/migrations.cpp` if the key moves. |
| Privileged OS action (desktop) | `ipc/ipc_interface.rep` (slot), `ipc/ipcserver.{h,cpp}` (implementation in the service), `service/server/*` (router, kill switch), caller via `core/utils/ipcClient`. |
| Branding | `client/cmake/branding/common.cmake`, `platforms/windows/amneziavpn.rc.in`, `service/server/amneziavpn-service.rc.in`, `cmake/CPack.cmake`, `deploy/installer/*`, `deploy/data/*`, `images/`, `metadata/`. Internal identifiers stay `Amnezia*`. |
| Language list | `ui/controllers/languageUiController.cpp`, `ui/models/languageModel.cpp`; strings in `translations/amneziavpn_fa_IR.ts`. |
| Amnezia Premium / gateway | `core/controllers/api/*`, `core/controllers/gatewayController.cpp`, `core/repositories/secureAppSettingsRepository.cpp` (`gatewayEndpoint`), `core/utils/api/*`, `ui/controllers/api/*`, `ui/models/api/*`, `Pages2/PageSettingsApi*.qml`, `PageSetupWizardApi*.qml`. |
| webpack.ir integration (planned) | New `core/controllers/webpack/` (client, auth, servers), `core/models/webpack/`, settings keys under `Webpack/`, UI controller `ui/controllers/webpack/`, pages `Pages2/PageWebpack*.qml`. Design: `docs/integration/WEBPACK.md`. Skill: `webpack-api`. |
| Windows build | `.github/workflows/windows-build.yml`, `deploy/build.bat`, `cmake/*`. |
| Release notes | `CHANGELOG.md` (Persian, newest first, with a "وضعیت بررسی" line saying what was and was not tested). |
