import re
import csv
import click

def parse_result(src_file):
    with open(src_file, 'r') as f:
        content = f.read()
    stat_line=r'\[[\s\d\.]+\]\s\{\d+\}.*'
    stats = re.findall(stat_line, content)

    res = []
    for stat in stats:
        stat_entries = stat.split(',')
        res_entry = {}
        for se in stat_entries:
            for column in se.split(','):
                c = column.lstrip().rstrip().rstrip('.').split(' ')
                if c[0].startswith('['):
                    # The first column, for e.g. '[ 9871.210131] {0}[pointer-chasing-4096] region_size 67108864'
                    res_entry['task_name'] = c[-3].split('[')[1].rstrip(']')
                    res_entry[c[-2]] = int(c[-1])
                elif c[0] == 'cycle':
                    res_entry['cycle_write_start'] = int(c[1])
                    res_entry['cycle_read_start'] = int(c[3])
                    res_entry['cycle_read_end'] = int(c[5])
                else:
                    res_entry[c[0]] = c[1]
        res.append(res_entry)
    
    return res


# Get average time (in ns) per cacheline for read or write operations
def get_average_ns(stats, cycle_type, region_size, block_size):
    assert cycle_type in ['read', 'write']
    
    res = -1
    for s in stats:
        if (int(s['region_size']) == region_size) and (int(s['block_size']) == block_size):
            total_cycle = s['cycle_read_end'] - s['cycle_write_start']
            write_cycle = s['cycle_read_start'] - s['cycle_write_start']
            read_cycle = s['cycle_read_end'] - s['cycle_read_start']
            ns = float(s['total']) / float(s['count']) / float(s['region_size']) * 64

            if cycle_type == 'read':
                res = float(read_cycle) / float(total_cycle) * ns
            elif cycle_type == 'write':
                res = float(write_cycle) / float(total_cycle) * ns

            break

    if len(stats) == 0:
        print('Input stats is empty')
        exit(1)
    
    if res == -1:
        print(f'Missing data for region_size {region_size} block_size {block_size}')
        exit(1)
    
    return res


def calc_amplification(stats, amp_type, region_size_1, region_size_2, block_size):
    ns1 = get_average_ns(stats, amp_type, region_size_1, block_size)
    ns2 = get_average_ns(stats, amp_type, region_size_2, block_size)
    return ns2 / ns1


@click.group()
def cli():
    pass


@cli.command()
@click.option('--src_file', '-s', required=True, help='Result file to parse')
@click.option('--out_file', '-o', required=True, help='Output csv file')
def amplification(src_file, out_file):
    '''Parse the amplification test result.'''
    data = parse_result(src_file)
    block_sizes = set([ int(d['block_size']) for d in data ])
    res = []
    for block_size in block_sizes:
        r = {}
        r['block_size'] = block_size
        r['rmw_buf_read_amp'] = calc_amplification(data, 'read', 2 ** 14, 2 ** 24, block_size)
        r['ait_buf_read_amp'] = calc_amplification(data, 'read', 2 ** 24, 2 ** 29, block_size)
        r['lsq_write_amp'] = calc_amplification(data, 'write', 2 ** 12, 2 ** 24, block_size)
        if block_size == 64:
            r['wpq_write_amp'] = calc_amplification(data, 'write', 512, 64, block_size)
        elif block_size == 256:
            r['wpq_write_amp'] = calc_amplification(data, 'write', 512, 256, block_size)
        else:
            r['wpq_write_amp'] = 1 
        res.append(r)
    with open(out_file, 'w') as f:
        writer = csv.DictWriter(f, fieldnames=res[0].keys())
        writer.writeheader()
        writer.writerows(res)

@cli.command()
@click.option('--src_file', '-s', required=True, help='Result file to parse')
@click.option('--out_file', '-o', required=True, help='Output csv file')
def pointer_chasing(src_file, out_file):
    '''Parse the pointer_chasing test result.'''
    data = parse_result(src_file)
    with open(out_file, 'w') as f:
        writer = csv.DictWriter(f, fieldnames=data[0].keys())
        writer.writeheader()
        writer.writerows(data)


if __name__ == "__main__":
    cli()
