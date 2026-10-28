"""Source guards for cooperative scheduling; not hardware timing tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UIScheduling(unittest.TestCase):
    def test_sd_fast_path_does_not_render_immediately(self):
        source = (ROOT / 'Services/Src/sd_diskio.c').read_text()
        self.assertEqual(source.count(
            'if ((uint32_t)(board_millis() - start) >= 10U) recording_io_yield();'), 2)

    def test_storage_backlog_has_bounded_priority(self):
        source = (ROOT / 'App/Src/oscilloscope_app.c').read_text()
        main = source.split('void oscilloscope_app_process(void)', 1)[1]
        self.assertIn('while(status.queue_depth', main)
        self.assertIn('(uint32_t)(board_millis()-started)<20U', main)
        self.assertLess(main.index('recording_process();'),
                        main.index('oscilloscope_app_io_yield();'))
        hook = source.split('void oscilloscope_app_io_yield(void)', 1)[1].split(
            'void oscilloscope_app_init(void)', 1)[0]
        self.assertNotIn('acquisition_process();', hook)
        self.assertIn('acquisition_process();', main)

    def test_channel_selection_survives_refresh(self):
        source = (ROOT / 'App/Src/oscilloscope_ui.c').read_text()
        self.assertIn('return g_settings_channel;', source)
        self.assertNotIn('update_voltage_scale(capture->channel[0].logical_channel)', source)
        self.assertIn('capture->channel[index].logical_channel == logical', source)
        self.assertIn('channel == g_settings_channel ? 3 : 0', source)


if __name__ == '__main__':
    unittest.main()
