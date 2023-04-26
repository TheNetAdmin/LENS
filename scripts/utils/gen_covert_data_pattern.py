import click
import random
import struct

def gen_byte(pattern: int) -> int:
    if pattern == 0:
        return 0xaa
    elif pattern == 1:
        return 0xcc
    elif pattern == 2:
        return 0xf0
    elif pattern == 3:
        return random.randint(0x00, 0xff)

def gen_64bit(pattern: int) -> int:
    d = 0
    for i in range(8):
        d |= gen_byte(pattern) << (i * 8)
    return d

@click.command()
@click.option("-b", "--total_bits", required=True, default=64)
@click.option("-o", "--output_file", required=True)
@click.option('-p', '--pattern', required=True, type=int)
def gen_pattern(total_bits, output_file, pattern):
    """
    Pattern:
        0: 0b01010101....
        1: 0b00110011....
        2: 0b00001111....
        3: random
    """

    assert(total_bits >= 64)
    assert(total_bits % 64 == 0)
    assert(0 <= pattern <= 3)
    data = []
    # Setup stage
    setup_data = 0xcc # First 8 bits are for setup, always 0xcc
    for i in range(7):
        setup_data |= gen_byte(pattern) << ((i + 1) * 8)
    data.append(setup_data)
    for _ in range(total_bits // 64 - 1):
        data.append(gen_64bit(pattern))
    
    with open(output_file, 'wb') as f:
        for d in data:
            b = struct.pack('Q', d)
            f.write(b)


if __name__ == "__main__":
    gen_pattern()
