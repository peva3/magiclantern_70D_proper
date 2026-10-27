#!/usr/bin/env python3
"""
hw_test log analyzer for the Canon 70D.

Parses ML/LOGS/HW_TEST.LOG as written by modules/hw_test/hw_test.c.

Usage:
    ./analyze_hw_test.py [path-to-HW_TEST.LOG]

Exit status is non-zero if any test FAILED, so this can gate a script.
"""

import re
import sys
from pathlib import Path

# hw_test.c writes "[HW] <name>: PASS|FAIL|SKIP" (see rst()/skip() in that file).
RESULT_RE = re.compile(r'\[HW\]\s+(.+?):\s+(PASS|FAIL|SKIP)\b')


def default_log_path():
    """HW_TEST.LOG is written by the camera to ML/LOGS on the card; when the card
    is mounted for analysis it usually sits next to this repo or in a CML copy."""
    here = Path(__file__).resolve().parent
    candidates = [
        here / 'ML' / 'LOGS' / 'HW_TEST.LOG',
        here / 'logs' / 'HW_TEST.LOG',
    ]
    for c in candidates:
        if c.exists():
            return c
    return candidates[0]


def parse_hw_test_log(log_file):
    results = {'pass': [], 'fail': [], 'skip': [], 'reason': {}, 'info': []}
    path = Path(log_file)
    if not path.exists():
        print(f"Error: {log_file} not found", file=sys.stderr)
        return results

    text = path.read_text(encoding='utf-8', errors='ignore')
    # Strip ANSI colour codes that leak in when the log is captured from a terminal.
    text = re.sub(r'\x1b\[[0-9;]*[mK]', '', text)

    for line in text.splitlines():
        m = RESULT_RE.search(line)
        if m:
            name, verdict = m.group(1).strip(), m.group(2)
            results[verdict.lower()].append(name)
            # Anything after the verdict is the reason string.
            tail = line[m.end():].strip(' ()')
            if tail:
                results['reason'][name] = tail
            continue
        if '[HW]' in line or '[FIO]' in line:
            results['info'].append(line.strip())

    return results


def print_summary(results):
    print("\n" + "=" * 60)
    print("HW_TEST Results Summary")
    print("=" * 60)

    print(f"\nPASS: {len(results['pass'])}")
    for name in results['pass']:
        print(f"   - {name}")

    if results['fail']:
        print(f"\nFAIL: {len(results['fail'])}")
        for name in results['fail']:
            reason = results['reason'].get(name)
            print(f"   - {name}" + (f"  ({reason})" if reason else ""))

    if results['skip']:
        print(f"\nSKIP: {len(results['skip'])}")
        for name in results['skip']:
            reason = results['reason'].get(name)
            print(f"   - {name}" + (f"  ({reason})" if reason else ""))

    if results['info']:
        print(f"\nINFO: {len(results['info'])}")
        for line in results['info'][:10]:
            print(f"   {line}")
        if len(results['info']) > 10:
            print(f"   ... and {len(results['info']) - 10} more")

    total = len(results['pass']) + len(results['fail']) + len(results['skip'])
    print("\n" + "=" * 60)
    if total == 0:
        print("No test results found - wrong file, or hw_test never ran.")
    else:
        print(f"Total: {total} tests ({len(results['pass'])} passed, "
              f"{len(results['fail'])} failed, {len(results['skip'])} skipped)")
    print("=" * 60 + "\n")
    return total


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        return 0

    log_file = sys.argv[1] if len(sys.argv) > 1 else default_log_path()
    print(f"Analyzing: {log_file}")

    results = parse_hw_test_log(log_file)
    total = print_summary(results)

    if total == 0:
        return 2
    return 1 if results['fail'] else 0


if __name__ == '__main__':
    sys.exit(main())
