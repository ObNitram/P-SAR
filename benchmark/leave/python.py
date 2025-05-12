import re
import matplotlib.pyplot as plt

def parse_log_file(filepath):
    leaving_times = {}
    leaved_times = {}

    pattern = re.compile(r'\[(\d+\.\d+)\] Node=(\d+) (leaving|leaved)')

    with open(filepath, 'r') as file:
        for line in file:
            match = pattern.search(line)
            if match:
                timestamp = float(match.group(1))
                node_id = int(match.group(2))
                event = match.group(3)

                if event == "leaving":
                    leaving_times[node_id] = timestamp
                elif event == "leaved":
                    leaved_times[node_id] = timestamp

    return leaving_times, leaved_times

def compute_durations(leaving_times, leaved_times):
    return {
        node: leaved_times[node] - leaving_times[node]
        for node in leaved_times
        if node in leaving_times
    }

def plot_and_save_durations(durations, output_filepath):
    sorted_nodes = sorted(durations.keys())
    sorted_durations = [durations[node] for node in sorted_nodes]

    plt.figure(figsize=(14, 6))
    plt.bar(sorted_nodes, sorted_durations)
    plt.xlabel("ID du nœud")
    plt.ylabel("Durée de départ (secondes)")
    plt.title("Temps nécessaire à chaque nœud pour quitter le système")
    plt.xticks(sorted_nodes)
    plt.grid(axis='y')
    plt.tight_layout()
    plt.savefig(output_filepath)
    print(f"Graphique enregistré sous : {output_filepath}")

if __name__ == "__main__":
    log_file = "benchmark/leave/log_leaving_20_163840.txt"  # Nom du fichier contenant les logs
    output_image = log_file + ".png"  # Nom du fichier image PNG

    leaving_times, leaved_times = parse_log_file(log_file)
    durations = compute_durations(leaving_times, leaved_times)
    plot_and_save_durations(durations, output_image)
