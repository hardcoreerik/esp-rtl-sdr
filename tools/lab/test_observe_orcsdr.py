import copy
import unittest
from observe_orcsdr import validate


class ObservationTests(unittest.TestCase):
    def samples(self):
        return [{'radio': dict(state='STREAMING', shadow_ok='1', metrics_ok='1',
                              frequency_hz='96100000', effective_sps='2400000',
                              drops='0', overruns='0', bytes=str(i*32768)),
                 'screen': None} for i in range(1, 4)]

    def test_progress(self):
        validate(self.samples(), 'tab5')

    def test_loss_and_stall_fail(self):
        for key, value in [('drops', '1'), ('overruns', '1'), ('bytes', '32768'),
                           ('frequency_hz', '99100000'), ('state', 'IDLE')]:
            samples = self.samples()
            samples[-1]['radio'][key] = value
            with self.assertRaises(ValueError, msg=key):
                validate(samples, 'tab5')

    def test_hosyond_display_and_stream(self):
        samples = [{'radio': dict(connected='1', streaming='1', frequency_hz='906875000',
                                  effective_sps='960000'), 'screen': {'visible_updates': str(i)}}
                   for i in range(3)]
        validate(samples, 'hosyond')
        stalled = copy.deepcopy(samples)
        stalled[-1]['screen']['visible_updates'] = '0'
        with self.assertRaises(ValueError):
            validate(stalled, 'hosyond')


if __name__ == '__main__':
    unittest.main()
