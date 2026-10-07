# Stage 2: distance tables and search prototype

Run the C tools from the repository root. These are host programs that reuse the original functions in `solver.c`.

## Source files

| File | Purpose |
| --- | --- |
| [stage2_build_orientation_distances.c](../../stage2_build_orientation_distances.c) | Builds the 729-entry orientation distance table with BFS and validates complete coverage, solved distance 0, and maximum distance 6 before saving. |
| [stage2_build_permutation_distances.c](../../stage2_build_permutation_distances.c) | Builds the 5,040-entry permutation distance table with BFS and validates complete coverage, solved distance 0, and maximum distance 7 before saving. |
| [stage2_verify_heuristic.c](../../stage2_verify_heuristic.c) | Checks `max(permutation distance, orientation distance)` against the original BFS oracle over all 3,674,160 states (H1). |
| [stage2_search_prototype.c](../../stage2_search_prototype.c) | Iterative-deepening search with an explicit stack; this early prototype has the fixed test root `p=0, o=1` and replays its returned path. |

## Tables and records

| File | Purpose |
| --- | --- |
| [orientation-distances.bin](orientation-distances.bin) | 729 unsigned one-byte distances, indexed by orientation rank. |
| [permutation-distances.bin](permutation-distances.bin) | 5,040 unsigned one-byte distances, indexed by permutation rank. |
| [orientation-distance-build.txt](orientation-distance-build.txt) | Historical orientation BFS coverage and maximum-distance output. |
| [permutation-distance-build.txt](permutation-distance-build.txt) | Historical permutation BFS coverage and maximum-distance output. |
| [heuristic-admissibility-all-states.txt](heuristic-admissibility-all-states.txt) | Full-domain H1 result: 3,674,160 checked, zero violations. |
| [search-prototype-ten-move-case.txt](search-prototype-ten-move-case.txt) | The fixed root's ten-move solution and successful path replay. |

The `.txt` records retain their original contents. The distance-table bytes also remain unchanged after renaming. The current builders additionally fail instead of saving if their coverage or maximum-distance checks do not match the expected values.

## Build and run

These commands regenerate the two binary tables, then verify the heuristic and run the prototype:

```bash
mkdir -p measurements/stage2
cc -std=c99 -O2 stage2_build_orientation_distances.c -o /tmp/build-orientation-distances
cc -std=c99 -O2 stage2_build_permutation_distances.c -o /tmp/build-permutation-distances
cc -std=c99 -O2 stage2_verify_heuristic.c -o /tmp/verify-heuristic
cc -std=c99 -O2 stage2_search_prototype.c -o /tmp/search-prototype

/tmp/build-orientation-distances
/tmp/build-permutation-distances
/tmp/verify-heuristic
/tmp/search-prototype
```

The prototype is an early fixed-case demonstration. Use the Stage 3 tools for arbitrary valid input strings and full-domain search verification.
