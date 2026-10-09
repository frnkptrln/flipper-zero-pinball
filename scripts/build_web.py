"""Build the shared C engine as an import-free WebAssembly module with Zig 0.15.2."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ("web/bridge.c", "pinball_physics.c", "pinball_render.c", "pinball_physics.h", "pinball_render.h", "pixel_ui.h")
EXPORTS = (
    "init", "controls", "cancel_controls", "nudge", "advance", "render", "phase", "score", "lives", "table",
    "multiplier", "tilted", "inputs", "charge", "ball_x", "ball_y", "ball_active",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="rebuild and compare the committed binary and manifest")
    args = parser.parse_args()
    zig = [sys.executable, "-m", "ziglang"]
    version = subprocess.check_output([*zig, "version"], text=True).strip()
    if version != "0.15.2":
        parser.error("install the pinned compiler: python -m pip install ziglang==0.15.2")
    with tempfile.TemporaryDirectory(prefix="pinball-web-") as directory:
        output = Path(directory) / "pinball.wasm"
        subprocess.run([
            *zig, "cc", "-target", "wasm32-wasi", "-mexec-model=reactor", "-O2", "-Wl,--strip-all",
            "-std=c11", "-Wall", "-Wextra", "-Werror",
            *[f"-Wl,--export=pb_{name}" for name in EXPORTS],
            *SOURCES[:3], "-o", str(output),
        ], cwd=ROOT, check=True)
        binary = output.read_bytes()
    manifest = {
        "compiler": "zig 0.15.2", "target": "wasm32-wasi reactor", "optimization": "O2; stripped",
        "wasm_sha256": hashlib.sha256(binary).hexdigest(),
        "sources": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCES},
    }
    manifest_text = json.dumps(manifest, indent=2) + "\n"
    target = ROOT / "web/pinball.wasm"
    metadata = ROOT / "web/build.json"
    if args.check:
        if target.read_bytes() != binary or metadata.read_text() != manifest_text:
            parser.exit(1, "Browser artifact differs from the shared sources; run scripts/build_web.py.\n")
    else:
        target.write_bytes(binary)
        target.chmod(0o644)
        metadata.write_text(manifest_text)
    print(f"{'Verified' if args.check else 'Built'} {len(binary):,} bytes; {manifest['wasm_sha256']}")


if __name__ == "__main__":
    main()
