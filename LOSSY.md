# Results using the ZFP compression option in Apache Parquet

The tables below include metrics for the following configurations:
* ZFP (fixed-precision mode, 32 bits)
* ZFP (fixed-precision mode, 28 bits)
* ZFP (fixed-precision mode, 24 bits)
* ZFP (fixed-precision mode, 20 bits)
* ZFP (fixed-precision mode, 16 bits)
* ZFP (fixed-precision mode, 14 bits)

## Compression ratio
The compression ratio is computed as uncompressed size / compressed size.
| Combination \ F32 data            | msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| no compression                    | 1.00   | 1.00   | 1.00   | 1.00        | 1.00      | 1.00      | 1.00        | 1.00       | 1.00      | 1.00     | 1.00        | 1.00     |
| ZFP (32-bit precision)            | 1.06   | 0.98   | 0.99   | 1.07        | 1.03      | 1.08      | 0.99        | 0.94       | 0.97      | 1.00     | 0.97        | 1.00     |
| ZFP (28-bit precision)            | 1.21   | 1.11   | 1.13   | 1.22        | 1.17      | 1.21      | 1.13        | 1.06       | 1.11      | 1.11     | 1.10        | 1.11     |
| ZFP (24-bit precision)            | 1.41   | 1.29   | 1.32   | 1.46        | 1.39      | 1.41      | 1.31        | 1.21       | 1.25      | 1.25     | 1.28        | 1.33     |
| ZFP (20-bit precision)            | 1.68   | 1.52   | 1.58   | 1.76        | 1.66      | 1.68      | 1.57        | 1.42       | 1.50      | 1.67     | 1.51        | 1.54     |
| ZFP (16-bit precision)            | 2.10   | 1.90   | 1.96   | 2.22        | 2.12      | 1.62      | 1.93        | 1.89       | 1.88      | 2.00     | 1.86        | 1.82     |
| ZFP (14-bit precision)            | 2.37   | 2.16   | 2.24   | 2.61        | 2.43      | 2.36      | 2.20        | 2.12       | 2.14      | 2.00     | 2.11        | 2.22     |
| ZFP (12-bit precision)            | 2.78   | 2.45   | 2.62   | 3.00        | 2.83      | 2.74      | 2.48        | 2.43       | 2.50      | 2.50     | 2.44        | 2.50     |

## Compression speed
The speed is measured in MB/s.
| Combination \ F32 data            | msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| ZFP (32-bit precision)            | 61.24  | 57.76  | 58.40  | 53.10       | 53.97     | 58.43     | 53.85       | 62.96      | 58.82     | 66.67    | 55.23       | 58.82    |
| ZFP (28-bit precision)            | 67.72  | 64.14  | 64.95  | 58.25       | 59.65     | 64.20     | 59.23       | 68.00      | 65.22     | 76.92    | 60.13       | 64.52    |
| ZFP (24-bit precision)            | 71.51  | 71.54  | 72.02  | 64.52       | 66.02     | 71.23     | 65.25       | 77.27      | 71.43     | 83.33    | 66.43       | 71.43    |
| ZFP (20-bit precision)            | 85.91  | 78.15  | 82.25  | 72.29       | 73.91     | 78.79     | 73.33       | 89.47      | 81.08     | 100.00   | 74.80       | 83.33    |
| ZFP (16-bit precision)            | 97.71  | 93.00  | 93.92  | 84.51       | 85.00     | 89.66     | 82.80       | 106.25     | 93.75     | 111.11   | 84.82       | 95.24    |
| ZFP (14-bit precision)            | 106.67 | 102.20 | 102.96 | 92.31       | 90.67     | 96.30     | 89.53       | 113.33     | 103.45    | 125.00   | 92.23       | 100.00   |
| ZFP (12-bit precision)            | 115.32 | 112.05 | 112.10 | 105.26      | 98.55     | 104.00    | 97.47       | 121.43     | 111.11    | 142.86   | 100.00      | 111.11   |
