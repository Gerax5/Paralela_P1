# Bitácora de pruebas

El benchmark utilizado considera las dos partes de los archivos compilados:

- **SEQ** -> versión secuencial.
- **OMP** -> versión paralela con OpenMP.

El objetivo es medir cuánto mejora (o empeora) el rendimiento al paralelizar; para las pruebas se calculó el **speedup**, esto se define como:

$$
\text{Speedup} = \frac{T_{seq}}{T_{par}}
$$

donde:

- $T_{seq}$ = tiempo promedio de la versión secuencial.
- $T_{par}$ = tiempo promedio de la versión paralela.

- **Speedup = 1.0** -> No hay ganancia, ambas versiones tardan lo mismo.
- **Speedup > 1.0** -> La versión paralela es más rápida (hay mejora).

  - Ej: Speedup = 2.0 -> OMP es el doble de rápido que SEQ.
  - Ej: Speedup = 5.0 -> OMP es 5 veces más rápido.
- **Speedup < 1.0** -> La versión paralela es más lenta (hay overhead de paralelización).

## Benchmark bucle

### PC1 - Escritorio

| N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|-------|-------------------|-------------------|-----------------|
| 2000  | 6.598020          | 8.775120          | 0.752           |
| 4000  | 11.783380         | 12.836300         | 0.919           |
| 6000  | 17.933300         | 15.059940         | 1.194           |
| 8000  | 28.949420         | 16.253840         | 1.781           |
| 10000 | 48.921300         | 22.612800         | 2.166           |
| 12000 | 68.567910         | 21.369830         | 3.209           |
| 14000 | 82.506950         | 20.197140         | 3.942           |
| 16000 | 105.261280        | 23.017500         | 4.573           |
| 18000 | 123.027270        | 23.712520         | 5.188           |
| 20000 | 140.503360        | 26.834800         | 5.236           |

### PC2 - Laptop

| N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|-------|-------------------|-------------------|-----------------|
| 2000  | 11.660620         | 3.189670          | 3.659           |
| 4000  | 22.163180         | 4.908770          | 4.515           |
| 6000  | 31.974470         | 5.495560          | 5.818           |
| 8000  | 51.081030         | 11.178510         | 4.571           |
| 10000 | 66.904320         | 13.873110         | 4.823           |
| 12000 | 89.123800         | 20.712340         | 4.303           |
| 14000 | 119.171460        | 33.654470         | 3.542           |
| 16000 | 142.775740        | 32.417460         | 4.406           |
| 18000 | 162.633430        | 27.016560         | 6.020           |
| 20000 | 174.230730        | 29.735440         | 5.856           |

## Benchmark unitario (N = 50000)

| PC            | N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|---------------|-------|-------------------|-------------------|-----------------|
| PC escritorio | 50000 | 443.005050        | 38.483720         | 11.511          |
| Laptop        | 50000 | 823.389900        | 58.601760         | 14.051          |

## Evidencia de pruebas

### PC1 (Escritorio)

![Test usando 2000 - 10000 puntos](../../images/pruebas/pc1_test_2000-10000.png)

![Test usando 12000 - 20000 puntos](../../images/pruebas/pc1_test_12000-20000.png)

![Test unitario con 50000 puntos](../../images/pruebas/pc1_test_50000.png)

### PC2 (Laptop)

![Test usando 2000 - 8000 puntos](../../images/pruebas/pc2_test_2000-8000.png)

![Test usando 10000 - 16000 puntos](../../images/pruebas/pc2_test_10000-16000.png)

![Test usando 18000 - 20000 puntos](../../images/pruebas/pc2_test_18000-20000.png)

![Test unitario con 50000 puntos](../../images/pruebas/pc2_test_50000.png)

## Gráficas

### PC1 (Escritorio): Tiempos vs N

![PC1 Escritorio](../../images/pruebas/pc1_tiempos.png)

### PC2 (Laptop): Tiempos vs N

![PC2 Laptop](../../images/pruebas/pc2_tiempos.png)

### Comparación tiempos N=50000

![Comparación N50000](../../images/pruebas/n50000_comparacion.png)

### Speedup vs N

![Speedup vs N](../../images/pruebas/speedup_vs_n.png)

## Interpretación de resultados

- En **problemas pequeños** (N bajos), la versión **OMP** en PC1 llega a ser más lenta que SEQ, debido al overhead de gestión de threads.
- A partir de **N=6000** en PC1, OMP supera claramente a SEQ y escala hasta **5.2x** en N=20000.
- En PC2 (laptop), el **speedup es mayor desde el inicio**, llegando hasta **~6x** en N=18000–20000.  
- Para **N=50000**, ambas máquinas muestran una aceleración significativa:
  - PC1 (escritorio): ~11.5x  
  - PC2 (laptop): ~14.0x  
- Esto refleja que el **algoritmo paralelo escala bien con N**, y que las diferencias de hardware impactan tanto en los tiempos absolutos como en el speedup relativo.
