import binascii
import pathlib
import struct
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
OSC_MAGIC = 0x3143534F
USB_MAGIC = 0x3150534F
USB_HEADER = struct.Struct("<IBBHHHIII")


def crc32(data: bytes, previous: int = 0) -> int:
    return binascii.crc32(data, previous) & 0xFFFFFFFF


def make_osc() -> bytes:
    samples = ((0, 4095), (100, 3995), (2048, 2048), (4095, 0))
    payload = b"".join(struct.pack("<HH", *pair) for pair in samples)
    header = bytearray(256)
    struct.pack_into("<IHHIIQII", header, 0, OSC_MAGIC, 1, 256,
                     256 + len(payload), 7, 1_788_284_800, 3, 0x00010000)
    struct.pack_into("<IHHHBBBBBB", header, 32, 2_400_000, 4, 2, 2048,
                     2, 1, 0, 0, 0, 0)
    struct.pack_into("<II", header, 168, len(payload), crc32(payload))
    struct.pack_into("<I", header, 176, 0)
    struct.pack_into("<I", header, 176, crc32(header))
    return bytes(header) + payload


def validate_osc(blob: bytes) -> bool:
    if len(blob) < 256:
        return False
    header = bytearray(blob[:256])
    magic, version, header_size, total = struct.unpack_from("<IHHI", header, 0)
    payload_size, payload_crc, saved_header_crc = struct.unpack_from("<III", header, 168)
    struct.pack_into("<I", header, 176, 0)
    return (magic, version, header_size, total) == (OSC_MAGIC, 1, 256, len(blob)) \
        and payload_size == len(blob) - 256 \
        and saved_header_crc == crc32(header) \
        and payload_crc == crc32(blob[256:])


def make_usb(command: int, sequence: int, payload: bytes = b"") -> bytes:
    header = USB_HEADER.pack(USB_MAGIC, 1, 24, command, 0, 0,
                             sequence, len(payload), 0)
    checksum = crc32(payload, crc32(header))
    return USB_HEADER.pack(USB_MAGIC, 1, 24, command, 0, 0,
                           sequence, len(payload), checksum) + payload


class BinaryContracts(unittest.TestCase):
    def test_analog_front_end_transfer(self):
        reference = 3.3 * 100_000 / (80_000 + 100_000)
        outputs = [0.1 * value + 0.9 * reference for value in (-15.0, 0.0, 15.0)]
        self.assertAlmostEqual(reference, 1.833333333, places=6)
        self.assertEqual([round(value, 3) for value in outputs], [0.15, 1.65, 3.15])

    def test_nominal_sample_timer_rates(self):
        timer_clock = 72_000_000
        self.assertEqual(timer_clock // 30, 2_400_000)
        self.assertEqual(3 * (timer_clock // 30), 7_200_000)

    def test_osc_header_and_payload_crc(self):
        blob = make_osc()
        self.assertTrue(validate_osc(blob))
        damaged = bytearray(blob)
        damaged[-1] ^= 0x01
        self.assertFalse(validate_osc(damaged))

    def test_osc_sample_layout_is_sample_major(self):
        payload = make_osc()[256:]
        self.assertEqual(struct.unpack("<8H", payload),
                         (0, 4095, 100, 3995, 2048, 2048, 4095, 0))

    def test_usb_frame_crc_and_length(self):
        frame = make_usb(0x0010, 42, b"configuration")
        values = USB_HEADER.unpack_from(frame)
        self.assertEqual(USB_HEADER.size, 24)
        self.assertEqual(values[0:3], (USB_MAGIC, 1, 24))
        self.assertEqual(values[6:8], (42, len(b"configuration")))
        zeroed = bytearray(frame[:24])
        struct.pack_into("<I", zeroed, 20, 0)
        self.assertEqual(values[8], crc32(frame[24:], crc32(zeroed)))

    def test_sources_publish_required_usb_identity(self):
        source = (ROOT / "Services/Src/usb_device.c").read_text(encoding="utf-8")
        protocol = (ROOT / "docs/USB_PROTOCOL.md").read_text(encoding="utf-8")
        self.assertIn("0x09, 0x12, 0x07, 0xF4", source)
        self.assertIn("'W','I','N','U','S','B'", source)
        self.assertIn("{5D4E3F66-6A90-4A57-9C0C-6F6C1B8A407E}", protocol)

    def test_linker_enforces_sram_headroom(self):
        linker = (ROOT / "stm32f407xe_flash.ld").read_text(encoding="utf-8")
        self.assertIn("_ram_reserved_end) >= 16K", linker)


if __name__ == "__main__":
    unittest.main()
