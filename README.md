# PixelPet

[![CI](https://github.com/wscytz/pixel-pet/actions/workflows/build.yml/badge.svg)](https://github.com/wscytz/pixel-pet/actions/workflows/build.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)
![platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Qt 6](https://img.shields.io/badge/Qt-6-green)
![emotion](https://img.shields.io/badge/emotion-pure%20algorithm%20(no%20ML)-ff69b4)

A tiny always-on-top desktop pet that turns the music you're listening to into a living **pixel kaomoji face** — happy songs smile and hop, sad ballads droop and slow down, dubstep bares its fangs. Pure algorithmic emotion recognition, **no ML models**.

## What it does

While audio plays, the pet continuously analyzes the signal and maps it onto a 2D **valence–arousal** emotion space (from the circumplex model), then drives a programmatically-drawn LED-style kaomoji face, its colors, and its motion.

The face is not a font glyph — it's hand-authored pixel art rendered with `QPainter` (eyes / brows / mouths / arms / signature accessories like tears, hearts, lightning), so it scales crisply and stays readable at any size.

**16 expression tiers** in total:
- **10 auto-detected emotions**: joyful, hype, healing, calm, sad, agitated, surprised, sleepy, angry, love
- **6 manual activity modes**: focus, work, game, rest, exercise, EDM — pin the face & colors while the sound-wave floor keeps reacting to the audio

### Beyond the face

- **Section awareness** — detects intro / verse / chorus / bridge / outro from chroma self-similarity and energy; the pet leans into a chorus.
- **Rhythm feel** — kick density drives the bounce, so a four-on-the-floor track hops harder than a sparse ballad; backbeat and syncopation are tracked alongside.
- **Emotion trajectory** — follows valence/arousal drift over time (rising / falling / tension / release) rather than reacting to every single frame.
- **Personal calibration** — A/B anchor a "happy" song and a "sad" song; the pet adapts to *your* major/minor threshold (what feels melancholy is subjective).
- **Focus log** — time spent in each manual activity mode is accumulated into a weekly focus report, stored locally.
- **Source panel** — a settings page auto-detects which source is currently feeding the pet and toggles each one on or off.

## Audio sources

| Source | How | Notes |
|---|---|---|
| Local file | drag a music file onto the window, or `pixel-pet <song>` | offline, ideal for testing a single track |
| Browser tab | companion extension (`browser-extension/`) captures the playing tab and streams live spectrum data over WebSocket | works with any streaming site; toggle capture with **Ctrl/Cmd+Shift+0** |
| System audio | **Windows only** — WASAPI loopback captures the default output device | feeds the pet from *any* desktop app, no browser needed |

The three sources share one analysis pipeline, so the emotion mapping is identical everywhere.

## Look & feel

- **Differential scaling** — the smaller the window, the more the face dominates and the EQ floor / margins / buttons shrink, so a mini pet still reads clearly.
- Size from **150 px mini** up to 520 px; center-anchored, position & size remembered.
- Frameless always-on-top window; drag to move, wheel / pinch to resize, hover to reveal close / minimize.
- Customize: window opacity (40–100 %), lock position, EQ floor style (**bars / mirror / wave**).
- Right-click menu for everything; minimize hides to the tray / menu bar.

## Building

See [BUILD.md](BUILD.md) — macOS (Homebrew Qt) and Windows (MSVC + Qt6) both supported; a GitHub Actions workflow builds and packages both platforms on every push.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## License

[GPL-2.0](LICENSE)
