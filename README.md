# What this is all about
This project includes a benchmark for evaluating the achieved comperession ratio and compression speed for floating-point data using new encodings, new compression algorithms and different configurations for Apache Parquet and Apache Arrow.

# Results
Results from the benchmark are available here:
* Lossless compression and new encodings: [here](/LOSSLESS.md)
* Lossy compression through ZFP: [here](/LOSSY.md)

# Summary
## Lossless compression improvements
The new BYTE_STREAM_SPLIT encoding improves compression ratio and compression speed for certain types of floating-point data where the upper-most bytes of a values do not change much.
The exisiting compressors and encodings in Parquet do not perform well for such data due to noise in the mantissa bytes.
The new encoding improves results by extracting the well compressible bytes into separate byte streams which can be afterwards compressed by a compressor like zstd.

## Lossy compression through ZFP
My project also examines the feasibility of integrating ZFP into the Parquet specification and into Apache Arrow.
The compressor is successfully integrated, requires a simple specification change to list the new compression method and requires few changes to Apache Arrow.

The tests show that the lossy compressor can lead to better compression ratio when enough bits are discarded.
