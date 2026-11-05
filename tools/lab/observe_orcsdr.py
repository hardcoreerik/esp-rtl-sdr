"""Read-only OrcSDR integration gate; never flashes or authenticates devices."""
import argparse
import json
import re
import time
from pathlib import Path


def fields(line):
    return dict(re.findall(r'(\w+)=("[^"]*"|\S+)', line))


def validate(samples, board):
    if len(samples) < 3:
        raise ValueError('Need at least three complete observations')
    frequency = samples[0]['radio']['frequency_hz']
    for sample in samples:
        radio = sample['radio']
        if board == 'tab5':
            if radio.get('state') != 'STREAMING':
                raise ValueError('Tab5 is not streaming')
            if radio.get('shadow_ok') != '1' or radio.get('metrics_ok') != '1':
                raise ValueError('Driver consistency check failed')
        elif radio.get('connected') != '1' or radio.get('streaming') != '1':
            raise ValueError('Hosyond is not connected and streaming')
        if int(radio['effective_sps']) <= 0 or radio['frequency_hz'] != frequency:
            raise ValueError('Sample delivery stopped or frequency changed')
    if board == 'tab5':
        for previous, current in zip(samples, samples[1:]):
            for key in ('drops', 'overruns'):
                if current['radio'][key] != previous['radio'][key]:
                    raise ValueError('Driver loss counters increased: ' + key)
            if int(current['radio']['bytes']) <= int(previous['radio']['bytes']):
                raise ValueError('IQ byte counter stalled or reset')
    elif int(samples[-1]['screen']['visible_updates']) <= int(samples[0]['screen']['visible_updates']):
        raise ValueError('Display update counter did not advance')


def query(port, commands, board):
    received = []
    for command in commands:
        port.write((command + '\r\n').encode())
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            line = port.readline().decode(errors='replace').strip()
            if line.startswith(('RTL_DRIVER_STATUS ', 'RTL_SDR_STATUS ', 'RTL_SCREEN_STATUS ')):
                received.append(line)
    prefix = 'RTL_DRIVER_STATUS ' if board == 'tab5' else 'RTL_SDR_STATUS '
    radio = [fields(line) for line in received if line.startswith(prefix)]
    screen = [fields(line) for line in received if line.startswith('RTL_SCREEN_STATUS ')]
    if not radio or (board == 'hosyond' and not screen):
        raise ValueError('Missing required serial diagnostics')
    return {'radio': radio[-1], 'screen': screen[-1] if screen else None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['tab5', 'hosyond'], required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--usb-serial', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=60)
    args = parser.parse_args()
    if args.port.upper() == 'COM17':
        parser.error('COM17 is the protected control device')
    if args.seconds < 20:
        parser.error('Observation must last at least 20 seconds')
    if args.output.exists():
        parser.error('Preserve existing results; choose a new output file')
    import serial
    from serial.tools.list_ports import comports
    match = [p for p in comports() if p.device.upper() == args.port.upper()]
    if len(match) != 1 or match[0].serial_number != args.usb_serial:
        parser.error('USB serial identity mismatch; device not opened')
    result = {'board': args.board, 'port': args.port, 'usb_serial': args.usb_serial,
              'pass': False, 'samples': [], 'scope': 'read-only integration observation',
              'driver_revision_attested': False,
              'not_tested': ['RF decoding', 'uninstall', 'no-PSRAM allocation', 'USB hotplug',
                             'all attached dongles', 'physical display appearance']}
    port = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    try:
        port.open()
        commands = ['RTL_DRIVER STATUS'] if args.board == 'tab5' else ['RTL_STATUS', 'RTL_SCREEN_STATUS']
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            result['samples'].append(query(port, commands, args.board))
            time.sleep(3)
        validate(result['samples'], args.board)
        result['pass'] = True
    except Exception as error:
        result['error'] = str(error)
    finally:
        port.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({'board': args.board, 'pass': result['pass'], 'samples': len(result['samples']),
                      'error': result.get('error')}))
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
