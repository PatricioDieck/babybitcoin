### TL;DR
- We made C++ dev feel JS-like: incremental builds + auto-restart.
- Key changes:
  - Split `cli.cpp` into its own unit (`cli.h` included in `main.cpp`) for smaller rebuilds. C++ builds into (translation units)- so if you include one cpp into another cpp file, then that whole unit gets recompiled on change by the compiler. This is why we split cli.cpp into its own unit- it forced recompiling main.cpp (and anything that included it)
  - Makefile now builds per-file with ccache, parallel jobs, and out-of-tree artifacts under `build/`.
  - Watcher (`watchexec`) rebuilds and restarts interactive mode on save.
  - Only mine genesis when the chain is empty; persist chain to disk.
- Result: fast “save → rebuild → app restarts → state preserved.”'

to run the watcher, you can use the following command:
```bash
watchexec -w src --exts cpp,h --ignore build --ignore data --ignore wallets --restart --clear -- \
  'make -j"$(sysctl -n hw.ncpu)" && ./babycoin -i'
```

### Summary
- Faster iteration: ccache + parallel + watch-and-restart.
- Clean tree: `.o/.d` moved under `build/`.
- Deterministic boot: load chain; mine genesis only when needed.
- Easy mental model for Makefile: inputs → objects → link → run; deps tracked automatically.

### Why we did this (first principles)
- C++ rebuilds were slow because:
  - Everything linked as one step every run.
  - Including `.cpp` inside `.cpp` forced unnecessary recompiles.
- We want:
  - Only recompile files that changed (and their dependents).
  - Cache object code across runs (ccache).
  - Auto-restart the app after a change to see results immediately.
  - Keep build artifacts out of the source tree.

### What we changed
- Build system:
  - Per-file compilation with dependency tracking (`-MMD -MF dep.d -MP`).
  - Caching with `ccache`.
  - Parallel jobs (`make -j...`).
  - Artifacts moved to `build/obj` and `build/dep`.
  - Disabled built-in rules so stray `.o` don’t land in `src/`.
- Source structure:
  - Removed `#include "cli.cpp"` from `main.cpp`.
  - Added `src/cli.h`, included it in `main.cpp`.
  - Compile `cli.cpp` as a separate translation unit → smaller rebuilds.
- Runtime boot sequence:
  - Load chain from `data/`; only mine genesis if `cs.height() == 0`.
  - Save chain after acceptance so restarts don’t re-genesis.
- Watcher:
  - `watchexec` watches `src/`, rebuilds, and restarts the app in interactive mode.

### How the hot-reload-like flow works
- We’re using “watch-and-restart” (fast and robust), not true in-process code swapping.
- Watcher observes source changes and runs:
  - Build only changed files (Make decides via `.d` dependency files).
  - Use cached objects for unchanged code (ccache).
  - Restart the app with `--interactive`.
- State persists because chain and wallets are saved to disk.

### Commands you’ll use
- Install tools (one-time):
```bash
brew install openssl ccache watchexec
```
- Build fast:
```bash
make -j"$(sysctl -n hw.ncpu)"
```
- Auto rebuild and restart interactive mode on save:
```bash
watchexec -w src --exts cpp,h --ignore build --ignore data --ignore wallets --restart --clear -- \
  'make -j"$(sysctl -n hw.ncpu)" && ./babycoin --interactive'
```
- Clean:
```bash
make clean
```
- If old objects appeared in `src/`, nuke them once:
```bash
find src -name '*.o' -delete; find src -name '*.d' -delete
```

### How to read the Makefile (what the moving parts mean)
- Variables:
  - `CXX`, `CXXFLAGS`: compiler and flags. We use ccache + clang++ with `-O0 -g` for fast dev builds.
  - `LDFLAGS`: links against OpenSSL and pthread.
  - `SRCS`: the list of `.cpp` files.
  - `BUILD_DIR`, `OBJ_DIR`, `DEP_DIR`: where we put `.o` and `.d` (keeps repo clean).
- Derivations:
  - `OBJS`: one `.o` per `.cpp` → placed under `build/obj/...`.
  - `DEPS`: one `.d` per `.cpp` → placed under `build/dep/...`.
- Rules:
  - `$(OBJ_DIR)/%.o: %.cpp`: compile a single file and write its dependency file with `-MMD -MP -MF`.
  - `-include $(DEPS)`: Make includes dependency graphs so only what changed rebuilds.
  - `$(TARGET): $(OBJS)`: link all objects into the final binary.
- Extras:
  - `MAKEFLAGS += --no-builtin-rules` + `.SUFFIXES:`: prevent default implicit rules that could drop `.o` into `src/`.

Note on “duplicate-looking” deps:
- `.d` files can list phony header targets (from `-MP`) and sometimes multiple targets for robustness. That’s normal and harmless.

### What the watcher watches and why
- Watches: `src/` with `--exts cpp,h`.
- Ignores: `build/`, `data/`, `wallets/` so it doesn’t retrigger on outputs or persisted state.
- Behavior: on any saved change, it rebuilds changed files and restarts the interactive app.

### Recompilation times and what changed
- Before:
  - Monolithic compiles; including `.cpp` in `.cpp` triggered wide rebuilds.
  - No caching; even unchanged code paid a cost.
- After:
  - Incremental builds: only modified `.cpp` (and dependents) recompile.
  - ccache: unchanged code compiles from cache → sub-second object regen.
  - Parallel: `-jN` uses all cores for faster cold builds.
  - Out-of-tree artifacts: clean source tree; simpler mental model.

Typical effects (rough guide):
- Cold build: still bounded by total code size, but parallel + clang speeds it up significantly.
- Incremental edit in one `.cpp`: usually <1s recompile + fast relink.
- Many small edits: still fast due to ccache and tracked deps.

### Optional: true hot reload (future)
- If you ever want real live code swapping without restarts:
  - Move hot code into a `.dylib` with a stable C interface.
  - Host process `dlopen`/`dlsym` the library and reload on rebuild.
  - Keep state (e.g., `ChainState`, `P2PNode`) in the host so reloads don’t lose it.
  - More complex; only worth it if restarts become a bottleneck.

### Dependencies recap
- Required:
  - `openssl` (already used).
  - `ccache` (build cache).
  - `watchexec` (watcher).
- Optional:
  - `llvm` (clang++) and LLD for faster linking (if supported on your setup).

- We reorganized builds (ccache + out-of-tree objects and deps) and added a watcher so edits rebuild quickly and auto-restart interactive mode. Only mine genesis on empty chains and persist state, making restarts deterministic and fast.