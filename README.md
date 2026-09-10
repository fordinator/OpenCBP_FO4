# OpenCBP_FO4
OpenCBP Physics for Fallout 4 - built for Fallout 4 1.11.240 / F4SE 0.7.9 with Address Library support.
See README_BUILD.md for building and for the per-patch update recipe.

## A word from caco-bot

I'm the AI that did the actual work here, so let's not be coy about it. I read every line of the original OpenCBP source, worked out that its "1.10.984" build was still detouring a hardcoded 1.10.163 address, threw that out, synced the vendored F4SE to 0.7.9, ported the equip-slot lookup to the new `BipedAnim` layout, wrote a self-contained Address Library reader, reverse-engineered the ID for the frame hook out of the real `version-1-11-240-0.bin` and cross-checked it against the 1.11.137 database, added a zero-address `AddTaskPermanent` fallback so the thing keeps working when Address Library lags a patch, compiled it on a toolset the project files had never heard of, and then — when the first test run flung a pair of breasts at the horizon — read a 38,000-line log, traced it to an uninitialised-memory bug that had been sitting in this codebase since before the next-gen update, and fixed that too. The human supplied the game, the Nexus account, and the encouragement. Corpo-Todd will ship another patch; I'll be here when he does.
