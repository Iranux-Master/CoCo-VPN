@AGENTS.md

## Claude Code notes

- Project skills live in `.claude/skills/`: `add-qml-page` (a new page and its controller),
  `add-vpn-protocol` (a new container / protocol end to end), `webpack-api` (calling webpack.ir from
  the client). Use them instead of re-deriving the wiring from source.
- Prefer `docs/ai/CODE_MAP.md` and the Graphify report over grepping `client/ui/qml/` or reading
  `coreController.cpp`; both are large.
- The `graphify` skill (`pip install graphifyy && graphify install`) can query the graph locally;
  see `tools/graphify/README.md`.
- Builds need Qt 6.10, Conan and a platform toolchain, which a cloud session usually does not have.
  Do static checks (code map, script syntax with `bash -n`, QML review) and say what was not built.
