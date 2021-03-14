import csv
import pandas as pd
import re


def parse_line(line: str):
    line = line.replace(" ", "")
    pattern = r"(?:iter\[\s*)(\d+)(?:\][\s\=]*\[)([\s\w\d\,\=]+)(?:\])"
    r = re.findall(pattern, line)[0]
    iter = int(r[0])
    res = []
    for s in r[1].split(","):
        idx, lat = s.split("=")
        idx = int(idx[1:])
        lat = int(lat)
        res.append({"iter": iter, "set": idx, "lat": lat})
    return res


def parse():
    with open("side_channel.log", "r") as f:
        lines = f.readlines()
        lines = [line.rstrip() for line in lines]
    res = []
    for line in lines:
        if line.startswith("per_cacheline_lat"):
            res += parse_line(line)
    with open("side_channel.csv", "w") as f:
        writer = csv.DictWriter(f, fieldnames=res[0].keys())
        writer.writeheader()
        writer.writerows(res)
    df = pd.DataFrame(res)
    meds = [0 for _ in range(256)]
    for s in range(256):
        meds[s] = df.loc[df["set"] == s]['lat'].median()
    summary = []

    # slt: suggested_lat_threshold
    slt=[float('inf'), -float('inf')]
    sltm=[float('inf'), -float('inf')]
    for d in res:
        m = meds[d["set"]]
        d["set_median"] = m
        d["over_median"] = 1 if d["lat"] > m else 0
        d["frac_median"] = d["lat"] / m
        summary.append(d)
        if d['iter'] == d['set'] and d['lat'] < 1200:
            slt[0] = min(slt[0], d['lat'])
            slt[1] = max(slt[1], d['lat'])
            sltm[0] = min(sltm[0], d['frac_median'])
            sltm[1] = max(sltm[1], d['frac_median'])
    with open("summary.csv", "w") as f:
        writer = csv.DictWriter(f, fieldnames=summary[0].keys())
        writer.writeheader()
        writer.writerows(summary)
    print(f"Suggested latency threshold: {slt}")
    print(f"Suggested latency threshold frac median: {sltm}")


if __name__ == "__main__":
    parse()
