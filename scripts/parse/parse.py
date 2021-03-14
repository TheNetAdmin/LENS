import re
import csv
from rich.console import Console
console = Console()

with open('agg.log', 'r') as f:
    content = f.read()

content = re.sub(r'\n\s+\,', ',', content)

res = []


for l in content.split('\n'):
    # print(l)
    if l.strip() == '':
        continue
    r = dict()
    for field in l.split(','):
        field = field.strip()
        if field == '':
            continue
        k, v = field.split('=')
        k = k.strip()
        v = v.strip()
        r[k] = v
    r['c_w_beg'], r['c_w_end'], r['c_r_end'] = r['cycle'].split(':')
    r['cycle_write'] = int(r['c_w_end']) - int(r['c_w_beg'])
    r['cycle_read'] = int(r['c_r_end']) - int(r['c_w_end'])
    # console.print(r)
    for k in r.keys():
        if k.startswith('lat_') or k.startswith('cycle_'):
            print(f'Processing: {k}')
            r[k] = int(float(r[k]) / float(r['count']) / (int(r['region_size']) / int(r['block_size'])))
    r['ns_write'] = int(float(r['cycle_write']) / (int(r['c_r_end']) - int(r['c_w_end'])) * int(r['average'].split(' ')[0]) / int(r['repeat']))
    r['ns_read'] = int(float(r['cycle_read']) / (int(r['c_r_end']) - int(r['c_w_end'])) * int(r['average'].split(' ')[0]) / int(r['repeat']))
    res.append(r)


keys = set()
for r in res:
    for k in r.keys():
        keys.add(k)

with open('agg.csv', 'w') as f:
    keys = list(keys)
    keys.sort()
    writer = csv.DictWriter(f, fieldnames=keys)
    writer.writeheader()
    writer.writerows(res)

