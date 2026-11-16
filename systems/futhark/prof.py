#!/usr/bin/env python3
# prof.py: sum a Futhark runtime report (FUT_PROFILE=1 stderr) by task and source location; prints the top lines.
import json, sys, collections
r = json.load(open(sys.argv[1]))
tot = collections.Counter()
for e in r["events"]:
    n = e["name"]
    if "_task_" not in n and not n.startswith("futhark_mc_segmap_parloop") or "parloop_" in n and not n.endswith("_total") and "segmap" not in n:
        pass
    if "_task_" in n:  # one event per parallel operation, wall time of the whole op
        tot[(n.split("_task_")[0], e["provenance"])] += e["duration"]
s = sum(tot.values())
print(f"total task time {s/1e3:.1f} ms (profiling inflates it)")
for (k, p), v in tot.most_common(int(sys.argv[2]) if len(sys.argv) > 2 else 15):
    print(f"{v/1e3:9.1f} ms  {k:28s} {p}")
