Desktop Audio Analyzer is a Windows desktop audio analysis app in early development. It opens a waveform window and captures the default Windows playback output. Add independent waveform, spectrum, and spectrogram windows from any window's toolbar.

The first implementation includes an event-driven WASAPI loopback backend, a bounded audio queue, shared stereo analysis, custom Qt Quick plots, independent display settings, named layouts, and background WAV spectrum-reference analysis. The full product scope and research are in [PLAN.md](PLAN.md).

Use Windows 11 x64 and Python 3.10 or newer. Dependencies are installed under the ignored `.tools` folder; setup needs internet access and roughly 2 GB of free space. Qt 6.8.3 and its matching MinGW 13.1 compiler are pinned for reproducibility.

```powershell
.\scripts\setup.ps1
.\scripts\build.ps1 -Test
.\scripts\run.ps1
```

Run a deterministic internal signal to inspect the interface without capturing desktop audio:

```powershell
.\scripts\run.ps1 -Demo -Windows 7
```

The toolbar adds windows, holds an individual display, opens its settings, and saves/loads layouts. `Ctrl+N` opens the analyzer menu, `Ctrl+S` opens layouts, Space holds/resumes a display, and `Ctrl+Q` exits all windows. Display hold leaves capture/analysis running. Each window has independent gain, history or spectral floor/slope, and always-on-top settings. Layouts preserve those settings and monitor geometry. Startup remains a waveform unless a saved layout or Restore Last Session is explicitly selected.

The spectrum window accepts a local WAV reference through Browse or drag-and-drop. Orange is the average reference power-density curve; mint is the current captured curve. This prototype uses a 4,096-point Hann transform and 75% overlap. Spectral values are one-sided power density, shown as dBFS/Hz, rather than tone-amplitude dBFS. Stereo curves average channel power, so opposite-phase audio does not cancel. Display slope pivots at 1 kHz and applies equally to live and reference traces.

Reference import currently supports mono/stereo RIFF WAV with PCM 8/16/24/32-bit or float32 samples at 8-192 kHz. Overlay requires a matching live/reference sample rate. A reference needs at least 4,096 frames. Only complete overlapped transform windows contribute to its spectrum; fewer than 1,024 trailing frames may remain outside the spectral average. Full-file sample peak and RMS include every decoded frame. Reference files and curves are not yet persisted in layouts. MP3/FLAC/AIFF, resampling, selected regions, reference caching, loudness analysis, VU/true-peak meters, stereometer and oscilloscope remain scheduled work.

The capture backend currently requires a mono/stereo endpoint and follows the default multimedia playback output. Exclusive-mode/ASIO paths may require separate routing. Capture errors, output changes and queue loss start a new analysis segment. An interval with no packets displays an unavailable state rather than leaving current meters frozen or treating uncertain data as measured silence. Format changes and device reconnects are handled by reopening the source.

Saved workspaces normally live in `%LOCALAPPDATA%/DesktopAudioAnalyzer/DesktopAudioAnalyzer/workspace.json`. Use `--data-dir <folder>` to override storage. Saves use atomic replacement and keep a previous-good backup. Invalid/future-version files are preserved before saving a new workspace. Off-screen windows are fitted to available monitors.

Build a local runnable folder with:

```powershell
.\scripts\package.ps1
```

Then run `dist/DesktopAudioAnalyzer/DesktopAudioAnalyzer.exe`. The local package is a development artifact; distribution licensing and signing are tracked in [THIRD_PARTY.md](THIRD_PARTY.md).

Deterministic DSP tests cover queue wraparound/concurrency, spectral calibration, DC scaling, anti-phase stereo, transient preservation, discontinuities, cancellation and malformed WAV input. The application supports `--demo`, `--windows 7`, `--quit-after 5`, `--report <json>`, `--screenshot <png>`, `--reference <wav>`, and `--exercise-layout` for integration checks. Run `scripts/smoke.ps1` for the combined demo/layout/reference smoke test, or add `-Capture` to check real loopback initialization and data flow.

The renderer retains plot geometry and updates spectrogram history in small texture tiles through public Qt APIs. On Windows it defaults to OpenGL with Qt's basic render loop following local multiwindow measurements. `QSG_RHI_BACKEND` and `QSG_RENDER_LOOP` remain available for comparison on other drivers. Performance budgets in the plan are targets; frame-pacing and eight-hour validation are outstanding. For a short resource probe, load `scripts/environment.ps1` and run `.tools/python/Scripts/python.exe scripts/benchmark.py --windows 7 --seconds 20`; reports distinguish private committed memory, total working set, and private working set.
