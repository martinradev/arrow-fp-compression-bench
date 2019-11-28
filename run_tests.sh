#!/bin/sh

# generate comparison at default compression level
fp_data="$(ls ../dataset/new/*.dp) $(ls ../dataset/32bit/*.sp)"
cases="-c uncompressed,plain,-1,32 gzip,plain,-1,32 zstd,plain,-1,32 gzip,dictionary,-1,32 zstd,dictionary,-1,32 gzip,split,-1,32 zstd,split,-1,32 zstd,split_xor,-1,32, zstd,split_component,-1,32 zstd,split_rle,-1,32 gzip,split_xor,-1,32 gzip,split_component,-1,32 gzip,split_rle,-1,32 zstd,split,1,32 zstd,split,4,32 zstd,split,7,32 zstd,split,10,32 zstd,plain,1,32 zstd,plain,4,32 zstd,plain,7,32, zstd,plain,10,32 zstd,dictionary,1,32, zstd,dictionary,4,32, zstd,dictionary,7,32 zstd,dictionary,10,32"
./parquet_test -b $fp_data $cases

parquet_data="$(ls ../dataset/turbo-student-data-set/operational-data/*.parquet) ../dataset/turbo-student-data-set/parquet-data-set/GT61/2018-01-01/Can_01_SPEC.parquet"
./parquet_test -p $parquet_data $cases
