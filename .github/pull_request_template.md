## What and why

<!-- One bounded change. Link the issue or the KNOWN_ISSUES / ROADMAP item. -->

## How it was checked

<!-- Say exactly what ran: Windows CI build, ctest names, a hands-on check on Windows, or "not built". -->

- [ ] Built (which platform):
- [ ] Tests run (which):
- [ ] Checked in the running app (what):

## Documentation

- [ ] `python3 -I tools/codemap/build_codemap.py` run if QML, controllers or registrations changed
- [ ] `ARCHITECTURE.md`, `docs/ai/CHANGE_MAP.md`, `docs/ai/KNOWN_ISSUES.md` still true
- [ ] `CHANGELOG.md` entry (Persian) with its "وضعیت بررسی" line
- [ ] New strings translated in `amneziavpn_fa_IR.ts`, layout checked right-to-left

## Safety

- [ ] No secret, key, password, token or client config in code, logs or docs
- [ ] No remote command built from raw user input
