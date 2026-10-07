# Stage 3: C search refinements and verification

The two C files preserve successive versions of the search. Run them from the repository root so they can load the renamed Stage 2 distance tables.

## Source files

| File | Purpose |
| --- | --- |
| [stage3_search_with_tables.c](../../stage3_search_with_tables.c) | Transition-table search using arithmetic move decoding; accepts a 14-character state, checks coordinate transitions, and replays the solution with the original cubie functions. |
| [stage3_search_with_move_lookup.c](../../stage3_search_with_move_lookup.c) | Later version using move-face and move-turn lookup arrays; supports individual states, sampled verification, and full-domain H3 verification. |

Both accept a state such as `21345671111111`. With no argument, both use the historical ten-move case `12345671111123`.

## Historical records

| File | What it records |
| --- | --- |
| [search-before-delayed-decoding.txt](search-before-delayed-decoding.txt) | Ten-move case before moving cubie decoding past same-face pruning. |
| [search-after-delayed-decoding.txt](search-after-delayed-decoding.txt) | The same case after delaying cubie decoding. |
| [search-with-transition-tables.txt](search-with-transition-tables.txt) | The same case using factored coordinate transition tables. |
| [transition-table-validation.txt](transition-table-validation.txt) | 51,921 coordinate/move checks and the ten-move case. |
| [known-case-validation.txt](known-case-validation.txt) | Original BFS comparison and replay for the solved, ten-move, and eleven-move cases. |
| [search-refactor-validation.txt](search-refactor-validation.txt) | Output comparison between the original single-case program and the host verification refactor. |
| [sample-1024-state-validation.txt](sample-1024-state-validation.txt) | A 1,024-state sample; this is not a substitute for full H3. |
| [full-state-validation-before-move-lookup.txt](full-state-validation-before-move-lookup.txt) | Full H3 before move lookup arrays: 3,674,160 states, zero length mismatches, zero replay failures. |
| [move-lookup-equivalence-validation.txt](move-lookup-equivalence-validation.txt) | Three-case comparison after introducing move lookup arrays. |
| [full-state-validation-after-move-lookup.txt](full-state-validation-after-move-lookup.txt) | Full H3 for the later version: 3,674,160 states, zero length mismatches, zero replay failures. |

These records preserve their original contents, including any old source filenames. Renaming them does not constitute a new measurement. The two full H3 runs have identical solution-distance histograms and search counters; their recorded validation times are 448.571779 s and 408.407421 s respectively. These are separate historical host runs, not target retired-instruction measurements.

## Build and run

```bash
cc -std=c99 -O2 stage3_search_with_tables.c -o /tmp/search-with-tables
cc -std=c99 -O2 stage3_search_with_move_lookup.c -o /tmp/search-with-move-lookup

/tmp/search-with-tables 21345671111111
/tmp/search-with-move-lookup 21345671111111
/tmp/search-with-move-lookup --sample 1024
```

To rerun the full host H3 check:

```bash
/tmp/search-with-move-lookup --h3
```

The later program initializes and checks the nine move mappings, checks all 51,921 coordinate/move transitions, and validates the Stage 2 table lengths and maxima. In batch mode it compares each solution length with the original exhaustive BFS oracle and replays each path using original cubie operations.

The oracle allocation and floating-point wall-clock timing belong to the host verification harness. This C harness is not the final RV32I program; the target program and its retired-instruction measurements are part of Stage 4.
