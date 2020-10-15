import csv
import click
import numpy as np


def calc_latency_per_cache_line(res, region_size, lat_type):
    assert lat_type in ['read', 'write']

    if region_size not in res.keys():
        print('Error: no result collected with region_size={region_size}')
        exit(1)

    r = res[region_size]
    cws = float(r['cycle_write_start'])
    crs = float(r['cycle_read_start'])
    cre = float(r['cycle_read_end'])
    ns = float(r['total']) / float(r['count'])

    ret = -1
    if lat_type == 'read':
        ret = (cre-crs) / (cre-cws) * ns / region_size * 64
    elif lat_type == 'write':
        ret = (crs-cws) / (cre-cws) * ns / region_size * 64

    return ret


@click.command()
@click.option('--src_file', '-s', required=True, type=click.Path(exists=True))
def latency(src_file):
    '''Calcule read latency of each level in the on-DIMM buffer hierarchy.'''
    res = {}
    with open(src_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row['block_size'] == '64':
                res[int(row['region_size'])] = row

    # x : l1 buf read latency
    # y : l2 buf read latency
    # z : media read lantecy

    # 1 * x + (64 / 256) * y + (256 / 4096) * z = read_lat_16KB
    # 1 * x + 1 * y + (256 / 4096) * z = read_lat_16MB
    # 1 * x + 1 * y + 1 * z = read_lat_1GB
    A = np.mat(f'1 , {(64/256)}, {(256/4096)}; 1, 1, {(256/4096)}; 1,1,1')
    B = np.mat(f'{calc_latency_per_cache_line(res, 16*1024, "read")}, '
               f'{calc_latency_per_cache_line(res, 16*1024*1024, "read")}, '
               f'{calc_latency_per_cache_line(res, 1024*1024*1024, "read")+40}')
    r = np.linalg.solve(A, B.T)
    print(f'read latency (ns):')
    print(f'    rmw buf: {float(r[0])}')
    print(f'    ait buf: {float(r[1])}')
    print(f'    media  : {float(r[2])}')


if __name__ == "__main__":
    latency()
