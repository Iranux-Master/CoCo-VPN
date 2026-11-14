# Graphify code graph

[Graphify](https://github.com/Graphify-Labs/graphify) turns the source into a knowledge graph:
classes, functions, includes and calls become nodes and edges, and community detection groups them
into clusters. It parses C++, Objective-C, Kotlin, Java and Swift here; it does **not** parse QML,
which is why `docs/ai/CODE_MAP.md` (`tools/codemap/build_codemap.py`) covers the QML side.

On 2026-11-14 the graph of `main` had about 11,000 nodes, 23,000 edges and 420 communities, built in
about 15 seconds with no LLM calls (AST only, deterministic).

| File (on the `graphify` branch) | Purpose |
|---|---|
| `graphify-out/GRAPH_REPORT.md` | Plain-language report: the most connected types ("god nodes"), communities, surprising links, suggested questions. Read this first. |
| `graphify-out/graph.json.gz` | The full graph; decompress to `graphify-out/graph.json` for the query commands. |
| `SOURCE.txt` | The `main` commit the graph was built from. |

`.graphifyignore` (gitignore syntax) keeps third-party code, images, fonts, translations, tests,
recipes, deploy and CMake out of the graph. `graphify-out/` is git-ignored on `main`.

## How it stays current

`.github/workflows/graphify.yml` runs on `ubuntu-latest` after every push to `main` that changes
`client/`, `service/`, `ipc/` or `common/` (and on manual dispatch). It installs the pinned set from
`requirements.txt` with `--require-hashes --only-binary=:all:`, runs `graphify update . --force`,
gzips `graph.json` and force-pushes the two files as the single commit of the `graphify` branch.
Never push to that branch by hand. `requirements.txt` and `pin.py` come from Iranux-Bash-Runner,
which uses the same Graphify release; to move to a newer one run `python3 -I tools/graphify/pin.py
<version>` and commit the result.

## Reading the graph as an agent

```bash
git fetch --depth=1 origin graphify
git show origin/graphify:graphify-out/GRAPH_REPORT.md
mkdir -p graphify-out && git show origin/graphify:graphify-out/graph.json.gz | gunzip > graphify-out/graph.json
```

## Building or querying it yourself

Python 3.11 to 3.14. The package is `graphifyy` on PyPI; the command is `graphify`.

```bash
python3 -m venv .venv-graphify
.venv-graphify/bin/pip install --require-hashes --only-binary=:all: -r tools/graphify/requirements.txt
.venv-graphify/bin/graphify update .
.venv-graphify/bin/graphify query "how does a batch account reach the server?"
.venv-graphify/bin/graphify path "ExportUiController" "SshSession"
.venv-graphify/bin/graphify explain "InstallController"
.venv-graphify/bin/graphify affected "SecureServersRepository"
```

`graphify-out/graph.html` is an interactive viewer (aggregated by community above 5,000 nodes).
For Claude Code, `graphify install` adds the `/graphify` skill, which answers questions from the graph
without re-reading the source.
