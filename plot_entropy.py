import numpy as np
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties

f = open("entropy.log", "r")
results = [[] for u in range(0, 5)]
results2 = [[] for u in range(0, 5)]
results3 = [[] for u in range(0, 5)]
sz = 67108864 
for line in f:
    tokens = line.split(" ")
    name = tokens[0].split("/")[-1].split(".")[0]
    entropy = int(name.split("_")[1])
    tokens[3] = int(tokens[3])
    tokens[2] = float(tokens[2])
    tokens[1] = float(tokens[1])
    if "fp." in line and "ZSTD" in line:
        results[0].append((entropy, sz/ tokens[3]))
        results2[0].append((entropy, int(sz/ (1024*1024) / tokens[1])))
        results3[0].append((entropy, int(sz/ (1024*1024) / tokens[2])))
    elif "dict" in line and "ZSTD" in line:
        results[1].append((entropy, sz/ tokens[3]))
        results2[1].append((entropy, int(sz/ (1024*1024) / tokens[1])))
        results3[1].append((entropy, int(sz/ (1024*1024) / tokens[2])))
    elif "no_enc" in line and "ZSTD" in line:
        results[2].append((entropy, sz/ tokens[3]))
        results2[2].append((entropy, int(sz/ (1024 * 1024) / tokens[1])))
        results3[2].append((entropy, int(sz/ (1024*1024) / tokens[2])))
    elif "dict" in line and "UNCOMPRESSED" in line:
        results[3].append((entropy, sz/ tokens[3]))
        results2[3].append((entropy, int(sz/ (1024 * 1024) / tokens[1])))
        results3[3].append((entropy, int(sz/ (1024*1024) / tokens[2])))
    elif "fp_xor" in line:
        results[4].append((entropy, sz/ tokens[3]))
        results2[4].append((entropy, int(sz/ (1024*1024) / tokens[1])))
        results3[4].append((entropy, int(sz/ (1024*1024) / tokens[2])))
    else:
        print(line)
        continue
        assert(False)

x = [[] for u in range(0, 5)]
y = [[] for u in range(0, 5)]
for i in range(0, 5):
    results[i] = sorted(results[i], key=lambda u: u[0])
    x[i] = [u[0] for u in results[i]]
    y[i] = [u[1] for u in results[i]]

fontP = FontProperties()
fontP.set_size('x-small')

# Compression ratio
fig = plt.figure()
plt.xlabel("Entropy (bits/element)")
plt.ylabel("Compression ratio")
labels = ["BYTE_STREAM_SPLIT + ZSTD", "DICTIONARY + ZSTD", "PLAIN + ZSTD", "DICTIONARY (Uncompressed)"]
markers=['o', '.', '+', 'x', '^']
for u in range(0, 4):
    plt.plot(x[u], y[u], label=labels[u], marker=markers[u])

plt.legend(prop=fontP)
plt.savefig("entropy_ratio.jpeg")
plt.close(fig)

# Compression speed
x = [[] for u in range(0, 5)]
y = [[] for u in range(0, 5)]
for i in range(0, 4):
    results2[i] = sorted(results2[i], key=lambda u: u[0])
    x[i] = [u[0] for u in results2[i]]
    y[i] = [u[1] for u in results2[i]]

fig = plt.figure()
plt.xlabel("Entropy (bits/element)")
plt.ylabel("Parquet write throughput (MiB/s)")
plt.ylim(.0, 600.0)
markers=['o', '.', '+', 'x', '^']
for u in range(0, 4):
    plt.plot(x[u], y[u], label=labels[u], marker=markers[u])

plt.legend(prop=fontP)
plt.savefig("entropy_write_speed.jpeg")
plt.close(fig)

# Compression speed
x = [[] for u in range(0, 5)]
y = [[] for u in range(0, 5)]
for i in range(0, 4):
    results3[i] = sorted(results3[i], key=lambda u: u[0])
    x[i] = [u[0] for u in results3[i]]
    y[i] = [u[1] for u in results3[i]]

plt.xlabel("Entropy (bits/element)")
plt.ylabel("Parquet read throughput(MiB/s)")
plt.ylim(.0, 1000.0)
markers=['o', '.', '+', 'x', '^']
for u in range(0, 4):
    plt.plot(x[u], y[u], label=labels[u], marker=markers[u])

plt.legend(prop=fontP)
plt.savefig("entropy_read_speed.jpeg")


