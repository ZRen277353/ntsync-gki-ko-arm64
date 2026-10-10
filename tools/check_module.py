#!/usr/bin/env python3
"""Reject module artifacts that contain unsafe timer or workqueue references."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile


FORBIDDEN_SYMBOLS = {
    "cancel_delayed_work",
    "cancel_delayed_work_sync",
    "delayed_work_timer_fn",
    "flush_delayed_work",
    "init_timer_key",
    "queue_delayed_work_on",
    "schedule_delayed_work_on",
    "system_wq",
}


def undefined_symbols(path: Path) -> set[str]:
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            raise ValueError(f"{path}: missing .symtab")

        return {
            symbol.name
            for symbol in symtab.iter_symbols()
            if symbol["st_shndx"] == "SHN_UNDEF" and symbol.name
        }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("modules", nargs="+", type=Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    failed = False

    for module in args.modules:
        try:
            bad = sorted(undefined_symbols(module) & FORBIDDEN_SYMBOLS)
        except (OSError, ValueError) as error:
            print(f"error: {error}", file=sys.stderr)
            failed = True
            continue

        if bad:
            print(f"error: {module} references forbidden symbols: {', '.join(bad)}", file=sys.stderr)
            failed = True
        else:
            print(f"ok: {module} has no module-owned timer/workqueue references")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
