import click
import random

base_id = 123123123
rnd = random.randrange(0, 9)

def generate_ids(num_records):
    return [base_id + i for i in range(num_records)]

def generate_records(num_records):
    res = []
    for nid in generate_ids(num_records):
        u = f'update basic set name = "{nid} {rnd}" where npi == {nid};'
        res.append(u);
    return '\n'.join(res)

def generate_location_records(num_records):
    res = []
    for nid in generate_ids(num_records):
        u = f'update location set city = "{nid} {rnd}" where npi == {nid};'
        res.append(u);
    return '\n'.join(res)

@click.command()
@click.argument('sql_file')
@click.argument('num_records')
def create_sql_file(sql_file, num_records):
    num_records = int(num_records)
    with open(sql_file, 'w') as f:
        records = generate_records(num_records)
        f.write(records)
    with open(f'{sql_file}.loc.sql', 'w') as f:
        records = generate_location_records(num_records)
        f.write(records)

if __name__== '__main__':
    create_sql_file()
