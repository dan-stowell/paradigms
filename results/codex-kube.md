# kube-apiserver, unchanged paradigm cells

Dataset: `/home/exedev/paradigms/data/kube-apiserver` (read-only). Checkout: `/home/exedev/paradigms-w`, branch `codex-kube`. All builds and measured processes use `systemd-run --user --scope -p MemoryMax=10G`; `OMP_WAIT_POLICY=passive`. No cell algorithms changed. Tools come from `/home/exedev/tools` without modifying them.

Measurements are in progress. Final tables and failure details will replace this paragraph.

Times are warm milliseconds, min / median / max over at least five rounds. Each system retains its bench.sh definition of a round (specified below); these definitions differ. The historical MinIO values are from GRID.md, not same-window reruns. Where GRID gives a range, ratios use its midpoint, so ratios are descriptive, not speedup claims. A missing comparable GRID value is N/A. Same-window tuned C samples are interleaved across the measurement window. The machine is shared; uptime/load is recorded, without waiting for quiet.
