# PixelPet

A tiny desktop pet that turns the music you're listening to into a living **pixel kaomoji face** — happy songs smile and hop, sad ballads droop and slow down, dubstep bares its fangs. Pure algorithmic emotion recognition, **no ML models**.

## What it does

While audio plays, the pet continuously analyzes the signal and maps it onto a 2D **valence–arousal** emotion space (from the circumplex model), then drives a programmatically-drawn LED-style kaomoji face, its colors, and its motion.

The face is not a font glyph — it's hand-authored pixel art rendered with `QPainter` (eyes / brows / mouths / arms / signature accessories like tears, hearts, lightning), so it scales crisply and stays readable at any size.

- **10 emotional expressions** (auto-detected): joyful, hype, healing, calm, sad, agitated, surprised, sleepy, angry, love
- **6 activity modes** (manual): focus, work, game, rest, exercise, EDM — fix the face & colors while the sound-wave floor keeps reacting to the audio
- **Algorithmic signal chain**: FFT → spectrum / RMS / centroid / BPM / chroma key detection (Krumhansl–Kessler mode profiles) → valence–arousal → emotion tier. Fully deterministic, tunable, no training.

## Audio sources

| Source | How | Notes |
|---|---|---|
| Local file | drag a music file onto the window, or `pixel-pet <song>` | offline, ideal for testing a single track |
| Browser tab | companion extension (`browser-extension/`) captures the playing tab and streams raw PCM over WebSocket | works with any streaming site |
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
