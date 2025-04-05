import os
import subprocess
import csv

# Define the directory containing the CNF files
cnf_directory = '../ssat-benchmarks/er-random-k-CNF'
output_dir = './er_kCNF/'
if not os.path.exists(output_dir):
    os.makedirs(output_dir)
csv_file_path = './er_kCNF/result.csv'
desired_idx1_list = [5]
desired_idx2_list= [40]

# Run the ganak command and generate output files
for filename in os.listdir(cnf_directory):
    if filename.endswith('.sdimacs'):
        # Extract idx1 and idx2 from the filename
        parts = filename.split('-')
        idx1 = int(parts[1])
        idx2 = int(parts[2])

        # Check if the file should be processed based on idx1 and idx2
        if idx1 in desired_idx1_list and idx2 in desired_idx2_list:
            cnf_file_path = os.path.join(cnf_directory, filename)
            output_file_path = os.path.join(output_dir, f"{filename}.txt")
            command = f"./build/ganak {cnf_file_path} > {output_file_path}"
            subprocess.run(command, shell=True)

# Extract time and cache size from each output file and write to CSV
if os.path.exists(csv_file_path):
    with open(csv_file_path, mode='a', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)
        for filename in os.listdir(output_dir):
            if filename.endswith('.txt'):
                output_file_path = os.path.join(output_dir, filename)
                with open(output_file_path, 'r') as file:
                    time_value = None
                    cache_size = None
                    for line in file:
                        if line.startswith('c time:'):
                            time_value = line.split(':')[1].strip().split('s')[0]
                        elif line.startswith('c bytes cache size'):
                            cache_size = line.split()[4]
                        if time_value and cache_size:
                            csv_writer.writerow([filename, time_value, cache_size])
                            break
else:
    with open(csv_file_path, mode='w', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(['filename', 'time', 'cache_size'])
        for filename in os.listdir(output_dir):
            if filename.endswith('.txt'):
                output_file_path = os.path.join(output_dir, filename)
                with open(output_file_path, 'r') as file:
                    time_value = None
                    cache_size = None
                    for line in file:
                        if line.startswith('c time:'):
                            time_value = line.split(':')[1].strip().split('s')[0]
                        elif line.startswith('c bytes cache size'):
                            cache_size = line.split()[4]
                        if time_value and cache_size:
                            csv_writer.writerow([filename, time_value, cache_size])
                            break

print("Processing complete and times with cache sizes written to CSV.")