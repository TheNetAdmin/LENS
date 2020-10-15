import os
import click
import struct


@click.group()
def cli():
    pass


@cli.command()
@click.option('--src_file', '-s', required=True, help='Result file to parse', type=click.Path(exists=True))
@click.option('--out_file', '-o', required=True, help='Output file')
def overwrite(src_file, out_file):
    '''Parse the overwrite test result'''
    fsize = os.stat(src_file).st_size
    fin = open(src_file, 'rb')
    with open(out_file, 'w') as f:
        for _ in range(fsize // 8):
            data = struct.unpack('L', fin.read(8))[0]
            f.write(str(data) + '\n')


if __name__ == "__main__":
    cli()
