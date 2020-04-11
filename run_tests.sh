#!/bin/sh

DATASET_DIR="/home/martin/code/guided_research/dataset/final_evaluation"
DATASET="$(ls $DATASET_DIR/64bit/*.dp) $(ls $DATASET_DIR/32bit/*.sp)"
TESTCASES="uncompressed,plain,-1 uncompressed,dictionary,-1 zstd,byte_stream_split,-1 zstd,plain,-1 lz4,byte_stream_split,-1 lz4,plain,-1"
NUM_RUNS=8

# Run memory benchmark
./parquet_test -b $DATASET -c $TESTCASES -r $NUM_RUNS > memory_results.txt

# Run IO benchmark
# This requires sudo rights since a sysctl is performed.
sudo ./parquet_test -b $DATASET -c $TESTCASES -r $NUM_RUNS -io > io_results.txt
