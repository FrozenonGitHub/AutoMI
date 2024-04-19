#!/bin/bash

# hostfile containing private IPs
host_file="/home/ubuntu/machines"

BUILD_DIR="/AutoMI/release/toolkits/graph_automi"

# data graph file prefix
graph_prefix="/data/livejournal-w6"

# query file
query_file="/data/livejournal-query-1-256.txt"

# for PowerLyra
baseline_num_queries=(16 32 64 128 256)

# for MultiLyra, AutoMI (w/o TF), AutoMI
dimitra_num_queries=(16 32 64 128 256)

for i in "${!baseline_num_queries[@]}"; do
    NUM_SOURCE_NODE=${baseline_num_queries[i]}

    echo "[powerlyra_sssp]: $NUM_SOURCE_NODE"
    mpiexec -n 32 -hostfile $host_file $BUILD_DIR/powerlyra_sssp --graph $graph_prefix --graph_opts="ingress=hybrid" --format=wtuple --query $query_file --num_query=$NUM_SOURCE_NODE --engine=plsync >> livejournal_powerlyra_sssp_out.txt 2>> livejournal_powerlyra_sssp_err.txt
done


for j in "${!dimitra_num_queries[@]}"; do
    NUM_SOURCE_NODE=${dimitra_num_queries[j]}

    echo "[multilyra_sssp]: $NUM_SOURCE_NODE"
    mpiexec -n 32 -hostfile $host_file $BUILD_DIR/multilyra_sssp --graph $graph_prefix --graph_opts="ingress=hybrid" --format=wtuple --query $query_file --num_query=$NUM_SOURCE_NODE --engine=plsync >> livejournal_multilyra_sssp_out.txt 2>> livejournal_multilyra_sssp_err.txt

    echo "[automi_sssp]: $NUM_SOURCE_NODE"
    mpiexec -n 32 -hostfile $host_file $BUILD_DIR/automi_sssp --graph $graph_prefix --graph_opts="ingress=hybrid" --format=wtuple --query $query_file --num_query=$NUM_SOURCE_NODE --engine=plsync >> livejournal_automi_sssp_out.txt 2>> livejournal_automi_sssp_err.txt
done
