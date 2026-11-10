#!/usr/bin/env python3
"""Headless boot trace of the Heroes Lore 3 reference runtime (S03)."""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hl3rt.runtime import Runtime  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("--msgs", action="store_true", help="log every objc message")
    args = ap.parse_args()
    rt = Runtime(trace=args.trace)
    rt.m.msg_trace = args.msgs
    try:
        rt.boot_to_app_main()
        rt.run_launch()
    finally:
        print(json.dumps(rt.report()["unimplemented"], indent=1))
        print("stop:", rt.m.stop_reason, "pending:", rt.pending_app_main)


if __name__ == "__main__":
    main()
