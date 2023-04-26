import sqlite3
import click
import csv
import logging
from pathlib import Path

logging.basicConfig(
    format="%(asctime)s %(levelname)-8s %(message)s",
    level=logging.INFO,
    datefmt="%Y-%m-%d %H:%M:%S",
)


@click.group()
def gen_db():
    pass


@gen_db.command()
@click.option(
    "-d",
    "--data_file",
    required=True,
    default="../data/nppes_npi/npidata_pfile_20050523-20220508.csv",
)
@click.option(
    "-o", "--out_sqlite_file", required=True, default="../data/nppes_npi/npi_data_basic.db"
)
def gen_sqlite_basic(data_file, out_sqlite_file):
    if Path(out_sqlite_file).exists():
        raise FileExistsError(f"File [{out_sqlite_file}] exists, please delete it first")

    conn = sqlite3.connect(out_sqlite_file)
    c = conn.cursor()

    c.execute(
        "CREATE TABLE basic (npi INTEGER NOT NULL, name TEXT, legal_business_name TEXT, PRIMARY KEY(npi))"
    )
    c.execute(
        "CREATE TABLE location (npi INTEGER NOT NULL, city TEXT, state TEXT, FOREIGN KEY (npi) REFERENCES basic (npi))"
    )
    conn.commit()

    basic_list = []
    loc_list = []
    ignored_lines = 0

    logging.info("Started")

    with open(data_file, "r") as f:
        reader = csv.DictReader(f)
        for index, data in enumerate(reader):
            if index % 100000 == 0:
                logging.info(f"Progress: {index}")

            name = (
                data["Provider First Name"]
                + " "
                + data["Provider Last Name (Legal Name)"]
            )
            if name.strip() == "":
                ignored_lines += 1
                continue

            basic_list.append(
                (
                    data["NPI"],
                    name,
                    data["Provider Organization Name (Legal Business Name)"],
                )
            )
            loc_list.append(
                (
                    data["NPI"],
                    data["Provider Business Mailing Address City Name"],
                    data["Provider Business Mailing Address State Name"],
                )
            )

    logging.info(f"To be inserted: {len(basic_list)}")
    logging.info(f"Ignored       : {ignored_lines}")

    c.executemany("INSERT INTO basic VALUES (?, ?, ?)", basic_list)
    c.executemany("INSERT INTO location VALUES (?, ?, ?)", loc_list)
    conn.commit()

    conn.close()


def convert_date(orig_date):
    return orig_date.replace("/", "-")


@gen_db.command()
@click.option(
    "-d",
    "--data_file",
    required=True,
    default="../data/nppes_npi/npidata_pfile_20050523-20220508.csv",
)
@click.option(
    "-o", "--out_sqlite_file", required=True, default="../data/nppes_npi/npi_data_ranged.db"
)
def gen_sqlite_ranged(data_file, out_sqlite_file):
    conn = sqlite3.connect(out_sqlite_file)
    c = conn.cursor()

    c.execute(
        "CREATE TABLE basic (npi INTEGER NOT NULL, name TEXT, enum_date DATE, last_update_date DATE, PRIMARY KEY(npi))"
    )
    c.execute(
        "CREATE TABLE location (npi INTEGER NOT NULL, city TEXT, state TEXT, FOREIGN KEY (npi) REFERENCES basic (npi))"
    )
    conn.commit()

    basic_list = []
    loc_list = []
    ignored_lines = 0

    logging.info("Started")

    with open(data_file, "r") as f:
        reader = csv.DictReader(f)
        for index, data in enumerate(reader):
            if index % 1000000 == 0:
                logging.info(f"Progress: {index}")

            name = (
                data["Provider First Name"]
                + " "
                + data["Provider Last Name (Legal Name)"]
            )
            if name.strip() == "":
                ignored_lines += 1
                continue

            enum_date = convert_date(data["Provider Enumeration Date"])
            last_update_date = convert_date(data["Last Update Date"])

            basic_list.append((data["NPI"], name, enum_date, last_update_date))
            loc_list.append(
                (
                    data["NPI"],
                    data["Provider Business Mailing Address City Name"],
                    data["Provider Business Mailing Address State Name"],
                )
            )

    logging.info(f"To be inserted: {len(basic_list)}")
    logging.info(f"Ignored       : {ignored_lines}")

    c.executemany("INSERT INTO basic VALUES (?, ?, ?, ?)", basic_list)
    c.executemany("INSERT INTO location VALUES (?, ?, ?)", loc_list)
    conn.commit()

    conn.close()

@gen_db.command()
@click.option(
    "-d",
    "--data_file",
    required=True,
    default="../data/nppes_npi/npidata_pfile_20050523-20220508.csv",
)
@click.option(
    "-o", "--out_db_file", required=True, default="../data/redis/npi_id_name.db"
)
def gen_redis_id_name(data_file, out_db_file):
    
    ignored_lines = 0
    id_name = []

    logging.info("Started")
    with open(data_file, "r") as f:
        reader = csv.DictReader(f)
        for index, data in enumerate(reader):
            if index % 1000000 == 0:
                logging.info(f"Progress: {index}")

            name = (
                data["Provider First Name"]
                + " "
                + data["Provider Last Name (Legal Name)"]
            )
            if name.strip() == "":
                ignored_lines += 1
                continue
            id_name.append((data["NPI"], name))

    logging.info(f"To be inserted: {len(id_name)}")
    logging.info(f"Ignored       : {ignored_lines}")

    with open(out_db_file, 'w') as f:
        for data in id_name:
            f.write(f"set {data[0]} \"{data[1]}\"\n")
        f.write("save\n")


if __name__ == "__main__":
    gen_db()
