## Futhark

Warm ms; min / median / max of five process medians. All configurations request huge pages.

| cell | config | oracle | kube warm | rounds | GRID MinIO | kube / MinIO |
|---|---|---|---|---|---|---|
| 1a | 1+huge | OK | 285.75 / 302.78 / 373.24 | 5 | 79 | 3.83x |
| 1a | 8spin+huge | OK | 163.04 / 253.40 / 419.91 | 5 | 24 | 10.56x |
| 1a | 8yield+huge | OK | 127.99 / 204.00 / 344.68 | 5 | 24 | 8.50x |
| 1b | 1+huge | OK | 1066.73 / 1275.39 / 2100.93 | 5 | — | — |
| 1b | 8spin+huge | OK | 326.43 / 332.00 / 535.80 | 5 | — | — |
| 1b | 8yield+huge | OK | 297.82 / 336.01 / 527.97 | 5 | — | — |
| 1b_scan | 1+huge | OK | 99.56 / 124.99 / 357.99 | 5 | 47 | 2.66x |
| 1b_scan | 8spin+huge | OK | 56.92 / 88.02 / 157.21 | 5 | 18 | 4.89x |
| 1b_scan | 8yield+huge | OK | 44.34 / 71.98 / 137.80 | 5 | 18 | 4.00x |
| 1c | 1+huge | OK | 154.16 / 207.44 / 278.01 | 5 | 111 | 1.87x |
| 1c | 8spin+huge | OK | 123.59 / 140.93 / 262.21 | 5 | 38 | 3.71x |
| 1c | 8yield+huge | OK | 92.34 / 112.47 / 217.22 | 5 | 38 | 2.96x |
| 2 | 1+huge | OK | 159.19 / 191.28 / 309.29 | 5 | 97 | 1.97x |
| 2 | 8spin+huge | OK | 220.04 / 270.74 / 420.03 | 5 | 58 | 4.67x |
| 2 | 8yield+huge | OK | 174.70 / 300.04 / 671.97 | 5 | 58 | 5.17x |
| 2lazy | 1+huge | OK | 248.99 / 276.91 / 320.05 | 5 | 165 | 1.68x |
| 2lazy | 8spin+huge | OK | 355.31 / 375.88 / 589.76 | 5 | 90 | 4.18x |
| 2lazy | 8yield+huge | OK | 342.47 / 397.32 / 917.94 | 5 | 90 | 4.41x |
| 3a | 1+huge | OK | 33.68 / 47.75 / 60.16 | 5 | 22 | 2.17x |
| 3a | 8spin+huge | OK | 28.89 / 54.28 / 80.15 | 5 | 10 | 5.43x |
| 3a | 8yield+huge | OK | 44.42 / 60.04 / 117.58 | 5 | 10 | 6.00x |
| 3a_loop | 1+huge | OK | 21.61 / 34.00 / 42.97 | 5 | 14 | 2.43x |
| 3a_loop | 8spin+huge | OK | 32.64 / 41.44 / 51.15 | 5 | 7.3 | 5.68x |
| 3a_loop | 8yield+huge | OK | 28.69 / 32.72 / 72.06 | 5 | 7.3 | 4.48x |
| 3b | 1+huge | OK | 128.06 / 146.77 / 567.02 | 5 | 107 | 1.37x |
| 3b | 8spin+huge | OK | 60.78 / 79.88 / 140.98 | 5 | 41 | 1.95x |
| 3b | 8yield+huge | OK | 62.47 / 81.90 / 131.05 | 5 | 41 | 2.00x |

## Halide

| cell / schedule / mapping | threads | oracle | kube warm min / median / max ms | GRID MinIO | kube / MinIO |
|---|---|---|---|---|---|
| counts | 1 | OK | 27.26 / 35.68 / 61.16 | — | — |
| 1a resolve_f | 1 | OK | 502.87 / 579.73 / 657.91 | 81–93 | 6.66x |
| 1b reltarget_f | 1 | OK | 85.31 / 105.18 / 110.49 | 42.7–46 | 2.37x |
| 1c contentjoin_f | 1 | OK | 295.76 / 302.37 / 341.33 | 122–133 | 2.37x |
| 2 flood_t_f | 1 | OK | 815.06 / 899.27 / 938.03 | — | — |
| 2 flood_f_f | 1 | OK | 589.35 / 615.15 / 733.41 | — | — |
| 3a layout_c0_w0 | 1 | OK | 90.89 / 98.70 / 107.51 | — | — |
| 3a layout_c0_w1 | 1 | OK | 95.59 / 100.15 / 105.42 | — | — |
| 3a layout_c1_w0 | 1 | OK | 102.40 / 114.00 / 125.00 | — | — |
| 3a layout_c1_w1 | 1 | OK | 100.04 / 115.26 / 121.02 | — | — |
| 3a layout_c2_w0 | 1 | OK | 100.74 / 103.92 / 108.52 | — | — |
| 3a layout_c2_w1 | 1 | OK | 100.27 / 104.08 / 111.71 | — | — |
| 3a layout_c3_w0 | 1 | OK | 89.61 / 93.16 / 111.78 | — | — |
| 3a layout_c3_w1 | 1 | OK | 87.60 / 90.88 / 103.83 | — | — |
| 3b reloc_f0_pf_v0 warm | 1 | OK | 113.91 / 116.41 / 123.22 | — | — |
| 3b reloc_f0_pf_v0 fresh 4KB | 1 | OK | 146.25 / 172.28 / 193.23 | — | — |
| 3b reloc_f0_pf_v0 fresh 2MB | 1 | OK | 105.06 / 105.48 / 113.17 | — | — |
| 3b reloc_f0_pf_v32 warm | 1 | OK | 105.18 / 106.07 / 108.03 | — | — |
| 3b reloc_f0_pf_v32 fresh 4KB | 1 | OK | 144.42 / 145.88 / 148.27 | — | — |
| 3b reloc_f0_pf_v32 fresh 2MB | 1 | OK | 109.71 / 110.37 / 122.76 | — | — |
| 3b reloc_f0_pt_v0 warm | 1 | OK | 105.20 / 106.86 / 113.41 | — | — |
| 3b reloc_f0_pt_v0 fresh 4KB | 1 | OK | 142.97 / 144.58 / 154.63 | — | — |
| 3b reloc_f0_pt_v0 fresh 2MB | 1 | OK | 112.42 / 113.21 / 123.47 | — | — |
| 3b reloc_f0_pt_v32 warm | 1 | OK | 110.64 / 111.98 / 129.01 | — | — |
| 3b reloc_f0_pt_v32 fresh 4KB | 1 | OK | 141.97 / 144.36 / 152.10 | — | — |
| 3b reloc_f0_pt_v32 fresh 2MB | 1 | OK | 110.20 / 111.86 / 112.94 | — | — |
| counts | 8 | OK | 5.11 / 6.71 / 7.22 | — | — |
| 1a resolve_t | 8 | OK | 79.41 / 84.46 / 94.32 | 19.8–27 | 3.61x |
| 1b reltarget_t | 8 | OK | 19.80 / 22.67 / 38.96 | 9.9–10.9 | 2.18x |
| 1c contentjoin_t | 8 | OK | 125.65 / 147.42 / 163.86 | 33–36.5 | 4.24x |
| 2 flood_t_t | 8 | OK | 350.00 / 399.29 / 410.53 | 79–90 | 4.73x |
| 2 flood_f_t | 8 | OK | 294.99 / 328.94 / 344.29 | 70–127 | 3.34x |
| 3a layout_c0_w0 | 8 | OK | 46.93 / 48.04 / 59.51 | — | — |
| 3a layout_c0_w1 | 8 | OK | 46.76 / 47.80 / 48.37 | — | — |
| 3a layout_c1_w0 | 8 | OK | 54.26 / 57.02 / 58.58 | — | — |
| 3a layout_c1_w1 | 8 | OK | 53.86 / 56.89 / 59.50 | — | — |
| 3a layout_c2_w0 | 8 | OK | 48.00 / 53.95 / 58.48 | — | — |
| 3a layout_c2_w1 | 8 | OK | 43.80 / 50.53 / 58.37 | — | — |
| 3a layout_c3_w0 | 8 | OK | 47.38 / 49.14 / 101.58 | — | — |
| 3a layout_c3_w1 | 8 | OK | 71.56 / 84.44 / 137.00 | 20–23 | 3.93x |
| 3b reloc_f0_pf_v0 warm | 8 | OK | 123.04 / 187.72 / 201.33 | — | — |
| 3b reloc_f0_pf_v0 fresh 4KB | 8 | OK | 263.76 / 285.11 / 352.86 | — | — |
| 3b reloc_f0_pf_v0 fresh 2MB | 8 | OK | 170.93 / 186.20 / 189.83 | — | — |
| 3b reloc_f0_pf_v32 warm | 8 | OK | 141.76 / 163.06 / 172.87 | — | — |
| 3b reloc_f0_pf_v32 fresh 4KB | 8 | OK | 189.92 / 230.12 / 246.29 | — | — |
| 3b reloc_f0_pf_v32 fresh 2MB | 8 | OK | 136.13 / 139.89 / 169.52 | — | — |
| 3b reloc_f0_pt_v0 warm | 8 | OK | 62.16 / 66.76 / 72.31 | — | — |
| 3b reloc_f0_pt_v0 fresh 4KB | 8 | OK | 111.90 / 136.34 / 154.57 | — | — |
| 3b reloc_f0_pt_v0 fresh 2MB | 8 | OK | 80.40 / 97.18 / 112.40 | — | — |
| 3b reloc_f0_pt_v32 warm | 8 | OK | 49.25 / 86.54 / 96.07 | 24.9–27 | 3.33x |
| 3b reloc_f0_pt_v32 fresh 4KB | 8 | OK | 83.71 / 113.51 / 151.62 | — | — |
| 3b reloc_f0_pt_v32 fresh 2MB | 8 | OK | 71.49 / 78.57 / 81.21 | 27–30.8 | 2.72x |
