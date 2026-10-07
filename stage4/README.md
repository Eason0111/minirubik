# Stage 4: RV32I cube solver

| File | Purpose |
|---|---|
| `search.s` | Measured RV32I solver, input validation, independent cubie replay; renderer absent. |
| `search-led.s` | Generated GUI solver with LED replay. |
| `gcc-reference.c` | Final freestanding C reference with the same search and independent replay. |
| `verify.py` | LED generation, original-C oracle verification, compiler comparison, and result checks. |
| `README.md` | Build instructions and evidence index. |

The ten coordinate/move arrays are embedded in `search.s`. The verification tool extracts these exact arrays for the GCC reference; no separate table source is required. The original `solver.c` and final `stage3_search_with_move_lookup.c` remain at repository root. Host oracle allocation is excluded from the target.

## Requirements

Run Python 3 in Ubuntu/WSL. Install native `cc` and the `riscv64-unknown-elf` GCC/binutils tools. Pinned Ripes: v2.2.6-106-g5b8a616. Set the `RIPES` path near the start of `verify.py` if your executable location differs. Target ISA: RV32I, no M/C.

## Check the existing results and package

From repository root:

```bash
python3 stage4/verify.py check-package
```

This checks source hashes and completed logs, reconstructs identical GUI/CLI assembly, and links the C reference. It does not repeat the full 2644-case simulation.

## Rerun tests

```bash
python3 stage4/verify.py test --label rerun --states 12345671111111 13642571111111 12345671111123 21345671111111 54721631111111
python3 stage4/verify.py test --label rerun-all --all11
python3 stage4/verify.py compare --label rerun
```

Use new labels to preserve existing evidence. Retired counts include parsing, bounds/solution output and independent replay; model is RV32_ISS. GCC is built with `-O2 -march=rv32i -mabi=ilp32`, freestanding flags, no standard runtime, and no relaxation. Code size is linked `.text`; static data is `.data + .bss + .rodata`. Rendering is excluded from both variants.

## GUI and build-time renderer switch

```bash
python3 stage4/verify.py build-led
python3 stage4/verify.py build-led --renderer off --output /tmp/solver-cli.s
```

The off build is byte-identical to `search.s`. On adds only renderer data/code and drawing calls during independent path replay. In Ripes add LED Matrix 0, Width 35, Height 25; load `stage4/search-led.s` as **Source file**. The renderer uses exported BASE/WIDTH/HEIGHT and rejects other dimensions. It shows the input after solving, then redraws after every complete HTM move. `--delay N` controls simulated delay iterations; zero removes the pause.

To test another cube, edit the single `input_state` string in `search.s`, then regenerate the GUI source. Existing logged hashes then refer to the previous input/source and must not be relabeled as a new run.

## Recorded results

| Version | Complete distance-11 cases | Maximum retired | Over 50M |
|---|---:|---:|---:|
| Before caching/unrolling | 2644 | 55,070,278 | 4 |
| Cached/unrolled, before target replay | 2644 | 44,407,335 | 0 |
| Final, with target replay | 2644 | 44,410,589 | 0 |

All complete runs passed optimal-length and original-C replay checks. Maximum state: `54721631111111`. Final named case `21345671111111`: 16,243,918 instructions. Final assembly `.text`: 1348 bytes; static data: 40799 bytes. C reference `.text`: 1652 bytes; static data: 40768 bytes. GCC comparison includes 0/1/10/11-move inputs. Assembly loses on the two shortest cases and wins by about 3.7% on the longer measured cases.

Evidence in `../measurements/stage4/`:
- `ripes-all11.csv/.txt`: original full target run.
- `ripes-register-cache-all11.csv/.txt`: optimized full target run before replay.
- `ripes-final-all11.csv/.txt`: final full target run.
- `gcc-comparison-final.csv/.txt`: final five-input, two-variant compiler comparison.

Raw logs retain original commands, source hashes and historical filenames. Tool consolidation did not change measured solver/C/GUI bytes or convert old logs into new measurements. Local backup tools and intermediate logs are excluded from submission, not deleted.
