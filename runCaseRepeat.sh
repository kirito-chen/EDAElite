#!/bin/bash

program="build/eda240819"



# 检查是否传入了参数
if [ -n "$1" ]; then
    case=$1
    seondDir=Case${case}
    rm Result/${seondDir}/*
    # 循环从 1 到 11
    for i in {1..3}
    do
        # 构造文件名
        echo ready to run case${case}
        input_nodes="WorkSpace/public/Benchmark/public_release/case_${case}.nodes"
        input_nets="WorkSpace/public/Benchmark/public_release/case_${case}.nets"
        input_timing="WorkSpace/public/Benchmark/public_release/case_${case}.timing"
        output_nodes="Result/${seondDir}/case_${case}_${i}_out.nodes"
        # 执行命令
        $program "$input_nodes" "$input_nets" "$input_timing" "$output_nodes"
    done
fi