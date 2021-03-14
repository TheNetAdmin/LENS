import click
import csv


@click.command()
@click.argument("in_file")
@click.argument("out_file")
def parse(in_file, out_file):
    res = []
    with open(in_file, "r") as f:
        lines = f.readlines()
        for line in lines:
            if not line.startswith("ctree_map"):
                continue
            line = line.strip()
            line = line.split(": ")[1]
            line = line.split(", ")
            line = [l.strip() for l in line]
            r = {
                "key": int(line[0].split("=")[1]),
                "ptr_raw": int(line[1].split("=")[1], 16),
                "ptr_ofs": int(line[2], 16),
            }
            r["page_idx"] = r["ptr_ofs"] // 4096
            r["page_ofs"] = r["ptr_ofs"] % 4096
            r["set_idx"] = r["page_idx"] % 256
            res.append(r)
    with open(out_file, "w") as f:
        writer = csv.DictWriter(f, fieldnames=res[0].keys())
        writer.writeheader()
        writer.writerows(res)


if __name__ == "__main__":
    parse()
