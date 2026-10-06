# Stage 1: Ripes memory and simulation speed

Measurements recorded on 2026-10-03 with Ripes `v2.2.6-106-g5b8a616` on Windows. This folder consolidates the original records; the measurements have not been rerun.

| File | Purpose |
| --- | --- |
| [memory-small-buffer.s](memory-small-buffer.s) | Writes a 1 KiB guest buffer, then holds for host-memory sampling. |
| [memory-large-buffer.s](memory-large-buffer.s) | Writes a 1 MiB guest buffer, then holds for host-memory sampling. |
| [measure-memory.ps1](measure-memory.ps1) | Runs either memory case and samples the Ripes process after the writes finish. |
| [memory-samples.csv](memory-samples.csv) | All 158 original samples from three small-buffer runs and three large-buffer runs. |
| [simulation-speed.s](simulation-speed.s) | Writes a 1 MiB buffer and exits; used for simulation-speed measurements. |
| [measurement-results.json](measurement-results.json) | All six original memory summaries and both original speed reports. |

## Memory records

The script measures the Windows Ripes process's Private Bytes. Each reported value is the mean of the last five samples taken after the guest write loop finishes. The two programs deliberately remain in a hold loop; their 8,000 ms timeout is expected.

| Run pair | Small buffer, guest bytes | Small buffer, mean Private Bytes | Large buffer, guest bytes | Large buffer, mean Private Bytes |
| --- | ---: | ---: | ---: | ---: |
| 1 | 1,024 | 4,804,608 | 1,048,576 | 92,549,120 |
| 2 | 1,024 | 4,788,224 | 1,048,576 | 92,446,720 |
| 3 | 1,024 | 4,784,128 | 1,048,576 | 92,524,544 |

`memory-samples.csv` preserves every original sample field. Its added `RunId`, `OriginalSamplesFile`, and `SampleIndex` columns identify each sample's source and original order. `small-1` through `small-3` correspond to the original `control` cases; `large-1` through `large-3` correspond to the original `large` cases.

## Speed records

The original reports have an empty ISA-extension list. Time is the host wall-clock model execution time, not the runtime of a physical RISC-V processor.

| Processor | Instructions retired | Execution time, ms |
| --- | ---: | ---: |
| RV32_ISS | 1,048,580 | 401 |
| RV32_5S | 1,048,580 | 7,682 |

## Reproduce

In Windows PowerShell, run the memory sampler from this folder:

```powershell
.\measure-memory.ps1 -Case control
.\measure-memory.ps1 -Case large
```

The script maps `control` to `memory-small-buffer.s` and `large` to `memory-large-buffer.s`. Update its `$ripesExe` path if Ripes is installed elsewhere. New runs produce their own timestamped records; they do not update the consolidated historical results.

For simulation speed:

```powershell
$ripesExe = 'C:\Users\User\Desktop\Ripes-v2.2.6-106-g5b8a616-win-x86_64\Ripes.exe'
& $ripesExe --mode cli --src "$PWD\simulation-speed.s" -t asm --proc RV32_ISS --iret --exectime --runinfo --json
& $ripesExe --mode cli --src "$PWD\simulation-speed.s" -t asm --proc RV32_5S --iret --exectime --runinfo --json
```

## Original record paths

`measurement-results.json` retains the original summary/report objects, including their historical absolute paths and source hashes. Those paths describe the original recording location; use the filenames in the table above for the current files. The original separate records also remain in Git history before this consolidation.
