import re
import matplotlib.pyplot as plt
import os

def parse_log_file(filepath):
    join_times = {}
    joined_times = {}

    pattern = re.compile(r'\[(\d+\.\d+)\] Node=(\d+) (joining|joined)')

    with open(filepath, 'r') as file:
        for line in file:
            match = pattern.search(line)
            if match:
                timestamp = float(match.group(1))
                node_id = int(match.group(2))
                event = match.group(3)

                if event == "joining":
                    join_times[node_id] = timestamp
                elif event == "joined":
                    joined_times[node_id] = timestamp

    return join_times, joined_times

def compute_durations(join_times, joined_times):
    return {
        node: joined_times[node] - join_times[node]
        for node in joined_times
        if node in join_times
    }

def plot_and_save_durations(durations, output_filepath):
    sorted_nodes = sorted(durations.keys())
    sorted_durations = [durations[node] for node in sorted_nodes]

    plt.figure(figsize=(14, 6))
    plt.bar(sorted_nodes, sorted_durations)
    plt.xlabel("ID du nœud")
    plt.ylabel("Durée de jonction (secondes)")
    plt.title("Temps nécessaire à chaque nœud pour rejoindre le système")
    plt.xticks(sorted_nodes)
    plt.grid(axis='y')
    plt.tight_layout()
    plt.savefig(output_filepath)
    print(f"Graphique enregistré sous : {output_filepath}")

if __name__ == "__main__":
    log_file = "./benchmark/join/log_40_327680.txt"  # Chemin vers le fichier de log
    output_image = log_file + ".png"  # Nom du fichier de sortie PNG

    join_times, joined_times = parse_log_file(log_file)
    durations = compute_durations(join_times, joined_times)
    plot_and_save_durations(durations, output_image)
