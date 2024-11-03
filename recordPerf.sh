#需要切换到root用户

#运行case1 并用perf监控
sudo perf record -g build/eda240819 "WorkSpace/public/Benchmark/public_release/case_1.nodes" "WorkSpace/public/Benchmark/public_release/case_1.nets" "WorkSpace/public/Benchmark/public_release/case_1.timing" "Result/case_1_out.nodes"
#查看报告
perf report -f

