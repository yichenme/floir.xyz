#!/usr/bin/env python3
"""Grep-based sanity check for the 21-zone MAP_DATA layout in Shared/StaticData.hh.
Parsing the C++ aggregate initializer properly isn't worth it here — assert on
substrings/counts instead, run from the repo root."""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent.parent
data_text = (root / "Shared" / "StaticData.hh").read_text()
defs_text = (root / "Shared" / "StaticDefinitions.hh").read_text()

assert "Tundra" not in data_text, "Tundra biome should have been removed"
assert data_text.count(".difficulty") == 21, "expected 21 zones with a .difficulty field"
for bad in ("kBoulder", "kMassiveBeetle", "kMassiveLadybug"):
    assert bad not in data_text, f"{bad} should not appear in MAP_DATA"
assert "MAX_DIFFICULTY" not in data_text, "MAX_DIFFICULTY belongs in StaticDefinitions.hh, not StaticData.hh"

for biome in ("Garden", "Jungle", "Desert"):
    for band in ("Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic", "Ultra"):
        name = f"{biome} \u00b7 {band}"
        assert name in data_text, f"missing zone name {name!r}"

match = re.search(r"MAX_DIFFICULTY\s*=\s*(\d+)", defs_text)
assert match, "MAX_DIFFICULTY not found in StaticDefinitions.hh"
assert int(match.group(1)) == 6, f"expected MAX_DIFFICULTY = 6, got {match.group(1)}"

print("zone layout grep checks ok")
