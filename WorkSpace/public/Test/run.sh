
rm -rf run_case*.log  run_case*.tcl   

arch_path="/home/public/Arch"
bench_path="/home/public/Benchmark/public_release"
output_path="/home/public/Test/Result"

exec_path="/home/public/Checker/eda_2024_track8_checker"

for ((i=1; i <=10; i++)); do	
  echo "read_arch ${arch_path}/fpga.lib ${arch_path}/fpga.scl ${arch_path}/fpga.clk" >> "run_case${i}.tcl" 
  echo "report_arch" >> "run_case${i}.tcl"
  echo "read_design ${bench_path}/case_${i}.nodes ${bench_path}/case_${i}.nets ${bench_path}/case_${i}.timing" >> "run_case${i}.tcl"
  echo "report_design" >> "run_case${i}.tcl"
  echo "read_output ${output_path}/case_${i}.nodes.out" >> "run_case${i}.tcl"
  echo "legal_check" >> "run_case${i}.tcl"
  echo "report_wirelength" >> "run_case${i}.tcl"
  echo "report_pin_density" >> "run_case${i}.tcl"
  echo "exit" >> "run_case${i}.tcl"

  ${exec_path}/checker run_case${i}.tcl | tee run_case${i}.log
done
