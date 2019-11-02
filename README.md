# What this is all about
This project includes a benchmark for evaluating the achieved comperession ratio and compression speed for floating-point data using new encodings, new compression algorithms and different configurations for Apache Parquet and Apache Arrow.

# Results
Results from the benchmark are available here:
* Lossless compression and new encodings: [here](/LOSSLESS.md)
* Lossy compression through ZFP: [here](/LOSSY.md)

# Data set

The data set includes multiple tests with high and low entropy.
The entropy of each data set is available below:

|       name     | entropy | size (MB) |
|----------------|---------|-----------|
| msg_bt.sp      |  22.754 |       134 |
| msg_lu.sp      |  24.293 |        98 |
| msg_sp.sp      |  23.282 |       146 |
| msg_sweep3d.sp |  19.691 |        63 |
| num_brain.sp   |  23.580 |        71 |
| num_comet.sp   |  21.880 |        54 |
| num_control.sp |  24.082 |        80 |
| num_plasma.sp  |  13.651 |        18 |
| obs_error.sp   |  17.785 |        32 |
| obs_info.sp    |  18.068 |        10 |
| obs_spitzer.sp |  17.359 |       100 |
| obs_temp.sp    |  22.185 |        20 |
| GT61 (avg)     |   5.173 |      2414 |
| GT62 (avg)     |   5.195 |      2429 |
| sbd_05.dp      |  25.000 |       269 |
| sbd_10.dp      |  25.000 |       269 |
| sbd_15.dp      |  25.000 |       269 |
| 631-tst.dp     |  26.553 |       824 |

The msg\*.sp, num\*.sp, obs\*.sp tests come from a [publicly available data set](https://userweb.cs.txstate.edu/~burtscher/research/datasets/FPsingle/).

The sbd_05.dp, sbd_10.dp, sbd_15.dp tests are taken from [here (courtesy of Brown University and SDR Bench](https://sdrbench.github.io/).
The tests are synthetic 64-bit floating-point data.

The 641-tst.dp test is taken from [here (courtesy of Kristopher William Keipert)](https://sdrbench.github.io/).

The GT61 and GT62 tests are from an internal source.
Because both tests are Parquet files with multiple columns, we report the average entropy over all columns.

# Summary
## Lossless compression improvements
The new BYTE_STREAM_SPLIT encoding improves compression ratio and compression speed for certain types of floating-point data where the upper-most bytes of a values do not change much.
The exisiting compressors and encodings in Parquet do not perform well for such data due to noise in the mantissa bytes.
The new encoding improves results by extracting the well compressible bytes into separate byte streams which can be afterwards compressed by a compressor like zstd.

## Lossy compression through ZFP
My project also examines the feasibility of integrating ZFP into the Parquet specification and into Apache Arrow.
The compressor is successfully integrated, requires a simple specification change to list the new compression method and requires few changes to Apache Arrow.

The tests show that the lossy compressor can lead to better compression ratio when enough bits are discarded.
