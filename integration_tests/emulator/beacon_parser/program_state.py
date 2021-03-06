from emulator.beacon_parser.units import Hex16
from parser import CategoryParser


class ProgramStateParser(CategoryParser):
    def __init__(self, reader, store):
        CategoryParser.__init__(self, '02: Program State', reader, store)

    def get_bit_count(self):
        return 2*8

    def parse(self):
        self.append_word("Program CRC", value_type=Hex16)
