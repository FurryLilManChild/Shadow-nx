#!/usr/bin/env python3
"""check_log.py <shadow.log> [repo]: gate 3. Reads the build line at the top of a Switch log (written by main.cpp from
switch/stamp.cmake: "build switch-X (date, commit <hash>)") and compares the commit with the repo's origin/main after a
fetch. MATCH (exit 0) means the log came from the latest pushed commit; anything else (exit 1) stops the report."""
import re, subprocess, sys

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    log, repo = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else '.')
    head = open(log, encoding='utf-8', errors='replace').read(20000)
    m = re.search(r'build (switch-[\w.]+) \(([^,]+), commit ([0-9a-f]+)(-dirty)?\)', head)
    if not m:
        print('MISMATCH: no build commit in the log (a build from before 0.5.0, or a broken stamp)'); return 1
    version, when, commit, dirty = m.group(1), m.group(2), m.group(3), m.group(4)
    subprocess.run(['git', '-C', repo, 'fetch', '-q', 'origin'], check=False)
    latest = subprocess.run(['git', '-C', repo, 'rev-parse', 'origin/main'], capture_output=True, text=True).stdout.strip()
    ok = latest.startswith(commit) and not dirty
    print(f"{'MATCH' if ok else 'MISMATCH'}: log {version} built {when} from {commit}{dirty or ''}; origin/main {latest[:12]}")
    return 0 if ok else 1

sys.exit(main())
