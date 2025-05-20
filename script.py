import subprocess
import re

# This script runs a command for each trace file, captures the output,
# and extracts the LLC TOTAL ACCESS, HIT, and MISS values to calculate the miss rate.
# It assumes that the command to run is "./lru-config1" and that the trace files are in a "trace" directory
# and are gzipped. The script also handles errors in command execution.
# The script also assumes that the lrcu-config1 program has been compiled and is executable.
# If it has not been compiled, do so by running: g++ -Wall --std=c++11 -o lru-config1 example/lru.cc lib/config1.a


# List of trace file names
traces = [
    "bzip2_10M.trace.gz",
    "graph_analytics_10M.trace.gz",
    "libquantum_10M.trace.gz",
    "mcf_10M.trace.gz"
]

# Base command prefix
base_cmd = "./lru-config1 -warmup_instructions 1000000 -simulation_instructions 10000000 -traces trace/"

miss_rates = []

for trace in traces:
    cmd = base_cmd + trace
    print(f"Running: {cmd}")
    
    try:
        # Run the command and capture output
        result = subprocess.run(cmd.split(), capture_output=True, text=True, check=True)
        output = result.stdout

        # Search for the LLC TOTAL ACCESS line
        match = re.search(r"LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)", output)
        if match:
            total_access = int(match.group(1))
            misses = int(match.group(3))
            miss_rate = misses / total_access
            miss_rates.append(miss_rate)
            print(f"{trace}: Miss Rate = {miss_rate:.4f}")
        else:
            print(f"LLC stats not found in output of {trace}")

    except subprocess.CalledProcessError as e:
        print(f"Error running command for {trace}:\n{e.stderr}")

# Calculate and print average miss rate
if miss_rates:
    avg_miss_rate = sum(miss_rates) / len(miss_rates)
    print(f"\nAverage Miss Rate: {avg_miss_rate:.4f}")
else:
    print("No miss rates computed.")