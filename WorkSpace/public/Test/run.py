import subprocess
import os
import time
import psutil
from tabulate import tabulate

def generate_run_tcl(case_number):
    tcl_content = f"""
    read_arch /home/cdong/Lab/EDA_Contest_Checker/Server/Arch/fpga.lib /home/cdong/Lab/EDA_Contest_Checker/Server/Arch/fpga.scl /home/cdong/Lab/EDA_Contest_Checker/Server/Arch/fpga.clk
    report_arch

    read_design /home/cdong/Lab/EDA_Contest_Checker/Server/Benchmark/public_release/case_{case_number}.nodes /home/cdong/Lab/EDA_Contest_Checker/Server/Benchmark/public_release/case_{case_number}.nets /home/cdong/Lab/EDA_Contest_Checker/Server/Benchmark/public_release/case_{case_number}.timing
    report_design

    read_output /home/cdong/Lab/EDA_Contest_Checker/Server/Benchmark/public_release/case_{case_number}.nodes

    legal_check
    report_wirelength
    report_pin_density
    exit
    """
    tcl_filename = f"run_case_{case_number}.tcl"
    with open(tcl_filename, "w") as tcl_file:
        tcl_file.write(tcl_content)
    return tcl_filename

def run_checker(binary, case_number):
      
    # Generate TCL file for this case
    tcl_file = generate_run_tcl(case_number)
    
    # Start timing
    start_time = time.time()
    
    # Run the binary with the generated TCL file and save the result in a log file
    with open(f"run_case_{case_number}.log", "w") as log_file:
        process = subprocess.Popen([binary, tcl_file], stdout=log_file, stderr=log_file)

    # Monitor CPU and memory usage
    cpu_percent = []
    memory_usage = []
    thread_count = []
    p = psutil.Process(process.pid)
    
    while process.poll() is None:
        cpu_percent.append(p.cpu_percent(interval=0.1))
        memory_usage.append(p.memory_info().rss / 1024 / 1024)  # Convert to MB
        thread_count.append(p.num_threads())
    stdout, stderr = process.communicate()
    
    # End timing
    end_time = time.time()
    runtime = end_time - start_time
    
    # Calculate average CPU and max memory usage
    avg_cpu = sum(cpu_percent) / len(cpu_percent) if cpu_percent else 0
    max_memory = max(memory_usage) if memory_usage else 0
    max_thread = max(thread_count) if thread_count else 0

    # Print the result, runtime, and resource usage
    print(f"Case {case_number}:")
    print(f"Runtime: {runtime:.4f} seconds")
    print(f"Average CPU usage: {avg_cpu:.2f}%")
    print(f"Peak memory usage: {max_memory:.2f} MB")
    print(f"Max thread count: {max_thread}")
    print("-" * 40)
       
    result = [f"Case {case_number}", f"{runtime:.4f} seconds", f"{avg_cpu:.2f}%", f"{max_memory:.2f} MB", max_thread]
    
    return runtime, avg_cpu, max_memory, result

def main():
    # Ensure checker.exe exists
    binary = "/home/cdong/Lab/EDA_Contest_Checker/Server/eda_2024_track8_checker/checker"

    if not os.path.exists(binary):
        print("Error: binary is not found in path" + binary + ".")
        return
     
    results = [] 
    total_runtime = 0
    total_cpu = 0
    max_memory = 0
    # Run for 10 cases
    for case in range(1, 10):
        runtime, avg_cpu, memory, result = run_checker(binary, case)
        total_runtime += runtime
        total_cpu += avg_cpu
        max_memory = max(max_memory, memory)
        results.append(result)

    headers = ["Case", "Runtime", "Average CPU", "Peak Memory", "Max Thread"]
    print(tabulate(results, headers=headers, tablefmt="grid"))

    print(f"Total runtime for all cases: {total_runtime:.4f} seconds")
    print(f"Average runtime per case: {total_runtime/10:.4f} seconds")
    print(f"Average CPU usage across all cases: {total_cpu/10:.2f}%")
    print(f"Peak memory usage across all cases: {max_memory:.2f} MB")

if __name__ == "__main__":
    main()
