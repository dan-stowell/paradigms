#!/usr/bin/env python3
"""Generate timed copies; compile with -Iref/flat. Run only with one thread.
Timers preserve partition fusion and sum phase durations across partitions.
"""
from pathlib import Path
for name in ["p1par", "p1compact"]:
 s=Path(f"ref/flat/{name}.c").read_text()
 s=s.replace("double best =", "double phase[6] = {0}; double pt;\n\tdouble best =",1)
 s=s.replace("double t0 = now();", "double t0 = now(); pt = t0;",1)
 def mark(needle, n):
  global s
  assert needle in s, needle
  s=s.replace(needle, f"phase[{n}] += now()-pt; pt=now();\n\t\t"+needle,1)
 mark("canon[0] = 0;",0)
 # prefix and scatter charged together
 mark("static rec *tabs[NP];",1)
 if name=="p1par":
  mark("// candidates: a CAS-max",2)
  needle="\t\t#pragma omp parallel for schedule(static, 65536)\n\t\tfor (u32 g = 1; g <= N; g++) {"
  a=s.index("// candidates: a CAS-max"); b=s.index(needle,a); c=s.index(needle,b+len(needle))
  s=s[:c]+"\t\tphase[3] += now()-pt; pt=now();\n"+s[c:]
  a=s.index("\t\t#pragma omp parallel for schedule(dynamic, 1)",c)
  s=s[:a]+"\t\tphase[4] += now()-pt; pt=now();\n"+s[a:]
  mark("double t1 = now();",5)
 else:
  # Per-partition clocks: keep the table hot, no additional barriers.
  s=s.replace("size_t n = pbase[p + 1] - pbase[p];", "pt=now();\n\t\t\tsize_t n = pbase[p + 1] - pbase[p];",1)
  needle="for (size_t i = cbase[p]; i < cbase[p+1]; i++) {"
  mark(needle,2)
  a=s.index(needle); b=s.index(needle,a+len(needle))
  s=s[:b]+"phase[3] += now()-pt; pt=now();\n\t\t\t"+s[b:]
  mark("for (size_t i = pbase[p]; i < pbase[p+1]; i++) {",4)
  a=s.index("\n\t\tfor (u32 i = 0; i < nb;")
  s=s[:a].rsplit("\n\t\t}",1)[0]+"\n\t\t\tphase[5] += now()-pt;\n\t\t}\n\t\tpt=now();"+s[a:]
  mark("double t1 = now();",5)
 s=s.replace("long bad =", "printf(\"phases ms: %.6f %.6f %.6f %.6f %.6f %.6f\\n\", phase[0]/reps*1e3, phase[1]/reps*1e3, phase[2]/reps*1e3, phase[3]/reps*1e3, phase[4]/reps*1e3, phase[5]/reps*1e3);\n\tlong bad =",1)
 s=s.replace("int T = omp_get_max_threads(),", "if (omp_get_max_threads() != 1) { fprintf(stderr, \"phase copies require one thread\\n\"); return 2; }\n\tint T = omp_get_max_threads(),",1)
 Path(f"build-h/{name}-phases.c").write_text(s)
