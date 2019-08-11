### Compression ratio
#### Data with only one column. Data has high 0-order-entropy when the distribution is over the values (4 bytes).
| F32 dataset name | no compression (MB) | gzip (MB) | dictionary + gzip (MB) | byte_stream_split + gzip (MB) | zstd (MB) | dictionary + zstd (MB) | byte_stream_split + zstd (MB) |
|------------------|---------------------|-----------|------------------------|-------------------------------|-----------|------------------------|-------------------------------|
| msg_bt           | 128                 | 108       | 109                    | 85                            | 112       | 113                    | 88                            |
| msg_lu           | 93                  | 86        | 86                     | 71                            | 87        | 88                     | 70                            |
| msg_sp           | 139                 | 116       | 116                    | 88                            | 125       | 126                    | 92                            |
| msg_sweep3d      | 60                  | 53        | 53                     | 41                            | 19        | 20                     | 13                            |
| num_brain        | 68                  | 60        | 61                     | 52                            | 60        | 61                     | 51                            |
| num_comet        | 52                  | 45        | 46                     | 39                            | 45        | 46                     | 39                            |
| num_control      | 77                  | 71        | 71                     | 66                            | 71        | 72                     | 68                            |
| num_plasma       | 17                  | 13        | 1                      | 13                            | 2         | 1                      | 1                             |
| obs_error        | 30                  | 21        | 21                     | 22                            | 24        | 24                     | 20                            |
| obs_info         | 10                  | 8         | 8                      | 7                             | 8         | 8                      | 7                             |
| obs_spitzer      | 95                  | 80        | 80                     | 80                            | 83        | 83                     | 81                            |
| obs_temp         | 20                  | 18        | 19                     | 17                            | 18        | 19                     | 17                            |

#### Multiple columns but data is extremely repetitive.
| Mixed dataset name | no compression (MB) | gzip (MB) | dictionary + gzip (MB) | byte_stream_split + gzip (MB) | zstd (MB) | dictionary + zstd (MB) | byte_stream_split + zstd (MB) |
|--------------------|---------------------|-----------|------------------------|-------------------------------|-----------|------------------------|-------------------------------|
| Can_01_SPEC        | 3300                | 1545      | 1131                   | 1788                          | 1741      | 1135                   | 1905                          |
| GT61               | 2414                | 115       | 123                    | 130                           | 153       | 156                    | 173                           |
| GT62               | 2429                | 116       | 124                    | 129                           | 156       | 154                    | 173                           |


### Compression speed
| F32 dataset name | gzip (s) | dictionary + gzip (s) | byte_stream_split + gzip (s) | zstd (s) | dictionary + zstd (s) | byte_stream_split + zstd (s) |
|------------------|----------|-----------------------|------------------------------|----------|-----------------------|------------------------------|
| msg_bt           | 5.43     | 5.55                  | 5.06                         | 0.67     | 0.67                  | 1.72                         |
| msg_lu           | 3.84     | 4.12                  | 3.65                         | 0.38     | 0.40                  | 1.23                         |
| msg_sp           | 8.18     | 7.75                  | 5.24                         | 0.54     | 0.55                  | 1.89                         |
| msg_sweep3d      | 2.55     | 2.56                  | 2.22                         | 0.14     | 0.16                  | 0.66                         |
| num_brain        | 3.16     | 3.20                  | 2.74                         | 0.25     | 0.27                  | 0.88                         |
| num_comet        | 1.83     | 1.86                  | 2.09                         | 0.21     | 0.23                  | 0.68                         |
| num_control      | 3.04     | 3.08                  | 4.27                         | 0.29     | 0.31                  | 1.09                         |
| num_plasma       | 0.57     | 0.15                  | 0.62                         | 0.02     | 0.07                  | 0.18                         |
| obs_error        | 1.42     | 1.45                  | 1.31                         | 0.17     | 0.19                  | 0.40                         |
| obs_info         | 0.35     | 0.38                  | 0.33                         | 0.03     | 0.05                  | 0.11                         |
| obs_spitzer      | 6.38     | 6.35                  | 6.61                         | 0.49     | 0.52                  | 1.31                         |
| obs_temp         | 0.80     | 0.84                  | 1.10                         | 0.07     | 0.09                  | 0.28                         |

| Mixed dataset name | gzip (s) | dictionary + gzip (s) | byte_stream_split + gzip (s) | zstd (s) | dictionary + zstd (s) | byte_stream_split + zstd (s) |
|--------------------|----------|-----------------------|------------------------------|----------|-----------------------|------------------------------|
| Can_01_SPEC        | 374.48   | 60.29                 | 340.57                       | 28.91    | 18.04                 | 107.00                       |
| GT61               | 39.12    | 25.51                 | 47.36                        | 6.40     | 9.25                  | 12.73                        |
| GT62               | 40.26    | 28.23                 | 46.97                        | 6.89     | 10.13                 | 12.77                        |



