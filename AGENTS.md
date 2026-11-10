This is a native Windows-first audio analyzer using C++20, CMake and Qt 6 Quick. Read PLAN.md for product behavior and docs/PROGRESS.md for the implemented scope.

Use `scripts/setup.ps1` to install project-local dependencies, `scripts/build.ps1 -Test` to build and run DSP tests, and `scripts/smoke.ps1 -Capture` to exercise the UI, layouts, WAV references and Windows capture. Keep generated tools, builds, packages and reports in the ignored .tools, build, dist and artifacts folders.

Keep DSP code independent of Qt. Capture may only copy/convert packets into preallocated bounded storage; no file decoding, UI work, FFTs or waits for consumers on the capture thread. Missing audio must be marked as discontinuous. Display frame drops must never silently discard measurement data.

Preserve single-capture/shared-analysis behavior and independent window settings. Use public Qt rendering APIs. The initial Windows renderer defaults to OpenGL and the basic render loop after local multiwindow measurements; environment overrides remain available. Preserve startup waveform behavior unless the user explicitly selects a startup layout or session restoration.

Treat loudness, VU and true-peak compliance as separate validated milestones. Do not label RMS as VU or LUFS, or mistake display smoothing/slope for measurement calibration. Spectrum reference averaging uses linear channel power before conversion to dB.
