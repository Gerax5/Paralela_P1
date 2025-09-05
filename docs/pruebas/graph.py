import pandas as pd
import matplotlib.pyplot as plt

# -----------------------
# Datos
# -----------------------
pc1 = pd.DataFrame({
    "N":[2000,4000,6000,8000,10000,12000,14000,16000,18000,20000],
    "SEQ":[6.598020,11.783380,17.933300,28.949420,48.921300,68.567910,82.506950,105.261280,123.027270,140.503360],
    "OMP":[8.775120,12.836300,15.059940,16.253840,22.612800,21.369830,20.197140,23.017500,23.712520,26.834800],
})
pc1["Speedup"] = pc1["SEQ"]/pc1["OMP"]

pc2 = pd.DataFrame({
    "N":[2000,4000,6000,8000,10000,12000,14000,16000,18000,20000],
    "SEQ":[11.660620,22.163180,31.974470,51.081030,66.904320,89.123800,119.171460,142.775740,162.633430,174.230730],
    "OMP":[3.189670,4.908770,5.495560,11.178510,13.873110,20.712340,33.654470,32.417460,27.016560,29.735440],
})
pc2["Speedup"] = pc2["SEQ"]/pc2["OMP"]

once = pd.DataFrame({
    "PC":["PC escritorio","Laptop"],
    "N":[50000,50000],
    "SEQ":[443.005050,823.389900],
    "OMP":[38.483720,58.601760],
})
once["Speedup"] = once["SEQ"]/once["OMP"]

# -----------------------
# 1) Tiempos PC1
# -----------------------
plt.figure()
plt.plot(pc1["N"], pc1["SEQ"], marker="o", label="SEQ")
plt.plot(pc1["N"], pc1["OMP"], marker="s", label="OMP")
plt.xlabel("N")
plt.ylabel("Tiempo promedio (ms)")
plt.title("PC1 (Escritorio): Tiempos vs N")
plt.legend()
plt.grid(True, linestyle="--", alpha=0.4)
plt.tight_layout()
plt.savefig("pc1_tiempos.png", dpi=200)

# -----------------------
# 2) Tiempos PC2
# -----------------------
plt.figure()
plt.plot(pc2["N"], pc2["SEQ"], marker="o", label="SEQ")
plt.plot(pc2["N"], pc2["OMP"], marker="s", label="OMP")
plt.xlabel("N")
plt.ylabel("Tiempo promedio (ms)")
plt.title("PC2 (Laptop): Tiempos vs N")
plt.legend()
plt.grid(True, linestyle="--", alpha=0.4)
plt.tight_layout()
plt.savefig("pc2_tiempos.png", dpi=200)

# -----------------------
# 3) Speedup vs N (ambas PCs)
# -----------------------
plt.figure()
plt.plot(pc1["N"], pc1["Speedup"], marker="o", label="PC1 (Escritorio)")
plt.plot(pc2["N"], pc2["Speedup"], marker="s", label="PC2 (Laptop)")
plt.xlabel("N")
plt.ylabel("Speedup (Tseq/Tomp)")
plt.title("Speedup vs N")
plt.legend()
plt.grid(True, linestyle="--", alpha=0.4)
plt.tight_layout()
plt.savefig("speedup_vs_n.png", dpi=200)

# -----------------------
# 4) Comparación N=50000
# -----------------------
plt.figure()
x = range(len(once))
width = 0.35
plt.bar([i - width/2 for i in x], once["SEQ"], width, label="SEQ")
plt.bar([i + width/2 for i in x], once["OMP"], width, label="OMP")
plt.xticks(list(x), once["PC"])
plt.ylabel("Tiempo promedio (ms)")
plt.title("Comparación tiempos N=50000")
plt.legend()
plt.tight_layout()
plt.savefig("n50000_comparacion.png", dpi=200)

print("Listo. Archivos generados:",
      "pc1_tiempos.png, pc2_tiempos.png, speedup_vs_n.png, n50000_comparacion.png")
