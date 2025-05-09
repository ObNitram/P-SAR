import matplotlib.pyplot as plt
from collections import defaultdict


# Fichier contenant les résultats
filename = "./benchmark/test2/results_scalling.txt"


# Regrouper les résultats par valeur de arg3
results_by_arg3 = defaultdict(list)


# Lecture du fichier
with open(filename, "r") as f:
    next(f)  # Skip header
    for line in f:
        parts = line.strip().split("\t")
        if len(parts) != 3:
            continue
        arg2, arg3, time = parts
        arg2 = int(arg2)
        arg3 = int(arg3)
        time = float(time)
        results_by_arg3[arg3].append((arg2, time))


# Génération d'un graphique par valeur de arg3
for arg3, data in results_by_arg3.items():
    # Trier les points selon arg2
    data.sort()
    arg2_vals, time_vals = zip(*data)

    plt.figure(figsize=(10, 6))
    plt.plot(arg2_vals, time_vals, marker="o")
    plt.title(f"Temps d'exécution pour un traitement de 5s par node")
    plt.xlabel("Nombre de nodes")
    plt.ylabel("Temps (s)")
    plt.grid(True)
    plt.tight_layout()

    # Créer un chemin de sortie dans le même dossier que le fichier d'entrée

    output_path = filename.replace(".txt", f".png")

    plt.savefig(output_path)
    plt.close()
    print(f"Graphique sauvegardé : {output_path}")
