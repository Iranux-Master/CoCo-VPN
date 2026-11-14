# Plan: Batch account creation off the GUI thread, with progress and Cancel (S2 / KI-03)

_Written by the `planner` agent on 2026-11-14 (first run, also its smoke test). A proposal: nothing here is implemented._

## Goal
Creating 1 to 100 accounts on the Share page stops freezing the window: accounts are built one after another on a worker thread, the progress text ("Creating accounts: %1 of %2") updates live, and a new **Cancel** button stops the batch after the step in flight. Partial groups already saved stay saved; the finish message says how many succeeded, failed and were cancelled. Out of scope: moving persistence and rendering out of `ExportUiController` (S3 / KI-04), parallel protocols, aborting an SSH call that has already started, and keeping groups in step with revokes (S7).

## Current state
- `client/ui/controllers/selfhosted/exportUiController.cpp:114` `startAccountBatch` validates (count 1–100), sets `m_batch*` members, then `QTimer::singleShot(50, …createNextBatchAccount)`.
- `:147` `createNextBatchAccount` builds one account × one protocol per tick by calling `ExportController::generate{OpenVpn,WireGuard,Awg,Xray,Connection}Config` **synchronously on the GUI thread** (one SSH round trip each), updates `m_batchGroup`, calls `persistBatchGroup()` (`:217`, writes `Sharing/accountGroups` and emits `accountGroupsChanged`), emits `batchProgressChanged`, and at the end `accountBatchFinished(succeeded, failed)`.
- Header (`exportUiController.h:26-28, 92, 95, 121-131`): properties `batchProgress`, `batchTotal`, `batchRunning`; no cancel API.
- `core/controllers/selfhosted/exportController.cpp` is effectively worker-safe: each call creates its own `SshSession`, reads `SecureServersRepository` / `SecureAppSettingsRepository` (`SecureQSettings` takes a `QMutex`), and QR codes are strings (`qrCodeUtils`). Side effect: it emits `appendClientRequested` (`:89`, `:171`), connected in `CoreSignalHandlers::initExportControllerHandler` (`core/controllers/coreSignalHandlers.cpp:138`) with context `this` on the main thread, so from a worker it becomes a queued call to `UsersController::appendClient` on the GUI thread (correct, but ordering relative to the batch changes).
- Precedent: `InstallUiController` (`installUiController.cpp:326-353`) uses `QtConcurrent::run` + `QFutureWatcher<ErrorCode>`; `coreSignalHandlers.cpp:148` already runs `revokeClient` through `QtConcurrent::run`.
- `PageShare.qml:866-888` has the start button and progress text; `:103` handles `onAccountBatchFinished`.
- The `graphify` branch does not exist on origin yet, so targeted grep was used.
- Doc drift: KI-03's hint should name `InstallUiController`; `ARCHITECTURE.md` and `CHANGE_MAP.md` describe the timer flow this replaces.

## Design
- **New core class `AccountBatchRunner`** (`client/core/controllers/selfhosted/accountBatchRunner.{h,cpp}`, CoCo-owned). It owns the batch state now in `m_batch*` and runs **one step at a time** in a worker: each step is `QtConcurrent::run(…)` returning `ExportResult`, watched by a `QFutureWatcher` whose `finished` slot runs on the GUI thread. Everything not thread-safe stays on the GUI thread: `QVariantMap` group updates, `tr()` / `errorString`, persistence. This is the first piece of KI-04's `AccountBatchController`, so S3 then only moves persistence.
- **API:** `start(const BatchRequest&)`, `cancel()`, `running()`, `progress()`, `total()`; signals `progressChanged()`, `groupUpdated(QVariantMap group)` (the UI controller persists it), `finished(int succeeded, int failed, bool cancelled)`.
- **Threading:** one step in flight; `std::atomic_bool m_cancelRequested` checked before scheduling each step. Cancel marks the current group `status: "cancelled"` if it has methods, otherwise drops it, then emits `finished(…, true)`. Lambdas capture `ExportController*` and value copies, never `this`. The runner is a child of `ExportUiController`; on destruction / quit it sets cancel and calls `m_watcher.waitForFinished()` so the worker never touches freed objects.
- **ExportUiController** becomes a thin adapter: `startAccountBatch` keeps its validation and forwards; new `Q_INVOKABLE void cancelAccountBatch()`; properties read from the runner; `groupUpdated` → `persistBatchGroup(group)`; signal `accountBatchFinished(int succeeded, int failed, bool cancelled)` (QML ignores extra arguments).
- **QML:** a Cancel button (`qsTr("Cancel batch")`) visible only while `batchRunning`, showing "Cancelling…" and disabled while a cancel is pending; a cancelled variant of the finish notification; Persian strings in `amneziavpn_fa_IR.ts`; buttons in a `RowLayout` so `LayoutMirroring` flips them for RTL.

## Slices (one PR each, in order)
### 1. Worker-based batch runner with Cancel  (size M, depends on: -)
- Files: `client/core/controllers/selfhosted/accountBatchRunner.{h,cpp}` (new); `client/cmake/sources.cmake` (add both next to `exportController.*`); `exportUiController.{h,cpp}` (delegate to the runner, remove the `QTimer` loop and `m_batch*`, add `cancelAccountBatch`, new `accountBatchFinished` signature); `Pages2/PageShare.qml` (Cancel button, cancelled message); `translations/amneziavpn_fa_IR.ts`.
- Steps:
  1. Move the per-container dispatch (`exportUiController.cpp:167-179`) into a private static `runStep(ExportController*, serverId, containerIndex, name)` in the runner.
  2. Move the group-building logic (`:157-213`) unchanged into the runner's watcher slot, keeping the name suffix rule and the length cap.
  3. Add cancel and wait-on-destruction as in Design.
  4. Rewire the adapter and QML; run `lupdate` for `fa_IR` only and add the translations.
  5. `python3 -I tools/codemap/build_codemap.py` (the QML API changes).
- Tests: new `client/tests/selfHostedAdmin/testAccountBatchRunner.cpp`, modelled on `testMultipleExports.cpp`. If injecting a step is too invasive, give `AccountBatchRunner` a constructor taking a `std::function<ExportResult(int container, QString name)>` step. Assert: (a) the progress/total sequence and `finished(2, 0, false)` for 2 accounts × 1 protocol; (b) a failing step yields `status: "failed"` and is counted; (c) `cancel()` during step 1 of 3 runs no further step and gives `finished(…, true)`; (d) the step's thread is not `qApp->thread()`. Manual Windows check on a real server, 20 accounts × 2 protocols: the window stays responsive (drag, scroll), the counter updates, Cancel stops within one step, the created groups appear and are shareable, the server's users list shows the created clients; repeat in Persian (RTL).
- Docs to update: `ARCHITECTURE.md` (flow), `docs/ai/CHANGE_MAP.md` (add the runner), `KNOWN_ISSUES.md` (delete KI-03, narrow KI-04), `docs/ai/TESTING.md` (new test), `CODE_MAP.md` (regenerated), `CHANGELOG.md` (Persian, "not built in the cloud session" if true), `ROADMAP.md` (S2 done, S3 scope).
- Done when: no `QTimer` in the batch path; no `generate*Config` call on the GUI thread; Cancel works; the new test passes locally or in CI; the code map check is clean.

## Risks
- **Queued `appendClientRequested` from the worker** (likely, low impact): `UsersController::appendClient` now runs slightly later on the GUI thread. Mitigation: confirm the `connect` context is on the main thread (it is, `coreSignalHandlers.cpp:138`) and check the users list in the manual test.
- **Concurrent use of the same server** (medium, medium): a single export or revoke during a batch means two SSH sessions editing one clients table. Mitigation: disable single-export actions while `batchRunning` (owner decision below).
- **Shutdown during a step** (low, high: crash): `waitForFinished` in the destructor can block quit for up to one SSH timeout; accept and document.
- **Cancel cannot interrupt an in-flight SSH call** (certain, low): the UI shows "Cancelling…" until the step returns.
- **Upstream merge** (low): only CoCo code and new files; `exportController.*` untouched.
- **Windows-only testing** (certain): the owner does the Windows check; say "not built" if no build ran.

## Owner decisions
| Question | Options | Recommendation | Blocks slice |
|---|---|---|---|
| The half-built account on Cancel | Keep it as "cancelled" (shareable methods) / delete locally / revoke its clients on the server | Keep it, marked cancelled; revoking is S7 | 1 (default can proceed) |
| Block single exports and revokes while a batch runs? | Block in the UI / allow | Block (disable the buttons while `batchRunning`) | 1 |
| Make S2 the start of S3 (runner in `client/core/`)? | Yes / keep S2 inside `ExportUiController` | Yes; S3 shrinks to persistence and rendering | 1 |

## Requests to webpack
None.

## Open questions for the implementer
- Are `ConfiguratorBase::create` and the protocol configurators free of GUI-thread-only objects (no `QPixmap`, no main-thread parent `QObject`)? See `client/core/configurators/*`.
- Does `UsersController::appendClient` assume it runs synchronously before the next export? See `client/core/controllers/selfhosted/usersController.cpp`.
- Does `client/cmake/sources.cmake` use a glob or an explicit list for `exportUiController.cpp`? Add the runner the same way.
- Do the tests in `client/tests/selfHostedAdmin/CMakeLists.txt` link the whole client core, so a new test target just works?
