"""Check a complete PC campaign's saved data; does not access hardware."""
import hashlib
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(sys.argv[1])


def require(condition, message):
    if not condition:
        raise SystemExit('FAIL: ' + message)


def events(folder):
    return json.loads((root / folder / 'events.json').read_text())


delivery = events('sustained-and-boundaries')
rates = [e for e in delivery if e['stage'] == 'sustained_iq']
require({e['rate'] for e in rates} ==
        {250000, 960000, 1024000, 2048000, 2400000, 2560000, 2880000, 3200000},
        'missing delivery rate')
require(all(e['elapsed'] >= 5 and not e['errors'] and not e['short_reads']
            for e in rates), 'incomplete or failed delivery observation')
restarts = [e for e in delivery if e['stage'] == 'restart_iq']
require(len(restarts) == 5 and all(e['rc'] == 0 and e['bytes'] == 262144
                                 for e in restarts), 'restart observations')

folder = root / 'tinysa-paused-matrix'
rf = events(folder.name)
blocks = [e for e in rf if 'artifact' in e]
require(len(blocks) == 40, 'RF matrix incomplete')
for row in blocks:
    data = (folder / row['artifact']).read_bytes()
    require(len(data) == 524288, 'IQ byte length: ' + row['artifact'])
    require(hashlib.sha256(data).hexdigest() == row['sha256'],
            'IQ hash: ' + row['artifact'])
    require(row['clip_fraction'] == 0, 'clipped RF observation')
on = next(e for e in rf if e['stage'] == 'source_on')
off = next(e for e in rf if e['stage'] == 'source_off')
require(on['tone_dbfs'] - off['tone_dbfs'] > 15, 'source-on tone prerequisite')
last = [e for e in rf if e['stage'] == 'source_command'][-1]
require(last['command'] == 'output off' and last['response'].endswith('ch> '),
        'final output-off acknowledgement missing')

for name in ['controls-verified', 'sustained-and-boundaries', 'tinysa-paused-matrix']:
    proc = subprocess.run([
        r'C:\Program Files\Wireshark\tshark.exe', '-r',
        str(root / name / 'controls.pcapng'), '-Y',
        'usb.bmRequestType==0xc0 || usb.bmRequestType==0x40',
        '-T', 'fields', '-e', 'frame.number'], capture_output=True, text=True)
    require(proc.returncode == 0 and bool(proc.stdout.strip()),
            'missing/unreadable vendor control capture: ' + name)

print('PASS: saved delivery/restart observations, 40 RF IQ hashes/lengths, '
      'tone prerequisite, no RF clipping, output-off acknowledgement, '
      'and nonempty vendor control captures. Decode pairing and RF setup '
      'still require the documented separate checks.')
