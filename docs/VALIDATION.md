The initial Windows prototype was built and checked locally on November 9, 2026, with Qt 6.8.3 and MinGW 13.1 in Release mode.

The deterministic DSP suite passes 29 checks covering queue capacity/wraparound/concurrent ordering, one-sided PSD calibration, DC scaling, invalid transform sizes, stereo anti-phase energy, transform cadence, discontinuities, transient-preserving envelopes, PCM decoding, WAV validation, reference coverage and cancellation.

Application smoke checks pass for the default waveform, seven independent windows, background WAV reference analysis, named-layout round trips, a saved startup layout, default-waveform startup after disabling restoration, invalid-workspace preservation, and real desktop loopback initialization/data flow. No QML warnings or queue overruns were observed in these short checks. The live capture check used the current default stereo output at 48 kHz; it does not establish coverage of unplug/reconnect, other devices, or long runs.

A packaged development build launches with Qt/compiler directories removed from PATH, using the DLLs and QML modules beside the executable. This is a local deployment check, not a clean-machine or public release certification. Hosted CI has not run; the repository contains a workflow for portable DSP and Windows builds.

Short visible-window measurements used the built-in stereo demo signal, 48 kHz audio, a fixed 4,096-point Hann transform with 75% overlap, OpenGL with Qt's basic render loop, and 980 x 430 windows at 125% DPI. The resource probes ran for 15 seconds, discarding the first five seconds for CPU/memory readings. Every measured window was exposed and produced frame swaps. The machine was the local Windows workstation, rather than the reference laptop proposed in PLAN.md.

| Workload | Process CPU time per second | Peak private working set | Peak private committed memory | Observed Qt frame-swap interval, 95th percentile |
| --- | --- | --- | --- | --- |
| One waveform window | 51.2 ms/s | 91.0 MiB | 116.9 MiB | 32.9 ms |
| Seven windows: three waveform, two spectrum, two spectrogram | 202.8 ms/s | 296.5 MiB | 329.4 MiB | 34.0-46.5 ms across windows |

Frame-swap intervals cover the short run and are observed through Qt callbacks; they are not compositor presentation measurements. The prototype does not meet the planned 60 fps/frame-pacing target, and the seven-window private working set exceeds the proposed 250 MiB budget. The waveform CPU reading is close to the 50 ms/s target. These figures guide the next optimization work and do not establish release performance across machines.

Full JSON reports and screenshots are generated under the ignored `artifacts` folder. Reproduce the checks with:

```powershell
.\scripts\build.ps1 -Test
.\scripts\smoke.ps1 -Capture
. .\scripts\environment.ps1
& $toolPython scripts/benchmark.py --windows 7 --seconds 15
& $toolPython scripts/benchmark.py --windows 1 --seconds 15
```

The next performance work is to schedule UI snapshots against rendering cadence, interpolate visual movement where appropriate, stop unused spectral processing, continue reducing per-window GPU/resource overhead, and validate frame pacing with a presentation-aware tool. Eight-hour stability, device changes, minimized/unfocused behavior, mixed refresh-rate displays, sample-rate changes and the broader audio-format matrix remain open gates.
