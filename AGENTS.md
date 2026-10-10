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

Frank works with human contributors and AI agents in this repository, often
at the same time. Agents using any model or provider are welcome. The
repository is their shared channel for coordination.

- Work on your own branch (`<agent>/…`, for example `codex/…` or
  `claude/…`). Open a draft pull request as soon as you start and list the
  files you expect to touch. Before you branch, read the open pull requests
  and keep away from their files. Never push to another agent's branch, and
  never to `main` directly.
- A pull request says what changed, why, what was checked (the commands and
  their results) and what remains unverified. Fix a failing check; do not
  weaken or skip it.
- Attribution is optional. Contributors, including AI agents, may identify
  themselves in a pull request or a `Co-authored-by` commit trailer. Credit
  actual contributions and use only names, model details and attribution
  email addresses you know to be accurate. If no attribution email is known,
  use the pull request description. No fixed agent, model or provider name
  is required.
- No status files, task lists or progress notes in the repository. The pull
  requests and the history are the record.
- Frozen material (below) is not edited in place. It changes only through the
  mechanism this repository defines for it, or not at all.
- **Who merges what.** An agent may merge a pull request that changes
  infrastructure, robustness, reproduction, tests or documentation of what
  exists — once CI is green *and* another agent has read the diff against the
  description. A pull request that changes what a work says, does, sounds or
  looks like — texts, scenes, decisions and their costs, pieces, compositions,
  essays, the data a site shows — is marked `needs Frank` (the label, or the
  title prefix `needs Frank:`) and stays open until Frank has read, played or
  listened. No agent merges it, however green it is.
- **Read the diff, not the badge.** Before merging another agent's pull
  request, check that the diff does what the description claims, that nothing
  the description lists as unverified is claimed elsewhere, and that no check
  was weakened. A pull request nobody has read is not reviewed.
