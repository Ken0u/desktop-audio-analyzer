Implementation began November 9, 2026. The repository is initialized locally on `main`.

The first slice implements the planned capture/queue/DSP/UI boundaries, waveform/spectrum/spectrogram windows, per-window display settings, named layout persistence, startup selection and background WAV reference analysis. This also begins parts of milestones 2, 3 and 5 so the capture prototype can be exercised through real workflows.

The capture thread uses event-driven WASAPI loopback and preallocated blocks. It records queue overruns and starts new analysis segments on lost data or source changes. The DSP engine shares a fixed 4,096-point transform across windows. Reference jobs use separate measurement state and a bounded streaming decoder.

The renderer retains plot geometry and completed 16-row spectrogram texture tiles. The initial Windows path uses OpenGL and Qt's basic render loop, selected after comparing the initial multiwindow measurements. Full settings-keyed DSP subscriptions, configurable FFT/window/hop, reference persistence, standards-based loudness, true-peak/VU, scope, stereo analysis and the full eight-hour performance/device matrix are still outstanding. The first milestone is in progress until that measurement matrix is completed.

Validation completed locally: release build, deterministic DSP suite, seven-window save/reload, explicit startup layout and default-waveform behavior, invalid-workspace preservation, background anti-phase WAV reference import, and live WASAPI data flow with zero observed queue overruns during the short capture check. The packaged build also launches with the development toolchain removed from PATH. GitHub Actions configuration is prepared for portable DSP tests and Windows application builds; hosted CI has not run yet.

Initial visible-window CPU/memory and Qt frame-swap measurements are recorded in [VALIDATION.md](VALIDATION.md). The 60 fps/frame-pacing and seven-window memory targets remain unmet; milestone 1 stays in progress.

Next: measure seven-window frame pacing, expand device-switch and endurance coverage, and expose FFT settings through shared analysis subscriptions. Then complete full reference import/caching and the remaining analyzers against the validation gates in PLAN.md.
