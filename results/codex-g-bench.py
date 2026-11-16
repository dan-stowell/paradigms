#!/usr/bin/env python3
"""Run from repository root after building build-g/{p2par,p2async}."""
import json, math, os, re, subprocess, time
from pathlib import Path
LIMIT = ["prlimit", "--as=8589934592", "--"]
out = Path("results/codex-g-bench.jsonl").open("w", buffering=1)
def log(x):
    out.write(json.dumps(x) + "\n")
    print(json.dumps(x), flush=True)
def uptime():
    return subprocess.check_output(LIMIT + ["uptime"], text=True).strip()
def run(dataset, threads, variant, reps):
    env = dict(os.environ, OMP_NUM_THREADS=str(threads), OMP_DYNAMIC="false")
    env["OMP_WAIT_POLICY"] = "active" if variant == "active" else "passive"
    exe = "p2async" if variant == "async" else "p2par"
    start = time.monotonic()
    s = subprocess.check_output(LIMIT + [f"build-g/{exe}", f"data/{dataset}", str(reps)], env=env, text=True)
    assert "P2 OK" in s, s
    total = float(re.search(r"total ([0-9.]+) s", s)[1])
    return dict(reps=reps, seconds=total, ms=total*1000/reps, wall=time.monotonic()-start, output=s)
def block(dataset, threads, extra=False):
    log(dict(event="before", dataset=dataset, threads=threads, extra=extra, utc=time.strftime("%FT%TZ",time.gmtime()), uptime=uptime()))
    reps = {}
    for v in ["active", "passive", "async"]:
        r = run(dataset, threads, v, 1)
        reps[v] = max(1, math.ceil(1.5/r["seconds"]))
        log(dict(event="pilot", dataset=dataset, threads=threads, variant=v, extra=extra, **r))
    for i in range(5):
        # Rotate ordering to avoid giving a variant a fixed position.
        variants=["active", "passive", "async"]
        for v in variants[i%3:]+variants[:i%3]:
            while True:
                r = run(dataset, threads, v, reps[v])
                accepted = r["seconds"] >= 1
                log(dict(event="run" if accepted else "short-retry", dataset=dataset, threads=threads, variant=v, extra=extra, repeat=i, **r))
                if accepted: break
                reps[v] = math.ceil(reps[v] * 1.5 / max(r["seconds"], .001))
    log(dict(event="after", dataset=dataset, threads=threads, extra=extra, utc=time.strftime("%FT%TZ",time.gmtime()), uptime=uptime()))
for d in ["minio", "kube-apiserver"]:
    for t in [1, 8]: block(d,t)
# Eight CPU-bound Python workers, each independently capped to 180 seconds.
load = [subprocess.Popen(LIMIT + ["timeout", "180s", "python3", "-c", "while True: pass"]) for _ in range(8)]
try:
    block("minio", 8, True)
finally:
    for p in load: p.terminate()
    for p in load: p.wait()
