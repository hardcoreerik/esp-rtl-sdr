"""One-hour network-only four-receiver external-power observation."""
import argparse
import ctypes
import datetime as dt
import json
import os
from pathlib import Path
import threading
import time
from urllib.request import Request, build_opener, ProxyHandler
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True)
args = parser.parse_args()
out = Path(args.out)
out.mkdir(parents=True, exist_ok=False)
root = 'http://192.168.1.41/api/v1/'
http = build_opener(ProxyHandler({}))
failures = []
failure_lock = threading.Lock()
stop_serial = threading.Event()
serial_state = {'connected': False, 'bytes': 0, 'error': ''}
duration = 60 * 60
interval = 30
phase_name = '2.56 MS/s'
PROFILES = ('FM_REFERENCE', 'VHF_ANOMALY', 'ISM_902', 'ADSB_1090')

def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()

def fail(reason):
    with failure_lock:
        failures.append({'utc': utc(), 'phase': phase_name, 'reason': reason})
        with (out / 'failures.jsonl').open('a') as log:
            log.write(json.dumps(failures[-1]) + '\n')
    print('FAIL:', reason, flush=True)

def serial_capture():
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = 'COM18', 115200, 0.5
    port.dtr = port.rts = False
    try:
        port.open()
        serial_state['connected'] = True
        tail = b''
        with port, (out / 'serial.log').open('ab', buffering=0) as log:
            while not stop_serial.is_set():
                chunk = port.read(port.in_waiting or 1)
                if not chunk:
                    continue
                log.write(chunk)
                serial_state['bytes'] += len(chunk)
                tail += chunk
                lines = tail.split(b'\n')
                tail = lines.pop()
                for line in lines:
                    if any(marker in line for marker in (b'task_wdt:', b'ESP_ERR_NO_MEM', b'start_hz failed', b'Guru Meditation', b'usb disconnected', b'bulk resubmit failed')):
                        fail('serial: ' + line.decode('utf-8', errors='replace'))
    except Exception as exc:
        serial_state['error'] = str(exc)
        fail('serial observation unavailable: ' + str(exc))
    finally:
        serial_state['connected'] = False

def request(route, post=False):
    with http.open(Request(root + route, data=b'' if post else None), timeout=8) as reply:
        return json.load(reply)

def snapshot():
    return {'utc': utc(), 'status': request('status'),
            'receivers': request('receivers')['receivers'],
            'scanners': [request(f'scanner?index={i}') for i in range(4)]}

def validate(s, expected_rate, baseline=None, previous=None):
    rx = s['receivers']
    scans = s['scanners']
    if len(rx) != 4 or len(scans) != 4:
        raise RuntimeError('Expected four receiver and scanner records')
    if s['status']['receivers']['streaming'] != 4:
        raise RuntimeError('Not all four receivers streaming')
    for i, (r, scan) in enumerate(zip(rx, scans)):
        if r.get('index') != i or not r.get('present') or r.get('state') != 3 or r.get('sample_rate') != expected_rate or r.get('last_error'):
            raise RuntimeError(f'RTL{i} receiver state/rate/error failed')
        if scan.get('receiver') != i or scan.get('run') != 'RUNNING' or scan.get('profile') != PROFILES[i] or not scan.get('iq_valid') or scan.get('last_error'):
            raise RuntimeError(f'RTL{i} scanner state/identity/error failed')
        initial = baseline['receivers'][i] if baseline else r
        if r.get('drops') != initial.get('drops') or r.get('usb_errors') != initial.get('usb_errors'):
            raise RuntimeError(f'RTL{i} drops={r.get("drops")} usb_errors={r.get("usb_errors")}')
        if previous:
            old = previous['receivers'][i]
            old_scan = previous['scanners'][i]
            for key in ('usb_addr', 'product', 'serial', 'tuner'):
                if r.get(key) != old.get(key):
                    raise RuntimeError(f'RTL{i} identity changed: {key}')
            if r['iq_bytes_consumed'] <= old['iq_bytes_consumed']:
                raise RuntimeError(f'RTL{i} IQ consumption stalled or reset')
            for key in ('hops_done', 'fft_frames_total'):
                if scan[key] <= old_scan[key]:
                    raise RuntimeError(f'RTL{i} {key} stalled or reset')
            if scan['sweep_index'] < old_scan['sweep_index']:
                raise RuntimeError(f'RTL{i} sweep counter reset')
    if previous and s['status']['uptime_us'] <= previous['status']['uptime_us']:
        raise RuntimeError('Board uptime reset')

thread = threading.Thread(target=serial_capture, daemon=True)
# Network-only observation: COM18 is disconnected.
state = {'state': 'STARTING', 'pid': os.getpid(), 'started_utc': utc(),
         'duration_seconds': duration, 'interval_seconds': interval, 'samples': 0,
         'failure_count': 0, 'serial': serial_state, 'first': None, 'last': None,
         'phases': [], 'phase': phase_name, 'sample_rate_sps': 2560000,
         'expected_profiles': list(PROFILES)}

def checkpoint():
    state['failure_count'] = len(failures)
    tmp = out / 'status.tmp'
    tmp.write_text(json.dumps(state, indent=2))
    os.replace(tmp, out / 'status.json')

def run_phase(rate):
    global phase_name
    phase_name = f'{rate / 1000000:g} MS/s'
    state.update(phase=phase_name, sample_rate_sps=rate, samples=0, first=None, last=None)
    failure_start = len(failures)
    first = snapshot()
    validate(first, rate)
    state['first'] = first
    state['started_utc'] = utc()
    state['expected_end_utc'] = (dt.datetime.now(dt.timezone.utc) + dt.timedelta(seconds=duration)).isoformat()
    start = time.monotonic()
    previous, previous_time = None, None
    current = first
    state['state'] = 'RUNNING'
    while True:
        now = time.monotonic()
        if previous_time is not None and now - previous_time > 90:
            fail(f'Observation gap of {now - previous_time:.1f} seconds')
        if current is not None:
            try:
                validate(current, rate, first, previous)
            except Exception as exc:
                fail(str(exc))
            current['sample_rate_sps'] = rate
            with (out / f'snapshots-{rate}.jsonl').open('a') as log:
                log.write(json.dumps(current) + '\n')
            state['samples'] += 1
            state['last'] = current
            previous, previous_time = current, now
        state['elapsed_seconds'] = round(time.monotonic() - start, 3)
        checkpoint()
        print(json.dumps({'state': state['state'], 'elapsed_seconds': state['elapsed_seconds'],
                          'samples': state['samples'], 'failure_count': len(failures)}), flush=True)
        if time.monotonic() - start >= duration:
            break
        time.sleep(min(interval, duration - (time.monotonic() - start)))
        try:
            current = snapshot()
        except Exception as exc:
            current = None
            fail('API observation failed: ' + str(exc))
    if current is None:
        fail('Final snapshot unavailable')
    if state['last']['status']['uptime_us'] - first['status']['uptime_us'] < duration * 1000000:
        fail('Recorded board uptime span is shorter than one hour')
    for i in range(4):
        if state['last']['scanners'][i]['sweep_index'] <= first['scanners'][i]['sweep_index']:
            fail(f'RTL{i} completed no additional sweeps')
    phase_result = {'sample_rate_sps': rate, 'started_utc': state['started_utc'],
                    'finished_utc': utc(), 'elapsed_seconds': state['elapsed_seconds'],
                    'samples': state['samples'], 'failures': len(failures) - failure_start,
                    'first': first, 'last': state['last']}
    state['phases'].append(phase_result)
    checkpoint()

checkpoint()
try:
    if not ctypes.windll.kernel32.SetThreadExecutionState(0x80000001):
        raise RuntimeError('Unable to keep the soak host awake')
    run_phase(2560000)
    state['state'] = 'COMPLETED_FAIL' if failures else 'COMPLETED_PASS'
except Exception as exc:
    fail('Soak aborted: ' + str(exc))
    state['state'] = 'ABORTED'
finally:
    stop_serial.set()
    state['observation_mode'] = 'network API only; serial/watchdog acceptance unavailable'
    try:
        with http.open(Request(root + 'receivers/stop', data=b'', headers={'X-OrcNode-Confirm':'stop-all'}), timeout=15) as reply:
            state['stop_result'] = json.load(reply)
        if not state['stop_result'].get('ok'):
            fail('Final stop failed')
    except Exception as exc:
        fail('Final stop failed: ' + str(exc))
    ctypes.windll.kernel32.SetThreadExecutionState(0x80000000)
    state['finished_utc'] = utc()
    if failures and state['state'] == 'COMPLETED_PASS':
        state['state'] = 'COMPLETED_FAIL'
    checkpoint()
    print(json.dumps({'state': state['state'], 'samples': state['samples'],
                      'failure_count': len(failures)}), flush=True)

