# 🏓 Flipper Zero Pinball

A simple pinball game for the **Flipper Zero** — designed for the 128×64 monochrome display in **portrait orientation** (hold the Flipper sideways!).

## 🎯 What It Does

This app turns your Flipper Zero into a pocket-sized pinball table. By holding the device sideways, the screen perfectly emulates the tall, narrow aspect ratio of a real pinball machine. 

```text
┌──────────────────────┐
│ SCORE: 0000    ● ● ● │
│ ──────────────────── │
│                      │
│      ╭─────╮         │
│      │ 100 │         │
│      ╰─────╯         │
│                      │
│              ╭────╮  │
│              │ 50 │  │
│              ╰────╯  │
│          o           │
│                      │
│                      │
│                      │
│                      │
│   \___        ___/   │
│                      │
│ █                  │ │
│ █                  │ │
│ █                  │o│
└──────────────────────┘
```

### Features

- **Portrait display** (64×128 pixels) for authentic pinball table feel
- **Two flipper arms** with snap-action physics
- **Two bumpers** that bounce the ball and award points
- **Spring-loaded ball launcher** — hold OK longer for more power
- **Sound effects** via the built-in speaker (bumper hits, flipper snaps, drain)
- **Simple collision physics** (gravity, wall bouncing, bumper impulse)

## 🎮 How to Play

Hold your Flipper Zero **sideways** (rotated 90°) so the D-pad is below or above the screen, depending on your preference.

| Control | Action |
|---------|--------|
| **Up / Left** | Activate left flipper |
| **Down / Right** | Activate right flipper |
| **OK (hold)** | Charge ball launcher |
| **OK (release)** | Launch ball |
| **Back** | Exit game |

## 📦 Installation

### Manual Installation
1. Compile the `.fap` using `ufbt` (see below).
2. Copy the resulting `.fap` to your Flipper's SD card: `SD Card/apps/Games/flipper_zero_pinball.fap`
3. On your Flipper: **Apps → Games → Pinball**

## 🔨 Building from Source

### Prerequisites
- Python 3.8+
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (Universal Flipper Build Tool)

### Build
```bash
pip install ufbt
cd flipper-zero-pinball
ufbt
```

The compiled `.fap` will be in the `dist/` directory.

### Deploy directly to Flipper
```bash
ufbt launch
```

## 📄 License

MIT License — see [LICENSE](LICENSE)
