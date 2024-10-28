#!/bin/bash

# ./build/eda240819 WorkSpace/public/Benchmark/public_release/case_1.nodes WorkSpace/public/Benchmark/public_release/case_1.nets WorkSpace/public/Benchmark/public_release/case_1.timing Result/case_1_out.nodes

program="build/eda240819"
# 检查是否传入了参数
if [ -n "$1" ]; then
    case_number=$1
    input_nodes="WorkSpace/public/Benchmark/public_release/case_${case_number}.nodes"
    input_nets="WorkSpace/public/Benchmark/public_release/case_${case_number}.nets"
    input_timing="WorkSpace/public/Benchmark/public_release/case_${case_number}.timing"
    output_nodes="Result/case_${case_number}_out.nodes"
    $program "$input_nodes" "$input_nets" "$input_timing" "$output_nodes"
else
    # 循环从 1 到 10
    for i in {1..10}
    do
        # 构造文件名
        echo ready to run case${i}
        input_nodes="WorkSpace/public/Benchmark/public_release/case_${i}.nodes"
        input_nets="WorkSpace/public/Benchmark/public_release/case_${i}.nets"
        input_timing="WorkSpace/public/Benchmark/public_release/case_${i}.timing"
        output_nodes="Result/case_${i}_out.nodes"
        
        # 执行命令
        $program "$input_nodes" "$input_nets" "$input_timing" "$output_nodes"
        ./checker/checker checker/case${i}.run
    done
fi
