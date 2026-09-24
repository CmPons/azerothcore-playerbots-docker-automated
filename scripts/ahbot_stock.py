#!/usr/bin/env python3
"""Print an AH listing-multiplier value; never modify config or contact a server."""
import argparse
import csv
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config/ahbot/tbc-cut-gems.tsv"
ARMOR_CATALOG = ROOT / "config/ahbot/level70-plate-shields.tsv"
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
    return merge_item_multipliers(config, dict.fromkeys(tbc_cut_gem_ids(), multiplier))


def level70_armor_multipliers(multiplier):
    """Batch sizes for tradable plate/shields usable at 70; defensive items get 2x."""
    if not 0 <= multiplier <= 10:
        raise ValueError("Armor multiplier must be between 0 and 10 (defensive items get 2x)")
    with ARMOR_CATALOG.open() as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    result = {}
    for row in rows:
        item, priority = int(row["entry"]), int(row["tank_priority"])
        if (item <= 0 or item in result or priority not in (0, 1)
                or int(row["subclass"]) not in (4, 6)
                or int(row["Quality"]) not in (2, 3, 4)
                or not 65 <= int(row["RequiredLevel"]) <= 70
                or not 1 <= int(row["ItemLevel"]) <= 164):
            raise ValueError("Invalid or duplicate armor catalog entry")
        result[item] = multiplier * (1 + priority)
    if not result:
        raise ValueError("Empty armor catalog")
    return result


def merge_item_multipliers(config, replacements):
    """Merge only named IDs. Zero removes an entry; unrelated values are preserved."""
    if any(item <= 0 or not 0 <= count <= 20 for item, count in replacements.items()):
        raise ValueError("Invalid item multiplier replacement")
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
    for item, multiplier in replacements.items():
        if multiplier:
            existing[item] = multiplier
        else:
            existing.pop(item, None)
    return ",".join(f"{item}:{count}" for item, count in existing.items())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--tbc-cut-gem-multiplier", type=int)
    parser.add_argument("--level70-armor-multiplier", type=int)
    args = parser.parse_args()
    if args.tbc_cut_gem_multiplier is None and args.level70_armor_multiplier is None:
        parser.error("At least one stock profile is required")
    try:
        config = args.config.read_text()
        if args.tbc_cut_gem_multiplier is not None:
            value = merge_tbc_gem_multipliers(config, args.tbc_cut_gem_multiplier)
            config = f"{KEY} = {value}\n"
        if args.level70_armor_multiplier is not None:
            value = merge_item_multipliers(config, level70_armor_multipliers(args.level70_armor_multiplier))
        print(value)
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
