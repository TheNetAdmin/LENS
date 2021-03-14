import click

base_id = 123123123


def generate_ids(num_records):
    return [base_id + i for i in range(num_records)]


def generate_records(num_records):
    res = []
    for i in range(num_records):
        res.append((base_id + i, f"name {i}", f"legal {i}"))
    res_str = [str(r) for r in res]
    return ",".join(res_str)


def generate_deletion(num_records):
    res = []
    for i in range(num_records):
        res.append(f"npi == {base_id+i}")
    return res


def generate_location_records(num_records):
    res = []
    for nid in generate_ids(num_records):
        res.append((nid, f"city {nid}", f"state {nid}"))
    return ",".join([str(r) for r in res])


def generate_location_records_deletion(num_records):
    res = []
    for nid in generate_ids(num_records):
        res.append(f"npi == {nid}")
    return res


@click.command()
@click.argument("sql_file")
@click.argument("num_records")
def create_sql_file(sql_file, num_records):
    num_records = int(num_records)
    with open(sql_file, "w") as f:
        records = generate_records(num_records)
        f.write(f"insert into basic values {records};\n")
    with open(f"{sql_file}.del.sql", "w") as f:
        deletes = generate_deletion(num_records)
        for d in deletes:
            f.write(f"delete from basic where {d};\n")
    with open(f"{sql_file}.loc.sql", "w") as f:
        records = generate_location_records(num_records)
        f.write(f"insert into location values {records};\n")
    with open(f"{sql_file}.loc.del.sql", "w") as f:
        deletes = generate_location_records_deletion(num_records)
        for d in deletes:
            f.write(f"delete from location where {d};\n")


if __name__ == "__main__":
    create_sql_file()
