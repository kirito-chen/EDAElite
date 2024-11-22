
#!/bin/bash

program="build/eda240819"
# 检查是否传入了参数
if [ -n "$1" ]; then
    case_number=$1
    echo ready to run case${case_number}
    input_nodes="WorkSpace/public/Benchmark/public_release/case_${case_number}.nodes"
    input_nets="WorkSpace/public/Benchmark/public_release/case_${case_number}.nets"
    input_timing="WorkSpace/public/Benchmark/public_release/case_${case_number}.timing"
    output_nodes="Result/case_${case_number}_out.nodes"
    
    if [ "$2" == "echo" ]; then
        echo "$program \"$input_nodes\" \"$input_nets\" \"$input_timing\" \"$output_nodes\""
    else
        $program "$input_nodes" "$input_nets" "$input_timing" "$output_nodes"
        ./checker/checker checker/case${case_number}.run
    fi
else
    # 清空Result文件夹
    rm -f Result/*
    rm data.json
    # 循环从 1 到 11
    for i in {1..11}
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