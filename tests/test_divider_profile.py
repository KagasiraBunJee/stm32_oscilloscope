"""Host-side numeric and source-contract checks; not ADC hardware validation."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class DividerProfile(unittest.TestCase):
    def test_nominal_and_worst_case_pin_voltage(self):
        self.assertAlmostEqual(14 * 10 / 57, 2.45614035, places=7)
        self.assertLess(14 * 10.1 / (47 * .99 + 10.1), 2.5)

    def test_fixed_point_conversion(self):
        gain = 18_810_000 * 65536 // 4095
        self.assertLess(gain, 2**31)
        for voltage in (0, 1, 12, 13.5, 14):
            raw = round(voltage / 5.7 / 3.3 * 4095)
            result_uv = raw * gain // 65536
            self.assertLess(abs(result_uv / 1e6 - voltage), .0024)

    def test_long_sampling_fits_divider_timer(self):
        conversion_seconds = (480 + 12) / 36_000_000
        self.assertLess(conversion_seconds, 1 / 10_000)
        self.assertEqual(72_000_000 // 10_000, 7200)

    def test_profile_contracts(self):
        cal = (ROOT / 'Services/Src/calibration.c').read_text()
        acq = (ROOT / 'Services/Src/acquisition.c').read_text()
        ui = (ROOT / 'App/Src/oscilloscope_ui.c').read_text()
        self.assertIn('#define CALIBRATION_VERSION 2U', cal)
        self.assertIn('18810000LL * 65536LL / 4095LL', cal)
        self.assertNotIn('high->offset_uv = -15000000', cal)
        self.assertIn('max_rate = acquisition_max_sample_rate(config);', acq)
        self.assertIn('ADC_SAMPLETIME_480CYCLES', acq)
        self.assertIn('g_interleaved = (g_enabled_count == 1U) && !g_divider_sampling;', acq)
        self.assertIn('CH1 14V', ui)
        self.assertNotIn('CH1 15V', ui)


if __name__ == '__main__':
    unittest.main()
