# New lossless encodings for Apache Parquet

The tables below include metrics for the following configurations:
* PLAIN encoding, UNCOMPRESSED
* PLAIN encoding, GZIP
* PLAIN encoding, ZSTD
* DICTIONARY encoding, GZIP
* DICTIONARY encoding, ZSTD
* BYTE_STREAM_SPLIT encoding, GZIP
* BYTE_STREAM_SPLIT encoding, ZSTD
* ADAPTIVE_BYTE_STREAM_SPLIT encoding, GZIP
* ADAPTIVE_BYTE_STREAM_SPLIT encoding, ZSTD

The *BYTE_STREAM_SPLIT* and *ADAPTIVE_BYTE_STREAM_SPLIT* encodings are new, designed by me and not yet part of the Apache Parquet project.
For the not-production-level C++ implementation of both, you can check my fork of Apache Arrow: https://github.com/martinradev/arrow.

The *BYTE_STREAM_SPLIT* encoding takes values in an array and scatters each byte of a value to different streams.
There are four streams for 32-bit floating-point values and eight streams for 64-bit floating-point values.
After the bytes of all values are scatter, the streams are concatenated.
Note that this encoding does not reduce the size of the array but makes it potentially more compressible. Thus, it is important to use a compression algorithm.

The *ADAPTIVE_BYTE_STREAM_SPLIT* encoding attempts to improve results of *BYTE_STREAM_SPLIT* encoding for cases when data in the array is very repetitive and a combination of PLAIN encoding and a compression algorithm achieve better results.
This encoding divides the array into blocks of K values. For each block it uses are heuristic to determine whether using the BYTE_STREAM_SPLIT encoding for the smaller block would achieve better results than using PLAIN encoding for it. For each block it has to store an additional *type bit* to be able to determine what encoding was used when decoding. This adds an overhead in storage because at least N/K bits are necessary for storing the types where N is the number of values in the array and K is the block size. Also, the heuristic can be complex enough that encoding is slow.
A possible heuristic can be seen in my Arrow fork patches. It is based on this idea: http://romania.amazon.com/techon/presentations/DataStreamsAlgorithms_FlorinManolache.pdf

# ARROW's default compression level
## Compression ratio
Compression ratio is computed as uncompressed size / compressed size. Higher is better.
### Data with only one column. Data has high 0-order-entropy when the distribution is over the values (4 bytes).

| Combination \ F32 data            | 631-tst|sbd_05 | sbd_10 | sbd_15 | msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|-------|--------|--------|--------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| no compression                    | 1.00   |1.00   | 1.00   | 1.00   | 1.00   | 1.00   | 1.00   | 1.00        | 1.00      | 1.00      | 1.00        | 1.00       | 1.00      | 1.00     | 1.00        | 1.00     |
| gzip                              | 1.06   |1.23   | 1.58   | 1.57   | 1.19   | 1.08   | 1.20   | 1.13        | 1.13      | 1.16      | 1.08        | 1.31       | 1.43      | 1.25     | 1.19        | 1.11     |
| dictionary + gzip                 | 1.06   |1.23   | 1.58   | 1.57   | 1.17   | 1.08   | 1.20   | 1.13        | 1.11      | 1.13      | 1.08        | **17.00**      | 1.43      | 1.25     | 1.19        | 1.05     |
| byte_stream_split + gzip          | **1.12**   |**1.47**   | 1.90   | **2.42**   | **1.52**   | 1.33   | **1.62**   | 2.73        | **1.39**      | **1.37**      | **1.22**        | 8.50       | **1.50**      | **1.67**     | 1.30        | 1.25     |
| adaptive byte_stream_split + gzip | missing|missing| missing| missing| 1.51   | 1.31   | 1.58   | 1.46        | 1.31      | 1.33      | 1.17        | 1.31       | 1.36      | 1.43     | 1.19        | 1.18     |
| zstd                              | 1.04   |1.13   | 1.17   | 1.18   | 1.14   | 1.07   | 1.11   | 3.16        | 1.13      | 1.16      | 1.08        | 8.50       | 1.25      | 1.25     | 1.14        | 1.11     |
| dictionary + zstd                 | 1.04   |1.13   | 1.17   | 1.18   | 1.13   | 1.06   | 1.10   | 3.00        | 1.11      | 1.13      | 1.07        | **17.00**      | 1.25      | 1.25     | 1.14        | 1.05     |
| byte_stream_split + zstd          | 1.11   |1.41   | **2.03**   | 2.33   | **1.52**   | **1.39**   | 1.58   | **4.62**        | **1.39**      | **1.37**      | **1.22**        | **17.00**      | 1.43      | 1.43     | **1.32**        | **1.25**     |
| adaptive byte_stream_split + zstd | missing|missing| missing| missing| 1.45   | 1.33   | 1.51   | **4.62**        | 1.33      | 1.33      | 1.13        | **17.00**      | **1.50**      | 1.43     | 1.17        | 1.18     |

![](plot_data/Figure_1.png)

### Multiple columns but data is extremely repetitive.
| Combination \ mixed data          | Can_01_SPEC | GT61  | GT62  |
|-----------------------------------|-------------|-------|-------|
| no compression                    | 1.00        | 1.00  | 1.00  |
| gzip                              | 2.14        | **20.99** | **20.94** |
| dictionary + gzip                 | **2.92**    | 19.63 | 19.59 |
| byte_stream_split + gzip          | 2.11        | 13.12 | 13.06 |
| adaptive byte_stream_split + gzip | 1.85        | 18.57 | 18.83 |
| zstd                              | 1.90        | 15.78 | 15.57 |
| dictionary + zstd                 | 2.91        | 15.47 | 15.77 |
| byte_stream_split + zstd          | 2.09        | 10.87 | 10.99 |
| adaptive byte_stream_split + zstd | 1.66        | 13.95 | 14.04 |

![](plot_data/Figure_2.png)

We can see that the *byte_stream_split encoding* improves the compression ratio for the majority of test cases.

The *adaptive byte_stream_split encoding* is only useful for whenever the data is very repetitive as is the case for the GT61 and GT62 tests. For all other cases, the encoding is not competitive due to the overhead from storing block types, encoding at the block level and less than ideal heuristic.


The *dictionary encoding* only produces good results for the Can_01_SPEC, GT61, GT62 and num_plasma which are tests with very repetitive data. For all other is typically worse than the *plain encoding*.

For a more detailed view of the column data in GT61 and GT62, go [here](SEPARATE_COLUMNS.md).

## Compression speed
Measurements are in *MBs/second*. Higher is better.

The best results in each column are highlighthed by bolding them.

| Combination \ F32 data            | 631-tst|sbd_05 | sbs_10 | sbd_15 |msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|-------|--------|--------|-------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| gzip                              | 26.30  |14.56  | 13.54  | 13.53  | 23.57  | 24.22  | 16.99  | 23.53       | 21.52     | 28.42     | 25.33       | 29.82      | 21.13     | 28.57    | 14.89       | 25.00    |
| dictionary + gzip                 | 26.02  |14.56  | 13.98  | 13.71  | 23.06  | 22.57  | 17.94  | 23.44       | 21.25     | 27.96     | 25.00       | 113.33     | 20.69     | 26.32    | 14.96       | 23.81    |
| byte_stream_split + gzip          | 26.84  |35.31  | 44.24  | 49.18  | 31.45  | 34.19  | 33.74  | 43.17       | 33.01     | 28.73     | 19.20       | 68.00      | 27.78     | 37.04    | 13.14       | 20.00    |
| adaptive byte_stream_split + gzip | missing|missing| missing| missing| 25.30  | 25.48  | 26.53  | 27.03       | 24.82     | 24.88     | 18.03       | 27.42      | 22.90     | 30.30    | 14.37       | 18.18    |
| zstd                              | 243.56 |291.01 | 293.05 | 284.56 |**191.04** | 244.74 | **257.41** | **428.57**      | **272.00**    | **247.62**    | **265.52**      | **850.00**     | 176.47    | **333.33**   | **193.88**      | **285.71**   |
| dictionary + zstd                 | 255.90 |277.17 | 290.26 | 279.47 |**191.04** | 232.50 | 252.73 | 375.00      | 251.85    | 226.09    | 248.39      | 242.86     | 157.89    | 200.00   | 182.69      | 222.22   |
| byte_stream_split + zstd          | **278.37** |**305.48** | **350.27** | **327.50?** |182.86 | **258.33** | 231.67 | 333.33      | 234.48    | 192.59    | 220.00      | 425.00     | **214.29**    | 250.00   | 161.02      | 222.22   |
| adaptive byte_stream_split + zstd | missing|missing| missing| missing|74.42  | 75.61  | 73.54  | 90.91       | 77.27     | 76.47     | 70.64       | 94.44      | 75.00     | 90.91    | 72.52       | 71.43    |

![](plot_data/Figure_3.png)

| Combination \ mixed data          | Can_01_SPEC | GT61   | GT62   |
|-----------------------------------|-------------|--------|--------|
| gzip                              | 8.81        | 61.71  | 60.33  |
| dictionary + gzip                 | 54.74       | 94.63  | 86.04  |
| byte_stream_split + gzip          | 8.66        | 39.32  | 38.62  |
| adaptive byte_stream_split + gzip | 9.69        | 50.97  | 51.71  |
| zstd                              | 114.15      | **377.19** | **352.54** |
| dictionary + zstd                 | **182.93**      | 260.97 | 239.78 |
| byte_stream_split + zstd          | 59.48       | 241.64 | 251.97 |
| adaptive byte_stream_split + zstd | 30.84       | 189.63 | 190.21 |

![](plot_data/Figure_4.png)

Using the *adaptive byte_stream_split encoding* typically leads to slower creation of parquet files. The is expected because the type heuristic has to read the whole block and approximate the most occurring elements. The improvement in compression ratio over the dictionary encoding and plain encoding is not high enough for the majority of tests so that the reduction in IO-usage can lead to a better performance. The adaptive encoding still produces better results for when GZIP is used. A reason could be that the transformed input is faster to parse in GZIP than for the plain or dictionary-produced input.

Using *byte_stream_encoding* with GZIP also leads to faster creation of parquet files. When ZSTD is used, there seems to be almost no performance difference between this encoding, plain and dictionary encoding. A reason could be that the time for apply the encoding and time for writing out the file to disk somehow even out.

*Dictionary encoding* achieves significantly better performance for highly repetitive data like Can_01_Spec, GT61 and GT62. The reason is that the compressed file size is smaller and less time is spent on IO.

## Combined scatter plot

![](plot_data/Figure_5.png)

This relative scatter plot includes all of the collected results for compression ratio and speed. The different symbol markers correspond to the encoding-compression algorithm combination and the color corresponds to a sample in the test data. For each color, markers located to the left, bottom or left-bottom are of interest.

Very high compression ratio is achieved for the GT61 and GT62 samples regardless of the encoding and compression algorithm. Plain encoding and zstd seems to be a good candidate because it achieves great compression ratio and is faster than any other combination.

For Can_01_SPEC the best combination is *dictionary + zstd*.

For almost any other case, the *BYTE_STREAM_SPLIT + zstd* combination tends to perform better in both compression ratio and speed.

# Different compression levels for ZSTD
## Compression ratio
The table shows the achieved compression ratio with different compression level for ZSTD and used encoding.
The best results in each row are highlighthed by bolding them.

| Data         | Level | zstd      | dictionary + zstd      | byte_stream_splt + zstd      | adaptive byte_stream_split + zstd      |
|--------------|-------|-----------|------------------------|------------------------------|----------------------------------------|
| 631-tst      | 1     | 1.04      | 1.04                   | **1.11**                         | missing                                |
|              | 4     | 1.05      | 1.05                   | **1.11**                         | missing                                |
|              | 7     | 1.06      | 1.06                   | **1.11**                         | missing                                |
|              | 10    | 1.06      | 1.06                   | **1.12**                         | missing                                |
| sbd_05       | 1     | 1.13      | 1.13                   | **1.41**                         | missing                                |
|              | 4     | 1.13      | 1.13                   | **1.46**                         | missing                                |
|              | 7     | 1.13      | 1.13                   | **1.46**                         | missing                                |
|              | 10    | 1.13      | 1.13                   | **1.46**                         | missing                                |
| sbd_10       | 1     | 1.17      | 1.17                   | **1.88**                         | missing                                |
|              | 4     | 1.33      | 1.33                   | **1.89**                         | missing                                |
|              | 7     | 1.41      | 1.41                   | **1.90**                         | missing                                |
|              | 10    | 1.41      | 1.41                   | **1.90**                         | missing                                |
| sbd_15       | 1     | 1.18      | 1.18                   | **2.33**                         | missing                                |
|              | 4     | 1.37      | 1.36                   | **2.38**                         | missing                                |
|              | 7     | 1.40      | 1.40                   | **2.44**                         | missing                                |
|              | 10    | 1.40      | 1.39                   | **2.44**                         | missing                                |
| msg_bt       |       |           |                        |                              |                                        |
|              | 1     | 1.14      | 1.13                   | **1.52**                     | 1.44                                   |
|              | 4     | 1.16      | 1.15                   | **1.60**                     | 1.52                                   |
|              | 7     | 1.16      | 1.15                   | **1.60**                     | 1.54                                   |
|              | 10    | 1.16      | 1.15                   | **1.60**                     | 1.56                                   |
| msg_lu       |       |           |                        |                              |                                        |
|              | 1     | 1.07      | 1.06                   | **1.39**                     | 1.33                                   |
|              | 4     | 1.07      | 1.06                   | **1.43**                     | 1.39                                   |
|              | 7     | 1.07      | 1.07                   | **1.45**                     | 1.43                                   |
|              | 10    | 1.07      | 1.07                   | **1.45**                     | 1.43                                   |
| msg_sp       |       |           |                        |                              |                                        |
|              | 1     | 1.11      | 1.10                   | **1.58**                     | 1.51                                   |
|              | 4     | 1.12      | 1.11                   | **1.62**                     | 1.58                                   |
|              | 7     | 1.13      | 1.13                   | **1.64**                     | 1.60                                   |
|              | 10    | 1.13      | 1.13                   | **1.64**                     | 1.60                                   |
| msg_sweep3d  |       |           |                        |                              |                                        |
|              | 1     | 3.16      | 3.00                   | **4.62**                     | 4.62                                   |
|              | 4     | 3.33      | 3.00                   | **5.00**                     | 4.62                                   |
|              | 7     | 3.33      | 3.16                   | **5.00**                     | **5.00**                               |
|              | 10    | 3.33      | 3.33                   | **5.00**                     | **5.00**                               |
| num_brain    |       |           |                        |                              |                                        |
|              | 1     | 1.13      | 1.11                   | **1.39**                     | 1.33                                   |
|              | 4     | 1.13      | 1.11                   | **1.42**                     | 1.39                                   |
|              | 7     | 1.13      | 1.11                   | **1.42**                     | 1.39                                   |
|              | 10    | 1.13      | 1.11                   | **1.42**                     | **1.42**                               |
| num_comet    |       |           |                        |                              |                                        |
|              | 1     | 1.16      | 1.13                   | **1.37**                     | 1.33                                   |
|              | 4     | 1.16      | 1.13                   | **1.37**                     | 1.33                                   |
|              | 7     | 1.16      | 1.13                   | **1.37**                     | 1.33                                   |
|              | 10    | 1.16      | 1.13                   | **1.37**                     | 1.33                                   |
| num_control  |       |           |                        |                              |                                        |
|              | 1     | 1.08      | 1.07                   | **1.22**                     | 1.13                                   |
|              | 4     | 1.08      | 1.07                   | **1.20**                     | 1.18                                   |
|              | 7     | 1.08      | 1.07                   | **1.22**                     | 1.18                                   |
|              | 10    | 1.08      | 1.07                   | **1.22**                     | 1.18                                   |
| obs_error    |       |           |                        |                              |                                        |
|              | 1     | 1.25      | 1.25                   | **1.43**                     | **1.43**                               |
|              | 4     | **1.50**  | 1.43                   | 1.43                         | **1.50**                               |
|              | 7     | 1.50      | 1.43                   | **1.58**                     | 1.50                                   |
|              | 10    | 1.50      | 1.50                   | **1.58**                     | 1.50                                   |
| obs_info     |       |           |                        |                              |                                        |
|              | 1     | 1.25      | 1.25                   | **1.43**                     | **1.43**                               |
|              | 4     | 1.43      | 1.43                   | **1.67**                     | **1.67**                               |
|              | 7     | 1.67      | 1.43                   | **2.00**                     | **2.00**                               |
|              | 10    | 1.67      | 1.43                   | **2.00**                     | **2.00**                               |
| obs_spitzer  |       |           |                        |                              |                                        |
|              | 1     | 1.14      | 1.14                   | **1.32**                     | 1.17                                   |
|              | 4     | 1.23      | 1.23                   | **1.30**                     | 1.22                                   |
|              | 7     | 1.27      | 1.27                   | **1.32**                     | 1.23                                   |
|              | 10    | 1.27      | 1.27                   | **1.32**                     | 1.23                                   |
| obs_temp     |       |           |                        |                              |                                        |
|              | 1     | 1.11      | 1.05                   | **1.25**                     | 1.18                                   |
|              | 4     | 1.11      | 1.05                   | **1.25**                     | 1.18                                   |
|              | 7     | 1.11      | 1.05                   | **1.25**                     | 1.18                                   |
|              | 10    | 1.11      | 1.05                   | **1.25**                     | 1.18                                   |
| GT61         |       |           |                        |                              |                                        |
|              | 1     | **15.78** | 15.47                  | 10.87                        | 14.90                                  |
|              | 4     | 17.00     | **18.86**              | 11.89                        | 16.31                                  |
|              | 7     | 19.95     | **20.12**              | 13.26                        | 18.86                                  |
|              | 10    | 20.12     | **20.29**              | 13.41                        | 19.16                                  |
| GT62         |       |           |                        |                              |                                        |
|              | 1     | 15.57     | **15.77**              | 10.99                        | 14.90                                  |
|              | 4     | 16.75     | **18.40**              | 11.85                        | 16.19                                  |
|              | 7     | 19.75     | **19.91**              | 13.20                        | 18.98                                  |
|              | 10    | 19.91     | **20.07**              | 13.42                        | 19.13                                  |

Note that there is a small discrepancy between ZSTD with level 1 and the tables from before for the ADAPTIVE_STREAM_SPLIT_ENCODING. The small difference in compression ratio is due to tweaks in the heuristic midway generating the data for the tables. Because it takes some time to re-generate the tables, I decided to leave it as it is because the difference is very small.
