read_arch /home/public/Arch/fpga.lib /home/public/Arch/fpga.scl /home/public/Arch/fpga.clk
report_arch
read_design /home/public/Benchmark/public_release/case_3.nodes /home/public/Benchmark/public_release/case_3.nets /home/public/Benchmark/public_release/case_3.timing
report_design
read_output /home/public/Test/Result/case_3.nodes.out
legal_check
report_wirelength
report_pin_density
exit
