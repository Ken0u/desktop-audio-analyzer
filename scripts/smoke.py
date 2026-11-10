"""Integration checks against the compiled application, using isolated local data."""
import argparse
import array
import json
import math
import os
from pathlib import Path
import subprocess
import time
import wave

import psutil

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'artifacts' / 'smoke'
EXE = ROOT / 'build' / 'release' / 'DesktopAudioAnalyzer.exe'


def run_case(name, extra=(), *, seconds=3, windows=1, demo=True, data_folder=None):
    folder = OUTPUT / name
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / 'report.json'
    arguments = [str(EXE), '--data-dir', str(data_folder or folder), '--windows', str(windows),
                 '--quit-after', str(seconds), '--report', str(report),
                 '--screenshot', str(folder / 'window.png')]
    if demo:
        arguments.append('--demo')
    arguments.extend(map(str, extra))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0  # Run the test helper without requesting a desktop window.
    started = time.perf_counter()
    with (folder / 'stdout.log').open('w') as out, (folder / 'stderr.log').open('w') as err:
        child = subprocess.Popen(arguments, cwd=ROOT, stdout=out, stderr=err, startupinfo=startup)
        process = psutil.Process(child.pid)
        peak_private = 0
        last_cpu = 0
        try:
            while child.poll() is None:
                try:
                    info = process.memory_info()
                    peak_private = max(peak_private, getattr(info, 'private', info.rss))
                    cpu = process.cpu_times()
                    last_cpu = cpu.user + cpu.system
                except psutil.NoSuchProcess:
                    break
                if time.perf_counter() - started > seconds + 30:
                    child.kill()
                    raise AssertionError(f'{name}: process did not exit')
                time.sleep(0.1)
        finally:
            child.wait(timeout=10)
    elapsed = time.perf_counter() - started
    assert child.returncode == 0, f'{name}: exit={child.returncode}; see {folder / "stderr.log"}'
    data = json.loads(report.read_text())
    assert not data['qmlErrors'], f'{name}: QML errors {data["qmlErrors"]}'
    assert data['windowCount'] == windows, f'{name}: wrong window count: {data}'
    assert data['queueOverruns'] == 0, f'{name}: capture overrun'
    assert data['screenshotSaved'], f'{name}: no rendered screenshot'
    if demo:
        assert data['analysisFrames'] >= 48000, f'{name}: DSP did not run'
        assert data['spectrogramRows'] > 0, f'{name}: spectrogram did not run'
    data['observedProcessCpuMsPerSecond'] = round(last_cpu * 1000 / elapsed, 1)
    data['peakPrivateMiB'] = round(peak_private / 1024**2, 1)
    data['elapsedSeconds'] = round(elapsed, 2)
    report.write_text(json.dumps(data, indent=2) + '\n')
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture', action='store_true')
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    fixture = OUTPUT / 'reference.wav'
    samples = array.array('h')
    for i in range(96000):
        value = round(16000 * math.sin(2 * math.pi * 1500 * i / 48000))
        samples.extend((value, -value))
    with wave.open(str(fixture), 'wb') as wav:
        wav.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        wav.writeframes(samples.tobytes())

    results = {}
    results['default'] = run_case('default')
    results['seven-windows'] = run_case('seven-windows',
        ['--reference', fixture, '--exercise-layout'], windows=7, seconds=6)
    assert results['seven-windows']['referenceLoaded'], 'Reference background job failed'
    assert results['seven-windows']['layoutRoundTripPassed'], 'Layout round trip failed'

    # Test startup restoration from the on-disk schema in a new process.
    saved = OUTPUT / 'seven-windows' / 'workspace.json'
    state = json.loads(saved.read_text())
    first, second = state['layouts']['Smoke test'][:2]
    first.update(gain=6, history=2, showReference=False, x=999999, y=999999, screen='missing-test-monitor')
    second.update(gain=-3, floor=-85, slope=4.5)
    state['startupLayout'] = 'Smoke test'
    saved.write_text(json.dumps(state))
    results['startup-layout'] = run_case('startup-layout', windows=7, seconds=2, data_folder=OUTPUT / 'seven-windows')
    restored = {window['instanceId']: window for window in results['startup-layout']['windowStates']}
    assert restored[first['instanceId']]['gain'] == 6 and restored[first['instanceId']]['history'] == 2
    assert restored[first['instanceId']]['showReference'] is False
    assert restored[first['instanceId']]['x'] < 999999 and restored[first['instanceId']]['y'] < 999999
    assert restored[second['instanceId']]['gain'] == -3 and restored[second['instanceId']]['slope'] == 4.5
    state = json.loads(saved.read_text())
    state['startupLayout'] = ''
    state['restoreLast'] = False
    saved.write_text(json.dumps(state))
    results['startup-default'] = run_case('startup-default', windows=1, seconds=2, data_folder=OUTPUT / 'seven-windows')

    corrupt_folder = OUTPUT / 'corrupt'
    corrupt_folder.mkdir(exist_ok=True)
    invalid = '{ broken configuration'
    (corrupt_folder / 'workspace.json').write_text(invalid)
    results['corrupt'] = run_case('corrupt', seconds=2)
    preserved = list(corrupt_folder.glob('workspace.json.invalid-*'))
    assert any(file.read_text() == invalid for file in preserved), 'Invalid workspace was not preserved'

    if args.capture:
        results['capture'] = run_case('capture', demo=False, seconds=5)
        assert results['capture']['state'] in ('Capturing', 'No audio packets - waiting', 'Waiting for playback'), results['capture']

    (OUTPUT / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps({name: {'cpuMsPerSecond': result['observedProcessCpuMsPerSecond'],
                             'privateMiB': result['peakPrivateMiB']} for name, result in results.items()}, indent=2))
    print(f'PASS: application smoke checks; reports and screenshots in {OUTPUT}')


if __name__ == '__main__':
    main()
