# Working in this repository

Pinball Zero: a Flipper Zero game in C with two tables, plus a browser edition
that runs the same C core as WebAssembly.

## Checks

```bash
cc -std=c11 -Wall -Wextra -Werror tests/physics_test.c pinball_physics.c -lm -o /tmp/pinball-physics-test && /tmp/pinball-physics-test
python -m pip install ziglang==0.15.2 && python scripts/build_web.py --check   # web/pinball.wasm must rebuild byte for byte
npm ci && node --test tests/web.test.mjs && npm run test:browser              # Playwright Chromium
# the .fap builds in CI with flipperdevices/flipperzero-ufbt-action (release SDK)
```

## Rules

- `web/pinball.wasm` and `web/build.json` are build outputs: change the C
  sources, run `scripts/build_web.py`, commit the result. Never edit them.
- The physics is shared by firmware and browser; a change must keep the
  native and WebAssembly trajectories identical (`tests/web.test.mjs`).
- Scores and settings in the browser stay in that browser.

## Working alongside other agents

Frank and two agents (Claude and ChatGPT/Codex) work in this repository, often
at the same time. The repository itself is the only channel between them.

- Work on your own branch (`codex/…`, `claude/…`). Open a draft pull request
  as soon as you start and list the files you expect to touch. Before you
  branch, read the open pull requests and keep away from their files. Never
  push to another agent's branch, and never to `main` directly.
- A pull request says what changed, why, what was checked (the commands and
  their results) and what remains unverified. Fix a failing check; do not
  weaken or skip it.
- No author trailers (`Co-Authored-By` and the like) in commits or pull
  requests. The commit author is enough.
- No status files, task lists or progress notes in the repository. The pull
  requests and the history are the record.
- Frozen material (below) is not edited in place. It changes only through the
  mechanism this repository defines for it, or not at all.
