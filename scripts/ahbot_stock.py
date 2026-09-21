#!/usr/bin/env python3
"""Print an AH listing-multiplier value; never modify config or contact a server."""
import argparse
import csv
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config/ahbot/tbc-cut-gems.tsv"
KEY = "AuctionHouseBot.ListProportion.ListMultipliedItemIDs"


def tbc_cut_gem_ids():
    with CATALOG.open() as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    ids = [int(row["entry"]) for row in rows]
    if not ids or len(ids) != len(set(ids)) or any(item <= 0 for item in ids):
        raise ValueError("Invalid or duplicate gem catalog IDs")
    return ids


def merge_tbc_gem_multipliers(config, multiplier):
    """Replace only curated gem entries, preserving all unrelated multipliers.

    Zero removes this profile's entries. A blank env setting skips this helper
    entirely. The module still applies its normal eligibility filters and random
    selection; these are batch sizes after selection, not guaranteed stock levels.
    """
    if not 0 <= multiplier <= 20:
        raise ValueError("Gem multiplier must be between 0 and 20")
    matches = re.findall(r"^\s*" + re.escape(KEY) + r"\s*=([^\n]*)$", config, re.M)
    if len(matches) > 1:
        raise ValueError("Duplicate listing-multiplier configuration keys")
    value = matches[0].split("#", 1)[0].strip().strip('"') if matches else ""
    existing = {}
    if value:
        for token in value.split(","):
            match = re.fullmatch(r"\s*([0-9]+):([0-9]+)\s*", token)
            if not match:
                raise ValueError(f"Invalid listing multiplier: {token!r}")
            item, count = map(int, match.groups())
            if item <= 0 or count <= 0 or item in existing:
                raise ValueError(f"Invalid or duplicate item multiplier: {token!r}")
            existing[item] = count
    for item in tbc_cut_gem_ids():
        if multiplier:
            existing[item] = multiplier
        else:
            existing.pop(item, None)
    return ",".join(f"{item}:{count}" for item, count in existing.items())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--tbc-cut-gem-multiplier", required=True, type=int)
    args = parser.parse_args()
    try:
        print(merge_tbc_gem_multipliers(args.config.read_text(), args.tbc_cut_gem_multiplier))
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
