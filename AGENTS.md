# AI Agent Entry Point

The first file every coding agent reads. It is short on purpose: the documents it routes to describe
the system well enough that you should not scan the source tree. Open source files only for the area
you are changing. When a document and the code disagree, the code wins: fix the document in the same
change.

## Reading order

Always read:

1. `AGENTS.md` (this file): rules and routing.
2. `ARCHITECTURE.md`: layout, runtime model, main flows, build, upstream relationship, rules.
3. `docs/ai/KNOWN_ISSUES.md`: the entries for the area you touch.

Then only what the task needs:

| Need | Document |
|---|---|
| Which QML page uses which controller; what each controller exposes to QML; page list | `docs/ai/CODE_MAP.md` (generated, `tools/codemap/build_codemap.py`) |
| C++ type relationships: who calls whom, clusters, what a change touches | Graphify graph on the `graphify` branch: `git fetch --depth=1 origin graphify && git show origin/graphify:graphify-out/GRAPH_REPORT.md`; queries in `tools/graphify/README.md` |
| Where a kind of change goes, and which files move together | `docs/ai/CHANGE_MAP.md` |
| Build, test, CI | `docs/ai/TESTING.md` |
| Connecting to webpack.ir (Iranux-Bash-Runner) services | `docs/integration/WEBPACK.md` |
| What comes next | `docs/ROADMAP.md` |
| What changed in CoCo (Persian, for the owner) | `CHANGELOG.md` |
| Product summary for users (Persian) | `README.md` |

Do not read `client/3rd/`, `recipes/`, `client/images/`, `client/translations/*.ts` (except `fa_IR`
when changing text), `metadata/` or `.github/workflows/deploy.yml` unless the task is about them:
they are large and rarely relevant.

## Product shape in five lines

- Qt 6.10 / C++17 / QML client in `client/`, privileged service in `service/`, IPC in `ipc/`.
- Fork of Amnezia VPN Client 5.0.3.1; internal names stay `Amnezia*` / `amnezia::` (do not rename).
- `CoreController` is the composition root; QML sees objects by context-property names that often
  differ from the class (`ExportController` = `ExportUiController`). See `docs/ai/CODE_MAP.md` §1.
- Self-hosted servers are managed over SSH with bash scripts in `client/server_scripts/` that run
  Docker containers per protocol.
- UI languages: Persian (RTL, primary) and English. Primary test platform: Windows.

## Rules

- **Language.** Code, comments, commit messages and these AI documents are in English. User-facing
  text is `qsTr()` with a Persian translation; `README.md` and `CHANGELOG.md` are Persian.
- **Layers.** New logic goes in `client/core/`; `client/ui/controllers/` stays a thin QML adapter;
  QML holds no logic or persistence (`ARCHITECTURE.md` §7).
- **Secrets.** Never log or commit SSH passwords, private keys, client configs, API keys or build
  secrets. Persist secrets only through `SecureQSettings`. The CI secrets (`PROD_AGW_PUBLIC_KEY`,
  `*_ENDPOINT`) stay in GitHub secrets.
- **Shell.** Never build a remote command from raw user input. Use a script in
  `client/server_scripts/` with `$VAR` placeholders (`scriptsRegistry`, `SshSession::replaceVars`)
  and validate the value first.
- **Upstream.** Keep upstream files' structure; put CoCo features in new files where possible
  (`ARCHITECTURE.md` §6). Do not reformat files you do not change (`.clang-format` exists, but
  upstream files are not uniformly formatted).
- **Text and RTL.** Every new string needs its `fa_IR` translation; check layouts right-to-left;
  accept Persian and Arabic digits in numeric input (CoCo already does this for batch counts).
- **QML wiring.** A new page needs a `PageEnum` entry, `Pages2/<Name>.qml` and a `qml.qrc` line. A
  new controller is constructed and registered in `coreController.cpp`. After either, regenerate the
  code map: `python3 -I tools/codemap/build_codemap.py` (CI fails a PR with a stale map).
- **Documentation is part of the change.** Before each commit, update `ARCHITECTURE.md`,
  `docs/ai/CHANGE_MAP.md`, `docs/ai/KNOWN_ISSUES.md` and `CHANGELOG.md` when they go stale. When you
  fix a known issue, delete its entry and name the fix in `CHANGELOG.md`.
- **Do not claim a test you did not run.** Most changes cannot be built in a cloud session (Qt +
  Conan + MSVC). Say "not built" when you could not build, and leave the Windows check to the CI
  build or the owner, as `CHANGELOG.md` already does.
- **Branches and PRs.** Default branch `main`. One change per branch (`claude/<slug>` or
  `feature/<slug>`), one PR, opened as a draft, with the template filled in. A push to `main`
  triggers the Windows installer build.
- **webpack.ir repository (`Iranux-Master/Iranux-Bash-Runner`) is read-only from this project.**
  Read its `AGENTS.md`, `docs/ai/API_REFERENCE.md` and module docs; never push to it from a CoCo task.
  Anything CoCo needs from webpack goes into `docs/integration/WEBPACK.md` §6 as a request.

## Commands

```bash
git submodule update --init --recursive
deploy/build.sh                                   # Linux / macOS
deploy\build.bat --installer ifw                  # Windows (as CI does)
AMNEZIA_BUILD_TESTS=ON deploy/build.sh            # with Qt tests, then: ctest --test-dir deploy/build
python3 -I tools/codemap/build_codemap.py         # regenerate docs/ai/CODE_MAP.md (add --check to verify)
```

Details, test names and what each test covers: `docs/ai/TESTING.md`.
