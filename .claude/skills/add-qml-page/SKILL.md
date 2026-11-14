---
name: add-qml-page
description: Add a new page (and, if needed, a new controller) to the CoCo VPN QML client with the right wiring - PageEnum, Pages2 file, qml.qrc, CoreController registration, Persian strings and the code map. Use for any new screen or drawer in client/ui/qml.
---

# Add a QML page to CoCo VPN

Read `docs/ai/CODE_MAP.md` first: it lists every context object QML can use, what each exposes,
and which existing pages use them. Find the closest existing page there and copy its structure.

## Steps

1. **Enum.** Add `Page<Name>` to `enum class PageEnum` in `client/ui/utils/pageEnum.h`. The enum
   key must equal the QML file name: `PageController::getPagePath` builds the path from it.
2. **QML file.** Create `client/ui/qml/Pages2/Page<Name>.qml`. Reuse `Controls2/*Type.qml`
   controls (header, buttons, text fields, drawers) and `Modules/Style/AmneziaStyle.qml` colours;
   do not introduce raw colours or new fonts. Use `qsTr()` for every string.
3. **Resource.** Add the file to `client/ui/qml/qml.qrc` next to the other `Pages2` entries.
4. **Navigation.** Open it with `PageController.goToPage(PageEnum.Page<Name>)` from the caller.
5. **Logic.** Keep QML declarative. If the page needs new behaviour:
   - logic in `client/core/controllers/<area>/<name>Controller.{h,cpp}` (no QML types);
   - a thin adapter `client/ui/controllers/<area>/<name>UiController.{h,cpp}` with `Q_PROPERTY`,
     `Q_INVOKABLE` / `public slots` and signals;
   - construct both and call `setQmlContextProperty("<QmlName>", ...)` in
     `client/core/controllers/coreController.cpp` (`initControllers`); add members to
     `coreController.h`; wire cross-controller signals in `coreSignalHandlers.cpp`;
   - list the new sources in `client/cmake/sources.cmake`.
6. **Persian.** Add translations for the new strings to
   `client/translations/amneziavpn_fa_IR.ts` (context = the QML file or C++ class name). Check that
   the layout reads right-to-left (anchors, `LayoutMirroring`, icons that point a direction).
7. **Code map.** Run `python3 -I tools/codemap/build_codemap.py` and commit `docs/ai/CODE_MAP.md`.
8. **Docs.** Add a `CHANGELOG.md` entry (Persian) and update `docs/ai/CHANGE_MAP.md` if a new kind
   of change appeared.

## Checks without a build

- `python3 -I tools/codemap/build_codemap.py --check` passes.
- The page name appears in `pageEnum.h`, `Pages2/`, and `qml.qrc`.
- No logic beyond bindings and controller calls in the QML file.
- Say in the PR that the page was not built or run if you could not build it.
