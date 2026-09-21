import matplotlib.pyplot as plt
import numpy as np
import sys

def plot_latency_cdf(filename, title, output_pdf):
    data = []
    with open(filename) as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) == 3:
                try:
                    data.append(float(parts[2]))
                except:
                    continue
    data = np.sort(data)
    percentiles = np.linspace(0, 100, len(data), endpoint=False)
    plt.figure(figsize=(8, 5))
    plt.plot(data, percentiles)
    plt.xlabel("Latency (μs)")
    plt.ylabel("Percentile (%)")
    plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output_pdf)

if __name__ == "__main__":
    filename = sys.argv[1]
    mode = sys.argv[2]
    title = f"VenkatSaiAravindDevarampati-Pagemap-Lat-CDF ({mode})"
    output = f"pagemap_cdf_{mode}.pdf"
    plot_latency_cdf(filename, title, output)
    print(f"[✓] CDF PDF saved as: {output}")
