import matplotlib
import matplotlib.pyplot as plt
import numpy as np

f = open("last.txt", "r").read()
lines = f.split("\n")
lines = lines[:len(lines)-1]
compression_ratio = {}
write_throughput = {}
read_throughput = {}
raw_size = {}

def get_name(line):
    first = line.split(" ")[0].split("/")[-1].split(".")
    print(first)
    name = first[0] + "." + first[1]
    return name

for u in lines:
    name = get_name(u)
    compression_ratio[name] = [0 for u in range(6)]
    write_throughput[name] = [0 for u in range(6)]
    read_throughput[name] = [0 for u in range(6)]

for u in lines:
    name = get_name(u)
    if "UNCOMPRESSED.no_enc" in u:
        tokens = u.split(" ")
        compressed_size = int(tokens[3])
        raw_size[name] = compressed_size

for u in lines:
    name = get_name(u)
    tokens = u.split(" ")
    write_time = float(tokens[1])
    read_time = float(tokens[2])
    compressed_size = int(tokens[3])
    real_size = raw_size[name]
    ratio = real_size / compressed_size
    write = (real_size / (1024 * 1024)) / write_time
    read = (real_size / (1024 * 1024)) / read_time
    i = 0
    if "UNCOMPRESSED.no_enc" in u:
        i = 0
    elif "UNCOMPRESSED.dict" in u:
        i = 1
    elif "ZSTD.no_enc" in u:
        i = 2
    elif "ZSTD.dict" in u:
        i = 3
    elif "ZSTD.fp." in u:
        i = 4
    elif "ZSTD.fp_simd." in u:
        i = 5
    else:
        print("Unknown name: {}".format(name))
        assert(False)
    compression_ratio[name][i] = ratio
    write_throughput[name][i] = write
    read_throughput[name][i] = read

def transform(val):
    i = -1
    for u in range(0, len(val)):
        if val[u] == max(val[2:]):
            i = u
            break
    assert(i != -1)
    for u in range(0, len(val)):
        if u == i:
            val[u] = "\\cellcolor{ngreen}" + "{:.2f}".format(val[u])
        else:
            val[u] = "{:.2f}".format(val[u])
    return val


print("Ratio")
for key in sorted(compression_ratio.keys()):
    val = transform(compression_ratio[key])
    line = "\\textbf{" + key + "}" + " & {} & {} & {} & {} & {}\\\\".format(val[0], val[1], val[2], val[3], val[4])
    print(line)

print()
print("Write throughput")
for key in sorted(write_throughput.keys()):
    val = transform(write_throughput[key])
    line = "\\textbf{" + key + "}" + " & {} & {} & {} & {} & {} & {} \\\\".format(val[0], val[1], val[2], val[3], val[4], val[5])
    print(line)

print()
print("Read throughput")
for key in sorted(read_throughput.keys()):
    val = transform(read_throughput[key])
    line = "\\textbf{" + key + "}" + " & {} & {} & {} & {} & {} & {} \\\\".format(val[0], val[1], val[2], val[3], val[4], val[5])
    print(line)
 
