import unittest
from run_standalone_fixture import assess, exercise, validate_target


GOOD = '''nooelec_smart_v5_r820t2
SOAK usb_soak_960k_6x16k eff=99 sps=959488 bytes=15351808 over=0 drops=0 short=0 urbs=6x16384 advice=ok window_ms=8000
SMOKE uninstall PASS
SMOKE OVERALL PASS passed=46 failed=0 skipped=0 hardware=RUN
'''


class FixtureTests(unittest.TestCase):
    def test_hardware_result(self):
        self.assertEqual(assess(GOOD, 'nooelec_smart_v5_r820t2')['checks_passed'], 46)
        self.assertEqual(assess(GOOD.replace('\n', '\r\r\n'), 'nooelec_smart_v5_r820t2')['checks_passed'], 46)

    def test_incomplete_or_unhealthy_results_rejected(self):
        for bad in (GOOD.replace('hardware=RUN', 'hardware=SKIP'),
                    GOOD.replace('drops=0', 'drops=1'),
                    GOOD.replace('uninstall PASS', 'uninstall FAIL'),
                    GOOD + 'Guru Meditation', GOOD.replace('sps=959488', 'sps=840000')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                assess(bad, 'nooelec_smart_v5_r820t2')

    def test_protected_or_wrong_device_rejected(self):
        validate_target('COM39', 'E8:F6:0A:E2:D6:03')
        for port, serial in [('COM17', '30:ED:A0:E2:E6:95'), ('COM017', 'E8:F6:0A:E2:D6:03'),
                             ('COM39', '30:ED:A0:E2:E6:95')]:
            with self.assertRaises(ValueError):
                validate_target(port, serial)

    def test_recovery_after_partial_flash_failure(self):
        calls = []
        def flash(which):
            calls.append(which)
            if which == 'smoke':
                raise RuntimeError('partial write')
        result = exercise(flash, lambda: {}, lambda: calls.append('resume'))
        self.assertEqual(calls, ['smoke', 'restore', 'resume'])
        self.assertFalse(result['pass'])
        self.assertTrue(result['resumed_application'])

    def test_test_and_recovery_failure_reported_separately(self):
        def fail():
            raise RuntimeError('test timeout')
        def flash(which):
            if which == 'restore':
                raise RuntimeError('recovery failure')
        result = exercise(flash, fail, lambda: None)
        self.assertIn('test_error', result)
        self.assertIn('recovery_error', result)
        self.assertFalse(result['pass'])


if __name__ == '__main__':
    unittest.main()
