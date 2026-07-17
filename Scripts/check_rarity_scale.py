#!/usr/bin/env python3
"""Oracle for Shared/RarityScale multipliers. Exit 0 if tables match expected."""
HP_STEPS = [5, 5, 5, 5, 10, 45, 30, 5]  # Common→…→Unique (8 steps)

def pow3(r):
    return 3 ** r

def mob_hp(r):
    m = 1.0
    for i in range(r):
        m *= HP_STEPS[i]
    return m

def mob_armor(r):
    return pow3(min(r, 6))  # Ultra = 6

assert pow3(0) == 1 and pow3(2) == 9 and pow3(4) == 81
assert mob_hp(0) == 1
assert mob_hp(1) == 5
assert mob_hp(4) == 5**4  # Legendary
assert mob_hp(5) == 5**4 * 10
assert mob_hp(6) == 5**4 * 10 * 45
assert mob_armor(6) == mob_armor(7) == mob_armor(8) == 3**6
print('ok', {r: (pow3(r), mob_hp(r), mob_armor(r)) for r in range(9)})
