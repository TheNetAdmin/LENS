import os
import click
import struct


@click.group()
def cli():
    pass


@cli.command()
@click.option('--src_file', '-s', required=True, help='Result file to parse', type=click.Path(exists=True))
@click.option('--out_file', '-o', required=True, help='Output file')
def wear_leveling(src_file, out_file):
    '''Parse the wear_leveling test result'''
    fsize = os.stat(src_file).st_size
    fin = open(src_file, 'rb')
    iter = 1
    with open(out_file, 'w') as f:
        f.write('iter,lat\n')
        for _ in range(fsize // 8):
            data = struct.unpack('L', fin.read(8))[0]
            f.write(str(iter) + "," + str(data) + '\n')
            iter += 1


if __name__ == "__main__":
    cli()
