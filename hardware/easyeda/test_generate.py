"""Regression checks for the Standard-to-Pro component-linking defect."""
import unittest
import generate


class ImportLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schematic = generate.schematic()
        cls.pcb = generate.pcb()

    def test_instance_ids_match_across_documents(self):
        def ids(doc):
            return {s.split('#@$')[0].split('~')[6]
                    for s in doc['shape'] if s.startswith('LIB~')}
        self.assertEqual(ids(self.schematic), ids(self.pcb))
        self.assertEqual(len(ids(self.pcb)), 67)

    def test_pad_net_mapping(self):
        by_id = {p['schematic_id']: p for p in generate.parts}
        for lib in self.pcb['shape']:
            if not lib.startswith('LIB~'):
                continue
            header, *shapes = lib.split('#@$')
            expected = by_id[header.split('~')[6]]['pins']
            actual = {}
            for shape in shapes:
                fields = shape.split('~')
                if fields[0] == 'PAD':
                    actual[fields[8]] = fields[7]
            self.assertEqual(expected, actual)

    def test_output_header_clears_mounting_holes(self):
        connector = next(p for p in generate.parts if p['ref'] == 'J8')
        # Conservative bounding-circle pad check including 0.5 mm clearance.
        for _, dx, dy, w, h, _ in generate.pads(connector):
            for hx, hy in [(3.5,3.5),(66.5,3.5),(3.5,86.5),(66.5,86.5)]:
                d = ((connector['x_mm']+dx-hx)**2 + (connector['y_mm']+dy-hy)**2)**.5
                self.assertGreater(d, 1.6 + (w*w+h*h)**.5/2 + .5)


if __name__ == '__main__':
    unittest.main()
