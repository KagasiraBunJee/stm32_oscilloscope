"""Source regression guards; physical SD negotiation still needs a board test."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SdRestart(unittest.TestCase):
    def test_mounted_card_is_reused_before_remount(self):
        source = (ROOT / 'Services/Src/storage.c').read_text()
        body = source.split('bool storage_mount(void)', 1)[1].split('void storage_process', 1)[0]
        self.assertLess(body.index('g_mounted && disk_status(0U) == 0U'),
                        body.index('g_mounted = mount_card()'))

    def test_hardware_reset_precedes_handle_reset(self):
        source = (ROOT / 'Services/Src/sd_diskio.c').read_text()
        body = source.split('static bool card_init(void)', 1)[1].split('DSTATUS disk_initialize', 1)[0]
        for operation in ('HAL_SD_DeInit', 'g_sd.Instance->MASK = 0U',
                          'g_sd.Instance->DCTRL = 0U', '__HAL_RCC_SDIO_FORCE_RESET',
                          '__HAL_RCC_SDIO_RELEASE_RESET', 'HAL_NVIC_ClearPendingIRQ'):
            self.assertLess(body.index(operation), body.index('memset(&g_sd'))
        self.assertNotIn('__HAL_RCC_DMA2_FORCE_RESET', body)

    def test_software_timeout_is_bounded_separately(self):
        source = (ROOT / 'Core/Inc/stm32f4xx_hal_conf.h').read_text()
        self.assertIn('#define SDMMC_SWDATATIMEOUT 1000U', source)
        self.assertNotIn('#define SDMMC_DATATIMEOUT', source)


if __name__ == '__main__':
    unittest.main()
