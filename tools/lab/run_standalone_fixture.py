"""Explicitly opted-in COM39 standalone test with mandatory image recovery."""
import argparse
import hashlib
import json
import re
import subprocess
import time
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_target(port, serial_number):
    if not re.fullmatch(r'COM[1-9][0-9]*', port.upper()):
        raise ValueError('Expected a canonical COM port')
    if port.upper() != 'COM39' or serial_number.upper() != 'E8:F6:0A:E2:D6:03':
        raise ValueError('This fixture is enrolled only for the test Tab5 COM39')


def assess(text, expected_profile):
    text = '\n'.join(text.splitlines())
    if any(marker in text for marker in ('Guru Meditation', 'abort() was called', 'panic_abort')):
        raise ValueError('Device panic/abort observed')
    if expected_profile not in text:
        raise ValueError('Expected dongle profile not observed')
    overall = re.search(r'^SMOKE OVERALL PASS passed=(\d+) failed=0 skipped=0 hardware=RUN$', text, re.M)
    if not overall or not re.search(r'^SMOKE uninstall PASS$', text, re.M):
        raise ValueError('Hardware smoke/uninstall did not pass without skips')
    soak = re.search(r'^SOAK \S+ .*?sps=(\d+) bytes=(\d+) over=0 drops=0 short=0 .*?advice=ok window_ms=(\d+)$', text, re.M)
    if not soak or int(soak[1]) < 940000 or int(soak[2]) <= 0 or int(soak[3]) < 8000:
        raise ValueError('Missing healthy eight-second quiet soak')
    return {'checks_passed': int(overall[1]), 'quiet_sps': int(soak[1]),
            'quiet_bytes': int(soak[2]), 'quiet_ms': int(soak[3]), 'uninstall_pass': True}


def exercise(flash, capture, resume):
    """Recovery is attempted even if the initial flash partially fails."""
    result = {'test_pass': False, 'restored_image': False, 'resumed_application': False}
    try:
        flash('smoke')
        result.update(capture())
        result['test_pass'] = True
    except Exception as error:
        result['test_error'] = str(error)
    finally:
        try:
            flash('restore')
            result['restored_image'] = True
            resume()
            result['resumed_application'] = True
        except Exception as error:
            result['recovery_error'] = str(error)
    result['pass'] = all(result[key] for key in ('test_pass', 'restored_image', 'resumed_application'))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('smoke-image', 'sdkconfig', 'restore-image', 'esptool-python', 'app-root', 'pairing-key', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--restore-sha256', required=True)
    p.add_argument('--smoke-sha256', required=True)
    p.add_argument('--port', default='COM39')
    p.add_argument('--usb-serial', default='E8:F6:0A:E2:D6:03')
    p.add_argument('--expected-profile', default='nooelec_smart_v5_r820t2')
    p.add_argument('--allow-flash', action='store_true')
    args = p.parse_args()
    validate_target(args.port, args.usb_serial)
    if not args.allow_flash:
        p.error('Explicit --allow-flash required; no device opened')
    for path in (args.smoke_image, args.sdkconfig, args.restore_image, args.esptool_python, args.pairing_key):
        if not path.is_file():
            p.error('Required local file missing: ' + str(path))
    if digest(args.restore_image) != args.restore_sha256.lower() or digest(args.smoke_image) != args.smoke_sha256.lower():
        p.error('Image hash mismatch; no flash attempted')
    config = args.sdkconfig.read_text(encoding='utf-8')
    if re.search(r'^CONFIG_SPIRAM=y$', config, re.M) or not re.search(r'^# CONFIG_SPIRAM is not set$', config, re.M):
        p.error('Fixture requires a PSRAM-disabled build')
    if 'CONFIG_ESP_RTL_SDR_SMOKE_BOARD_TAB5=y' not in config:
        p.error('Fixture requires the Tab5 board profile')
    if args.output.exists():
        p.error('Preserve existing output; choose a new directory')
    runner = args.app_root / 'apps/orcsdr-tab5/tools/run-tab5-ui-regression.ps1'
    if not runner.is_file():
        p.error('Trusted OrcSDR serial runner missing')
    import serial
    from serial.tools.list_ports import comports
    devices = [d for d in comports() if d.device.upper() == args.port.upper()]
    if len(devices) != 1 or (devices[0].serial_number or '').upper() != args.usb_serial.upper():
        p.error('Device USB identity mismatch; no flash attempted')
    args.output.mkdir(parents=True)
    resume_config = args.output / 'resume-config.json'
    resume_config.write_text(json.dumps({'runner': str(runner.resolve()), 'key': str(args.pairing_key.resolve()),
                                         'port': args.port, 'output': str(args.output.resolve())}), encoding='utf-8')

    def flash(which):
        image = args.smoke_image if which == 'smoke' else args.restore_image
        expected = args.smoke_sha256 if which == 'smoke' else args.restore_sha256
        if digest(image) != expected.lower():
            raise ValueError('Image changed after preflight: ' + which)
        with (args.output / (which + '-flash.log')).open('w') as log:
            subprocess.run([str(args.esptool_python), '-m', 'esptool', '--port', args.port,
                            '--baud', '921600', 'write-flash', '0x10000', str(image)],
                           check=True, timeout=120, stdout=log, stderr=subprocess.STDOUT)

    def capture():
        port = serial.Serial(port=None, baudrate=115200, timeout=.3)
        port.dtr = False
        port.rts = False
        port.port = args.port
        text = ''
        try:
            port.open()
            deadline = time.monotonic() + 180
            with (args.output / 'smoke-serial.log').open('w', encoding='utf-8') as log:
                while time.monotonic() < deadline:
                    line = port.readline().decode(errors='replace')
                    text += line
                    log.write(line)
                    log.flush()
                    if any(marker in line for marker in ('SMOKE OVERALL', 'Guru Meditation', 'abort() was called')):
                        break
        finally:
            port.close()
        return assess(text, args.expected_profile)

    def resume():
        # USB enumeration alone does not prove the rebooted application's CLI is ready.
        deadline = time.monotonic() + 60
        ready = False
        while time.monotonic() < deadline:
            probe = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=1)
            probe.dtr = False
            probe.rts = False
            probe.port = args.port
            try:
                probe.open()
                probe.write(b'\r\nRTL_DRIVER STATUS\r\n')
                until = time.monotonic() + 2
                while time.monotonic() < until:
                    if probe.readline().startswith(b'RTL_DRIVER_STATUS '):
                        ready = True
                        break
            except serial.SerialException:
                pass
            finally:
                probe.close()
            if ready:
                break
            time.sleep(1)
        if not ready:
            raise RuntimeError('Restored application CLI did not respond within 60 seconds')
        subprocess.run(['pwsh', '-NoProfile', '-File', str(Path(__file__).with_name('resume_tab5.ps1')),
                        '-ConfigPath', str(resume_config)], check=True, timeout=120,
                       stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    result = exercise(flash, capture, resume)
    result.update({'port': args.port, 'usb_serial': args.usb_serial, 'smoke_sha256': args.smoke_sha256,
                   'restore_sha256': args.restore_sha256, 'sdkconfig_sha256': digest(args.sdkconfig)})
    (args.output / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
