import subprocess
import time

# Arrays for the 2nd and 3rd arguments
arg2_list = list(range(2, 40))
arg3_list = [10]

# Output file for timing results only
output_file = "./benchmark/sort/results_sort.txt"

with open(output_file, "w") as f:
    f.write("Arg2\tArg3\tTime(s)\n")
    for arg2 in arg2_list:
        for arg3 in arg3_list:
            # Build the command
            cmd = ["./build/main-psar", "5000", str(arg2), str(arg3)]

            print(f"Running: {' '.join(cmd)}")
            start_time = time.time()

            # Run and show stdout/stderr in real-time
            try:
                subprocess.run(cmd, check=True)
                elapsed = time.time() - start_time
            except subprocess.CalledProcessError:
                elapsed = -1  # failed execution

            # Save timing result
            f.write(f"{arg2}\t{arg3}\t{elapsed:.6f}\n")

print(f"\nBenchmark terminé. Résultats enregistrés dans {output_file}")
