"""Short steady-state resource probe. Does not establish frame-pacing compliance."""
import argparse
import json
from pathlib import Path
import subprocess
import time

import psutil

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=int, default=20)
parser.add_argument('--windows', type=int, default=7)
args = parser.parse_args()
folder = root / 'artifacts' / f'benchmark-{args.windows}'
folder.mkdir(parents=True, exist_ok=True)
command = [str(root / 'build/release/DesktopAudioAnalyzer.exe'), '--demo', '--windows', str(args.windows),
           '--data-dir', str(folder), '--quit-after', str(max(10, args.seconds)), '--report', str(folder / 'app.json')]
with (folder / 'stderr.log').open('w') as error:
    child = subprocess.Popen(command, cwd=root, stderr=error)
    process = psutil.Process(child.pid)
    start = time.perf_counter()
    readings = []
    while child.poll() is None:
        try:
            now = time.perf_counter() - start
            cpu = process.cpu_times()
            memory = process.memory_info()
            if 5 <= now <= max(10, args.seconds) - 0.5:
                unique = process.memory_full_info().uss
                readings.append((now, cpu.user + cpu.system, getattr(memory, 'private', memory.rss), memory.rss, unique))
        except psutil.NoSuchProcess:
            break
        if time.perf_counter() - start > args.seconds + 30:
            child.kill()
            raise RuntimeError('Benchmark timed out')
        time.sleep(0.1)
    child.wait()
if child.returncode:
    raise RuntimeError(f'App exit {child.returncode}; see {folder / "stderr.log"}')
if len(readings) < 2:
    raise RuntimeError('No steady-state readings')
result = json.loads((folder / 'app.json').read_text())
if len(result['renderedWindows']) != args.windows or any(not window['exposed'] or window['frameSwaps'] < 30 for window in result['renderedWindows']):
    raise RuntimeError('Every benchmark window must be exposed and rendering')
result['steadyProcessCpuMsPerSecond'] = round((readings[-1][1] - readings[0][1]) * 1000 / (readings[-1][0] - readings[0][0]), 1)
result['peakSteadyPrivateCommittedMiB'] = round(max(row[2] for row in readings) / 1024**2, 1)
result['peakSteadyWorkingSetMiB'] = round(max(row[3] for row in readings) / 1024**2, 1)
result['peakSteadyPrivateWorkingSetMiB'] = round(max(row[4] for row in readings) / 1024**2, 1)
result['steadyMeasuredSeconds'] = round(readings[-1][0] - readings[0][0], 1)
(folder / 'resources.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
