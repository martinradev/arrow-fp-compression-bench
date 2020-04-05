import sys
from pytablewriter import MarkdownTableWriter

entries = []
f = open("last_simd.txt", "r")
for line in f:
    tokens = line[:-1].split(" ") 
    sz = int(tokens[3])
    t_dec = float(tokens[2])
    t = float(tokens[1])
    name = tokens[0].split("/")
    name = name[len(name)-1]
    tokens = name.split(".")
    name = tokens[0] + "." + tokens[1]
    l = len(tokens)
    precision = int(tokens[l-1][3:-1])
    level = int(tokens[l-2])
    enc = tokens[l-3]
    comp = tokens[l-4].lower()
    if enc == "no_enc":
        enc = "plain"
    elif enc == "dict":
        enc = "dictionary"
    elif enc == "fp":
        enc = "split"
    elif enc == "fp_xor":
        enc = "split+xor"
    elif enc == "fp_comp":
        enc = "split component"
    elif enc == "fp_rle":
        enc = "split+rle"
    else:
        print(enc)
        assert(False)
    entries.append((name, enc, comp, level, precision, sz, t))

uncompressed_size = {}

h = ["Configuration \\ Test"]
table1 = []
table2 = []
row = ["no compression"]
row2 = ["no compression"]
for u in entries:
    if u[1] == "plain" and u[2] == "uncompressed":
        print(u)
        h.append(u[0])
        row.append("1.00")
        uncompressed_size[u[0]] = u[5]
        row2.append((u[5] / (1024 * 1024)) / u[6])
table1.append(row)
table2.append(row2)

comb = [("gzip", "plain", "gzip"), ("dictionary + gzip", "dictionary", "gzip"),
        ("byte_stream_split + gzip", "split", "gzip"), ("byte_stream_split_xor + gzip", "split+xor", "gzip"),
        ("byte_stream_split_component + gzip", "split component", "gzip"),
        ("byte_stream_split_rle + gzip", "split+rle", "gzip"),

        ("zstd", "plain", "zstd"), ("dictionary + zstd", "dictionary", "zstd"),
        ("byte_stream_split + zstd", "split", "zstd"), ("byte_stream_split_xor + zstd", "split+xor", "zstd"),
        ("byte_stream_split_component + zstd", "split component", "zstd"),
        ("byte_stream_split_rle + zstd", "split+rle", "zstd")]

comb = [("byte_stream_split + zstd", "split", "zstd")]

best = {}
best2 = {}
for u in uncompressed_size:
    best[u] = -1.0
    best2[u] = -1.0

for c in comb:
    i = 0
    for u in entries:
        if u[1] == c[1] and u[2] == c[2] and u[3] == -1:
            assert(h[1+i] == u[0]) # check that order is preserved
            ratio = uncompressed_size[u[0]] / u[5]
            if ratio > best[u[0]]:
                best[u[0]] = ratio
            speed = (u[5] / (1024 * 1024)) / u[6]
            if speed > best2[u[0]]:
                best2[u[0]] = speed
            i+=1
 
for c in comb:
    row = [c[0]]
    row2 = [c[0]]
    i = 0
    for u in entries:
        if u[1] == c[1] and u[2] == c[2] and u[3] == -1:
            assert(h[1+i] == u[0]) # check that order is preserved
            ratio = uncompressed_size[u[0]] / u[5]
            s = "{0:.2f}".format(ratio)
            if ratio == best[u[0]]:
                s = "***" + s + "***"
            row.append(s)
            speed = (u[5] / (1024 * 1024)) / u[6]
            s = "{0:.2f}".format(speed)
            if speed == best2[u[0]]:
                s = "***" + s + "***"
            row2.append(s)
            i += 1
    table1.append(row)
    table2.append(row2)

writer = MarkdownTableWriter()
writer.table_name = "Compression ratio"
writer.headers = h
writer.value_matrix = table1
writer.write_table()
print()

writer = MarkdownTableWriter()
writer.table_name = "Compression speed"
writer.headers = h
writer.value_matrix = table2
writer.write_table()

"""
h = ["Test", "Level", "zstd", "dictionary + zstd", "byte_stream_split + zstd"]
mp = {}
for u in uncompressed_size:
    for v in ["plain", "dictionary", "split":
        mp[(u,v)] = []

for u in entries:
    for i in range(1, 13, 3):
        if u[1] in ["split", "dictionary", "plain"] and u[2] == "zstd" and u[3] == i:
            mp[(u[0], u[1])].append((i, uncompressed_size[u[0]] / u[5]))
            
table = []
for u in mp:
   res = mp[u] 
   i = 0
   for v in res:
        if i == 0:
            table.append([u, v[0], v[1]])
        else:
            table.append(["", v[0], v[1]])
        i += 1

print()
writer = MarkdownTableWriter()
writer.table_name = "Compression levels"
writer.headers = h
writer.value_matrix = table
writer.write_table()
"""
