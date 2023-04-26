import csv

def parse_line(line: str):
    line = line.replace(" ", "")
    line = line.split(",")
    res = {}
    for l in line:
        k, v = l.split("=")
        res[k] = int(v)
    return res


def parse_shared_lib():
    with open("side_channel.log", "r") as f:
        lines = f.readlines()
        lines = [line.rstrip() for line in lines]
    res = []
    for line in lines:
        if line.startswith("iter="):
            res.append(parse_line(line))
    assert(len(res) > 0)
    with open("summary.csv", 'w') as f:
        writer = csv.DictWriter(f, fieldnames=res[0].keys())
        writer.writeheader()
        writer.writerows(res)

if __name__ == "__main__":
    parse_shared_lib()
