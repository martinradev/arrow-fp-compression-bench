# Results using the ZFP compression option in Apache Parquet

The tables below include metrics for the following configurations:
* ZFP (fixed-precision mode, 32 bits)
* ZFP (fixed-precision mode, 28 bits)
* ZFP (fixed-precision mode, 24 bits)
* ZFP (fixed-precision mode, 20 bits)
* ZFP (fixed-precision mode, 16 bits)
* ZFP (fixed-precision mode, 14 bits)

| Combination \ F32 data            | msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| ZFP (32-bit precision)            | 121    | 95     | 140    | 56          | 66        | 48        | 78          | 18         | 31        | 10       | 98          | 20       |   
| ZFP (28-bit precision)            | 106    | 84     | 123    | 49          | 58        | 43        | 68          | 16         | 27        | 9        | 86          | 18       |   
| ZFP (24-bit precision)            | 91     | 72     | 105    | 41          | 49        | 37        | 59          | 14         | 24        | 8        | 74          | 15       |   
| ZFP (20-bit precision)            | 76     | 61     | 88     | 34          | 41        | 31        | 49          | 12         | 20        | 6        | 63          | 13       |   
| ZFP (16-bit precision)            | 61     | 49     | 71     | 27          | 32        | 32        | 40          | 9          | 16        | 5        | 51          | 11       |   
| ZFP (14-bit precision)            | 54     | 43     | 62     | 23          | 28        | 22        | 35          | 8          | 14        | 5        | 45          | 9        |   
| ZFP (12-bit precision)            | 46     | 38     | 53     | 20          | 24        | 19        | 31          | 7          | 12        | 4        | 39          | 8        |

| Combination \ F32 data            | msg_bt | msg_lu | msg_sp | msg_sweep3d | num_brain | num_comet | num_control | num_plasma | obs_error | obs_info | obs_spitzer | obs_temp |
|-----------------------------------|--------|--------|--------|-------------|-----------|-----------|-------------|------------|-----------|----------|-------------|----------|
| ZFP (32-bit precision)            | 2.09   | 1.61   | 2.38   | 1.13        | 1.26      | 0.89      | 1.43        | 0.27       | 0.51      | 0.15     | 1.72        | 0.34     |
| ZFP (28-bit precision)            | 1.89   | 1.45   | 2.14   | 1.03        | 1.14      | 0.81      | 1.30        | 0.25       | 0.46      | 0.13     | 1.58        | 0.31     |
| ZFP (24-bit precision)            | 1.79   | 1.30   | 1.93   | 0.93        | 1.03      | 0.73      | 1.18        | 0.22       | 0.42      | 0.12     | 1.43        | 0.28     |
| ZFP (20-bit precision)            | 1.49   | 1.19   | 1.69   | 0.83        | 0.92      | 0.66      | 1.05        | 0.19       | 0.37      | 0.10     | 1.27        | 0.24     |
| ZFP (16-bit precision)            | 1.31   | 1.00   | 1.48   | 0.71        | 0.80      | 0.58      | 0.93        | 0.16       | 0.32      | 0.09     | 1.12        | 0.21     |
| ZFP (14-bit precision)            | 1.20   | 0.91   | 1.35   | 0.65        | 0.75      | 0.54      | 0.86        | 0.15       | 0.29      | 0.08     | 1.03        | 0.20     |
| ZFP (12-bit precision)            | 1.11   | 0.83   | 1.24   | 0.57        | 0.69      | 0.50      | 0.79        | 0.14       | 0.27      | 0.07     | 0.95        | 0.18     |
