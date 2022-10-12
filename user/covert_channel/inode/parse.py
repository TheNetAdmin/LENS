import click
import pathlib
import re
import csv

@click.command()
@click.argument('task_path')
def parse(task_path):
    task_path = pathlib.Path(task_path)
    result = []
    bit_id = 0
    with open(task_path / 'receiver.log', 'r') as f:
        for line in f:
            if not line.startswith('Latency Summary:'):
                continue
            else:
                break
        for line in f:
            lat = int(re.findall(r'(?:\[\s*25000.*45000\].*?)(\d+)', line)[0])
            result.append(
                {
                    'iter': bit_id,
                    'long_lat': lat,
                }
            )
            bit_id += 1
    with open(task_path / 'receiver.csv', 'w') as f:
        writer = csv.DictWriter(f, fieldnames=result[0].keys())
        writer.writeheader()
        writer.writerows(result)

if __name__ == '__main__':
    parse()
