# Rogue10m combat and weapon switching validation

Implementation baseline: e32e5abe672dd9554b3f51e90b672f6039077d29. Built with UE5.8.3 CL58210709, Win64 Development Editor, MSVC14.44 and Windows SDK10.0.22621.

| Suite | Passed | Errors | Test warnings |
| --- | ---: | ---: | ---: |
| AttackResources | 4 | 0 | 0 |
| HybridSkillTree | 3 | 0 | 0 |
| ProgressionRequirements | 4 | 0 | 0 |
| WeaponCooldown | 5 | 0 | 0 |
| WeaponSwitch | 8 | 0 | 0 |
| Total | 24 | 0 | 0 |

The changes aggregate duplicate resource costs before payment, share policy-neutral RequiredAll evaluation, preserve shared attack cooldown deadlines across weapon changes, and reject inventory weapon mutations during attack/death before writing equipment state. Different item instances of the same weapon type receive the swap cooldown floor; duplicate instance requests do not reapply effects. Inventory callback reentry cannot accept attacks or nested equipment changes.

Tests run through actual Character, Inventory, Combat and GAS code in isolated NullRHI worlds. They cover failure atomicity, attack timing boundaries, idle/walk/sprint eligibility, duplicate requests, and cooldown preservation. The original project's98 save files were compared before and after using SHA256; no differences were found. Save contents or hashes are not published.

The final Editor build succeeded. Earlier local attempts included a shadowed member-name compile failure and fixture warnings; both were corrected before the24-test result. Raw local logs, engine reports, machine/account identifiers, and failed-run transcripts remain local and are intentionally excluded from this publication branch. Older feature documents may refer to those local evidence paths.

This is not a rendered playtest. Physical key input, hit presentation, swap animations, full user-profile loading and menu pause behavior were not validated here. Four active slots per weapon and remembered presets are still pending. Shared attack cooldown support does not implement independent cooldowns for every skill. No save migration, production-map change, new multiplayer, main merge or PR is included.
