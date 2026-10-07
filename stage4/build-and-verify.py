"""Stage 4 build, validation and GCC comparison. Run in WSL from repository root."""
import sys
import textwrap
import argparse
import csv
import hashlib
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RIPES = Path(
    "/mnt/c/Users/User/Desktop/"
    "Ripes-v2.2.6-106-g5b8a616-win-x86_64/Ripes.exe"
)
BUDGET = 50_000_000

# 原始 C 版負責提供最短距離與獨立重播。
HELPER = r'''
#define main baseline_main
#include "solver.c"
#undef main

static int distance_from(uint32_t rank, const uint8_t *oracle)
{
    state_t state;
    unrank_state(rank, &state);
    int length = 0;
    while (rank != 0) {
        if (length >= 11 || oracle[rank] >= MOVES)
            return -1;
        state = apply_move(state, oracle[rank]);
        rank = rank_state(&state);
        ++length;
    }
    return length;
}

int main(int argc, char **argv)
{
    uint8_t diameter = 0;
    uint8_t *oracle = build_table(&diameter);
    if (oracle == NULL || diameter != 11) {
        fputs("Could not build baseline oracle\n", stderr);
        free(oracle);
        return 1;
    }

    if (argc == 2 && strcmp(argv[1], "--list11") == 0) {
        unsigned count = 0;
        for (uint32_t rank = 0; rank < STATES; ++rank) {
            int length = distance_from(rank, oracle);
            if (length < 0) {
                free(oracle);
                return 1;
            }
            if (length != 11)
                continue;
            state_t state;
            unrank_state(rank, &state);
            for (unsigned i = 0; i < 7; ++i)
                putchar('1' + state.p[i]);
            for (unsigned i = 0; i < 7; ++i)
                putchar('1' + state.o[i]);
            putchar('\n');
            ++count;
        }
        free(oracle);
        return count == 2644 ? 0 : 1;
    }

    char line[256];
    while (fgets(line, sizeof line, stdin)) {
        char *input = strtok(line, " \t\r\n");
        state_t state;
        if (input == NULL || !parse_state(input, &state)) {
            fputs("Invalid replay input\n", stderr);
            free(oracle);
            return 1;
        }

        uint32_t rank = rank_state(&state);
        int expected = distance_from(rank, oracle);
        unsigned count = 0;
        char *name;

        while ((name = strtok(NULL, " \t\r\n")) != NULL) {
            unsigned move = 0;
            while (move < MOVES &&
                   strcmp(name, move_names[move]) != 0)
                ++move;
            if (move == MOVES) {
                fputs("Unknown replay move\n", stderr);
                free(oracle);
                return 1;
            }
            state = apply_move(state, (uint8_t)move);
            ++count;
        }

        printf("%u %d %u %u\n", (unsigned)rank, expected,
               (unsigned)rank_state(&state), count);
        fflush(stdout);
    }

    free(oracle);
    return ferror(stdin) ? 1 : 0;
}
'''

def required(pattern, text):
    match = re.search(pattern, text, re.M)
    if match is None:
        raise RuntimeError(f"Missing output field: {pattern}")
    return match.group(1)

def windows_path(path):
    return subprocess.check_output(
        ["wslpath", "-w", str(path.resolve())], text=True
    ).strip()

def test_main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--all11", action="store_true")
    parser.add_argument("--states", nargs="+")
    parser.add_argument("--label", default="run")
    args = parser.parse_args()
    if args.all11 and args.states:
        parser.error("--all11 and --states cannot be combined")
    if not re.fullmatch(r"[a-z0-9-]+", args.label):
        parser.error("--label accepts lowercase letters, digits and hyphens")

    if args.all11:
        prospective = ROOT / f"measurements/stage4/ripes-{args.label}-all11.txt"
        if prospective.exists():
            raise SystemExit("Preserve existing logs or choose a new --label")
    source = ROOT / "stage4/solve-cube-rv32i.s"
    text = source.read_text(encoding="utf-8-sig")
    input_pattern = r'(?m)^[ \t]*input_state:[ \t]*\.asciz[ \t]*"[^"]*"'
    if len(re.findall(input_pattern, text)) != 1:
        raise RuntimeError("Expected exactly one input_state declaration")
    if not RIPES.is_file():
        raise RuntimeError(f"Ripes not found: {RIPES}")

    logdir = ROOT / "measurements/stage4"
    logdir.mkdir(parents=True, exist_ok=True)
    mode = "all11" if args.all11 else (
        "selected-cases" if args.states else "four-cases"
    )
    name = f"ripes-{args.label}-{mode}"
    csv_path = logdir / f"{name}.csv"
    raw_path = logdir / f"{name}.txt"

    with tempfile.TemporaryDirectory(
        prefix=".ripes-check-", dir=ROOT / "stage4"
    ) as temporary:
        temporary = Path(temporary)
        cfile = temporary / "oracle.c"
        helper = temporary / "oracle"
        assembly = temporary / "case.s"
        cfile.write_text(HELPER, encoding="utf-8")

        print("Building original C oracle helper...", flush=True)
        subprocess.run(
            ["cc", "-std=c99", "-O2", "-I", str(ROOT),
             str(cfile), "-o", str(helper)],
            check=True
        )

        if args.all11:
            print("Enumerating all distance-11 states...", flush=True)
            listing = subprocess.run(
                [str(helper), "--list11"],
                capture_output=True, text=True, check=True
            )
            cases = listing.stdout.splitlines()
            if len(cases) != 2644 or len(set(cases)) != 2644:
                raise RuntimeError("Expected 2644 distinct distance-11 states")
        elif args.states:
            cases = list(dict.fromkeys(args.states))
        else:
            cases = [
                "12345671111111",
                "13642571111111",
                "12345671111123",
                "21345671111111",
            ]

        oracle = subprocess.Popen(
            [str(helper)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1
        )
        maximum = 0
        maximum_state = ""
        over_budget = 0
        checked = 0

        try:
            with csv_path.open("w", newline="", encoding="utf-8") as csvfile, \
                 raw_path.open("w", encoding="utf-8") as rawfile:
                writer = csv.writer(csvfile)
                writer.writerow([
                    "state", "rank", "expected_depth", "actual_depth",
                    "instructions_retired", "model_ms",
                    "replay_final_rank", "within_budget"
                ])
                rawfile.write(
                    source.name + " SHA256: " +
                    hashlib.sha256(source.read_bytes()).hexdigest() + "\n"
                )
                rawfile.write(
                    "solver.c SHA256: " +
                    hashlib.sha256((ROOT / "solver.c").read_bytes()).hexdigest()
                    + "\n"
                )

                for index, state in enumerate(cases, 1):
                    changed = re.sub(
                        input_pattern,
                        lambda _: f'input_state: .asciz "{state}"',
                        text
                    )
                    assembly.write_text(changed, encoding="utf-8")

                    result = subprocess.run(
                        [str(RIPES), "--mode", "cli",
                         "--src", windows_path(assembly),
                         "-t", "asm", "--proc", "RV32_ISS",
                         "--iret", "--exectime", "--runinfo",
                         "--timeout", "120000"],
                        capture_output=True, text=True,
                        errors="replace", timeout=150
                    )
                    raw = result.stdout + result.stderr
                    rawfile.write(f"\nCASE {index}: {state}\n{raw}\n")
                    rawfile.flush()

                    # Ripes 字串輸出可能夾帶零字元。
                    output = raw.replace("\0", "")
                    if "Target replay: PASS" not in output:
                        raise RuntimeError("Target replay did not pass: " + output)
                    if result.returncode != 0:
                        raise RuntimeError(output)
                    if "Program exited with code: 0" not in output:
                        raise RuntimeError("Program did not exit successfully")
                    processor = required(
                        r"^processor:[ \t]*([^\r\n]*)", output
                    ).strip()
                    extensions = required(
                        r"^ISA extensions:[ \t]*([^\r\n]*)", output
                    ).strip()
                    if processor != "RV32_ISS" or extensions:
                        raise RuntimeError("Unexpected processor or ISA extensions")

                    depth = int(required(
                        r"^Solved at depth[ \t]+(\d+)", output
                    ))
                    moves = required(
                        r"^Moves:([^\r\n]*)", output
                    ).split()
                    retired = int(required(
                        r"^===== instructions retired[ \t]*\r?\n[ \t]*(\d+)",
                        output
                    ))
                    milliseconds = int(required(
                        r"^===== wall-clock model execution time \(ms\)"
                        r"[ \t]*\r?\n[ \t]*(\d+)",
                        output
                    ))

                    oracle.stdin.write(
                        state + " " + " ".join(moves) + "\n"
                    )
                    oracle.stdin.flush()
                    answer = oracle.stdout.readline()
                    if not answer:
                        raise RuntimeError(oracle.stderr.read())
                    rank, expected, final_rank, replay_count = map(
                        int, answer.split()
                    )

                    if (expected < 0 or depth != expected or
                            replay_count != depth or final_rank != 0):
                        raise RuntimeError(
                            f"{state}: depth={depth}, expected={expected}, "
                            f"replayed={replay_count}, final_rank={final_rank}"
                        )
                    if args.all11 and expected != 11:
                        raise RuntimeError("A listed state is not distance 11")

                    within = retired <= BUDGET
                    over_budget += int(not within)
                    if retired > maximum:
                        maximum = retired
                        maximum_state = state

                    writer.writerow([
                        state, rank, expected, depth, retired,
                        milliseconds, final_rank, within
                    ])
                    csvfile.flush()
                    checked += 1

                    if not args.all11 or index % 25 == 0 or index == len(cases):
                        print(
                            f"[{index}/{len(cases)}] {state}: "
                            f"depth={depth}, replay=PASS, "
                            f"iret={retired:,}, budget={'PASS' if within else 'FAIL'}",
                            flush=True
                        )

                summary = (
                    f"Checked={checked}/{len(cases)}\n"
                    f"Optimal lengths and original-C replay: PASS\n"
                    f"Maximum retired={maximum:,} at {maximum_state}\n"
                    f"Over budget={over_budget}\n"
                )
                rawfile.write("\n" + summary)
                print(summary, end="")
                print(f"Saved: {csv_path.relative_to(ROOT)}")
                print(f"Saved: {raw_path.relative_to(ROOT)}")
        finally:
            oracle.stdin.close()
            try:
                code = oracle.wait(timeout=5)
            except subprocess.TimeoutExpired:
                oracle.kill()
                oracle.wait()
                raise RuntimeError("Oracle helper did not terminate")
            if code != 0:
                raise RuntimeError(oracle.stderr.read())

        if over_budget:
            raise SystemExit("Correctness passed, but some cases exceed the budget")


HERE=Path(__file__).resolve().parent

CASES = ['12345671111111','13642571111111','12345671111123',
         '21345671111111','54721631111111']
FLAGS = ['-O2','-march=rv32i','-mabi=ilp32','-ffreestanding',
         '-fno-builtin','-msmall-data-limit=0','-nostdlib',
         '-Wl,--no-relax,-e,main,-Ttext=0,-Tdata=0x10000000']

def run(args):
    return subprocess.check_output([str(x) for x in args], text=True, stderr=subprocess.STDOUT)

def sizes(elf):
    report=run(['riscv64-unknown-elf-size','-A',elf])
    values={k:int(v) for k,v in re.findall(r'(?m)^(\.\S+)\s+(\d+)\s+\d+\s*$',report)}
    return values['.text'],sum(values.get(k,0) for k in ('.data','.bss','.rodata')),report

def compare_main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--label',default='final')
    args=parser.parse_args()
    if not re.fullmatch(r'[a-z0-9-]+',args.label):
        parser.error('Invalid label')
    logdir=ROOT/'measurements/stage4'
    logdir.mkdir(parents=True,exist_ok=True)
    rawpath=logdir/f'gcc-comparison-{args.label}.txt'
    csvpath=logdir/f'gcc-comparison-{args.label}.csv'
    if rawpath.exists() or csvpath.exists():
        raise SystemExit('Comparison logs already exist; preserve or rename them before rerunning.')
    with tempfile.TemporaryDirectory(prefix='.gcc-compare-',dir=HERE) as tmp:
        tmp=Path(tmp)
        tables_path=tmp/'gcc-reference-tables.s'
        tables_path.write_text(reference_tables(),encoding='utf-8')
        helper=tmp/'oracle'
        (tmp/'oracle.c').write_text(HELPER,encoding='utf-8')
        run(['cc','-O2','-std=c99','-I',ROOT,tmp/'oracle.c','-o',helper])
        with rawpath.open('w',encoding='utf-8') as raw, csvpath.open('w',newline='',encoding='utf-8') as cf:
            raw.write(run(['riscv64-unknown-elf-gcc','--version']))
            raw.write('Flags: '+' '.join(FLAGS)+'\nRenderer absent in both variants.\n')
            raw.write('C adaptation: same search, no host counters; split input coordinates; pointer table bases.\n')
            raw.write('Both variants include input validation, matching output, and independent cubie replay. Renderer absent.\n')
            for f in [HERE/'solve-cube-gcc-reference.c',tables_path,ROOT/'stage4/solve-cube-rv32i.s',ROOT/'stage3_search_with_move_lookup.c',ROOT/'solver.c']:
                raw.write(f'{f.name} SHA256={hashlib.sha256(f.read_bytes()).hexdigest()}\n')
            writer=csv.writer(cf)
            writer.writerow(['state','variant','depth','iret','text_bytes','static_bytes','replay'])
            for state in CASES:
                for variant in ['gcc','assembly']:
                    elf=tmp/f'{variant}.elf'
                    if variant=='gcc':
                        inp=tmp/'input.s'
                        inp.write_text(f'.section .rodata\n.globl input_state\ninput_state: .asciz "{state}"\n')
                        sources=[HERE/'solve-cube-gcc-reference.c',tables_path,inp]
                    else:
                        source=(ROOT/'stage4/solve-cube-rv32i.s').read_text(encoding='utf-8-sig')
                        source,n=re.subn(r'(?m)^\s*input_state:\s*\.asciz\s*"[^"]*"',f'input_state: .asciz "{state}"',source)
                        assert n==1
                        source,n=re.subn(r'(?m)^main:', '.globl main\nmain:',source)
                        assert n==1
                        asm=tmp/'case.s'
                        asm.write_text(source+'\n',encoding='utf-8')
                        sources=[asm]
                    raw.write(run(['riscv64-unknown-elf-gcc',*FLAGS,*sources,'-o',elf]))
                    assert not run(['riscv64-unknown-elf-nm','-u',elf]).strip(), 'Undefined compiler helper'
                    dump=run(['riscv64-unknown-elf-objdump','-d',elf])
                    assert not re.search(r'\b(?:mul\w*|div\w*|rem\w*)\b',dump), 'Forbidden instruction'
                    text,static,size_report=sizes(elf)
                    assert static<=131072
                    result=subprocess.run([str(RIPES),'--mode','cli','--src',windows_path(elf),
                        '-t','elf','--proc','RV32_ISS','--iret','--runinfo','--timeout','120000'],
                        capture_output=True,text=True,errors='replace',timeout=150)
                    output=(result.stdout+result.stderr).replace('\0','')
                    raw.write(f'\n{variant} {state}\n{size_report}\n{output}\n')
                    raw.flush()
                    assert result.returncode==0 and 'Program exited with code: 0' in output, output
                    assert 'Target replay: PASS' in output, output
                    assert required(r'^processor:[ \t]*([^\r\n]*)',output).strip()=='RV32_ISS'
                    assert not required(r'^ISA extensions:[ \t]*([^\r\n]*)',output).strip()
                    depth=int(required(r'^Solved at depth\s+(\d+)',output))
                    moves=required(r'^Moves:([^\r\n]*)',output).split()
                    retired=int(required(r'^===== instructions retired\s*\n\s*(\d+)',output))
                    replay=subprocess.run([str(helper)],input=state+' '+' '.join(moves)+'\n',text=True,capture_output=True,check=True)
                    rank,expected,final,count=map(int,replay.stdout.split())
                    assert depth==expected==count and final==0
                    writer.writerow([state,variant,depth,retired,text,static,'PASS'])
                    cf.flush()
                    print(f'{state} {variant}: depth={depth}, iret={retired:,}, .text={text}, replay=PASS',flush=True)
            raw.write('\nAll 10 runs: optimal length and original-C replay PASS\n')
    print('Saved:',csvpath)
    print('Saved:',rawpath)


def build_led_main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--renderer',choices=['on','off'],default='on')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--delay',type=int,default=100000)
    args=parser.parse_args()
    assert 0<=args.delay<=100000000
    root=ROOT
    source=(root/'stage4/solve-cube-rv32i.s').read_text(encoding='utf-8-sig')
    assert 'replay_cubie_loop:' in source, 'Install target replay first'
    out=args.output or root/'stage4/solve-cube-led-rv32i.s'
    assert out.resolve() != (root/'stage4/solve-cube-rv32i.s').resolve(), 'Keep the measured source intact'

    # Coordinates: +x right, +y up, +z front. Corner 7 is fixed ULF.
    coords=[(1,1,1),(1,-1,1),(-1,-1,1),(1,1,-1),(1,-1,-1),(-1,-1,-1),(-1,1,-1),(-1,1,1)]
    slots=['UFR','DRF','DFL','URB','DBR','DLB','UBL','ULF']
    vectors={'R':(1,0,0),'L':(-1,0,0),'U':(0,1,0),'D':(0,-1,0),'F':(0,0,1),'B':(0,0,-1)}
    rot=[lambda v:(v[0],v[2],-v[1]),lambda v:(-v[1],v[0],v[2]),lambda v:(v[2],v[1],-v[0])]
    maps=[[1,4,2,0,3,5,6,7],[0,1,2,4,5,6,3,7],[0,2,5,3,1,4,6,7]]
    twists=[[1,2,0,2,1,0,0,0],[0,0,0,1,2,1,2,0],[0]*8]
    for f in range(3):
        for dst,src in enumerate(maps[f]):
            if dst==src: continue
            assert rot[f](coords[src])==coords[dst]
            for j in range(3):
                assert rot[f](vectors[slots[src][(j-twists[f][dst])%3]])==vectors[slots[dst][j]]

    origins={'U':(9,0),'L':(0,7),'F':(9,7),'R':(18,7),'B':(27,7),'D':(9,14)}
    colors={'U':0xffffff,'D':0xffff00,'F':0x00cc44,'B':0x0044ff,'R':0xff2222,'L':0xff8800}
    descriptors=[]
    occupied=set()
    for corner,(x,y,z) in enumerate(coords):
        for slot,face in enumerate(slots[corner]):
            col,row={'F':(x,-y),'B':(-x,-y),'R':(-z,-y),'L':(z,-y),'U':(x,z),'D':(x,-z)}[face]
            ox,oy=origins[face]
            px,py=ox+4*((col+1)//2),oy+3*((row+1)//2)
            for yy in range(py,py+3):
                for xx in range(px,px+4):
                    assert 0<=xx<35 and 0<=yy<25 and (xx,yy) not in occupied
                    occupied.add((xx,yy))
            descriptors.append(f'    .word {corner}, {slot}, {px}, {py}')
    assert len(occupied)==288

    renderer=r'''
    .data
    .align 2
    led_dimensions_msg: .asciz "LED Matrix must be 35 x 25\n"
    .align 2
    led_colors:
    COLORS
    led_stickers:
    DESCRIPTORS
    .text
    # Input s0: 14 ASCII bytes (7 cubies, 7 orientations).
    # Preserve all registers used by replay; only caller-saved registers change.
    led_draw:
        addi sp, sp, -32
        sw s2, 0(sp)
        sw s3, 4(sp)
        sw s4, 8(sp)
        sw s5, 12(sp)
        sw s6, 16(sp)
        sw s7, 20(sp)
        sw s8, 24(sp)
        li s4, LED_MATRIX_0_BASE
        li t1, LED_MATRIX_0_WIDTH
        li t2, LED_MATRIX_0_HEIGHT
        li t3, 35
        bne t1, t3, led_bad_dimensions
        li t3, 25
        bne t2, t3, led_bad_dimensions
        slli a6, t1, 2       # byte stride from the exported width
        mv t0, s4
    led_clear_row:
        li t1, LED_MATRIX_0_WIDTH
    led_clear:
        sw zero, 0(t0)
        addi t0, t0, 4
        addi t1, t1, -1
        bne t1, zero, led_clear
        addi t2, t2, -1
        bne t2, zero, led_clear_row
        la s3, led_stickers
        li s2, 24
    led_sticker:
        lw t0, 0(s3)
        lw t2, 4(s3)
        li t1, 7
        beq t0, t1, led_fixed
        add t0, s0, t0
        lbu t1, 0(t0)
        addi t1, t1, -49
        lbu t3, 7(t0)
        addi t3, t3, -49
        sub t2, t2, t3
        bge t2, zero, led_color_index
        addi t2, t2, 3
        j led_color_index
    led_fixed:
        li t1, 7
    led_color_index:
        slli t0, t1, 1
        add t0, t0, t1
        add t0, t0, t2
        slli t0, t0, 2
        la t1, led_colors
        add t0, t0, t1
        lw s5, 0(t0)
        lw s6, 8(s3)        # x
        slli s6, s6, 2
        add s6, s6, s4
        lw t1, 12(s3)       # y
        beq t1, zero, led_address_ready
    led_address_row:
        add s6, s6, a6
        addi t1, t1, -1
        bne t1, zero, led_address_row
    led_address_ready:
        li s7, 3
    led_row:
        mv t0, s6
        li s8, 4
    led_pixel:
        sw s5, 0(t0)
        addi t0, t0, 4
        addi s8, s8, -1
        bne s8, zero, led_pixel
        add s6, s6, a6
        addi s7, s7, -1
        bne s7, zero, led_row
        addi s3, s3, 16
        addi s2, s2, -1
        bne s2, zero, led_sticker
        # Visible delay depends on simulator speed; excluded from CLI benchmark.
        li t0, DELAY
        beq t0, zero, led_restore
    led_delay:
        addi t0, t0, -1
        bne t0, zero, led_delay
    led_restore:
        lw s2, 0(sp)
        lw s3, 4(sp)
        lw s4, 8(sp)
        lw s5, 12(sp)
        lw s6, 16(sp)
        lw s7, 20(sp)
        lw s8, 24(sp)
        addi sp, sp, 32
        ret
    led_bad_dimensions:
        la a0, led_dimensions_msg
        li a7, 4
        ecall
        li a0, 1
        li a7, 93
        ecall
    '''
    renderer=textwrap.dedent(renderer)
    renderer=renderer.replace('COLORS','\n'.join('    .word '+', '.join(hex(colors[f]) for f in corner) for corner in slots))
    renderer=renderer.replace('DESCRIPTORS','\n'.join(descriptors)).replace('DELAY',str(args.delay))
    if args.renderer=='on':
        # Initialize/draw before replaying; update only after a complete HTM move.
        needle='    li   s2, 0\nreplay_move_loop:'
        assert source.count(needle)==1
        source=source.replace(needle,'    jal  ra, led_draw\n'+needle)
        needle='    addi s2, s2, 1\n    j    replay_move_loop'
        assert source.count(needle)==1
        source=source.replace(needle,'    jal  ra, led_draw\n'+needle)
        source+=renderer
    out.write_text(source,encoding='utf-8',newline='\n')
    print('Geometry checks: R/B/D cubie orientation and 24 stickers PASS')
    print(f'Renderer={args.renderer}; saved {out}')
    print('GUI: LED Matrix 0 must be 35 x 25. Load as Source file, then run.')


def reference_tables():
    """Export the ten precomputed arrays already embedded in solve-cube-rv32i.s."""
    source=(ROOT/'stage4/solve-cube-rv32i.s').read_text(encoding='utf-8-sig')
    marker='\n.data\ntrying_msg:'
    if source.count(marker)!=1:
        raise RuntimeError('Unexpected table boundary in solve-cube-rv32i.s')
    tables=source.split(marker,1)[0]
    labels=re.findall(r'(?m)^(\w+):',tables)
    expected=['perm_R','orient_R','perm_B','orient_B','perm_D','orient_D',
              'position_dist','orientation_dist','move_face','move_turns']
    if labels!=expected:
        raise RuntimeError('Unexpected table labels')
    return '\n'.join('.globl '+n for n in labels)+'\n'+tables.replace('.data','.section .rodata',1)+'\n'

def check_results():
    logs=ROOT/'measurements/stage4'
    digest=hashlib.sha256((ROOT/'stage4/solve-cube-rv32i.s').read_bytes()).hexdigest()
    raw=(logs/'ripes-final-distance11-validation.txt').read_text(encoding='utf-8')
    if not any(f'{name} SHA256: {digest}' in raw for name in ['search.s', 'solve-cube-rv32i.s']) or 'Checked=2644/2644' not in raw:
        raise RuntimeError('Final target log is incomplete or source hash differs')
    with (logs/'ripes-final-distance11-validation.csv').open(newline='') as f:
        rows=list(csv.DictReader(f))
    assert len(rows)==2644 and len({r['state'] for r in rows})==2644
    assert all(int(r['expected_depth'])==int(r['actual_depth'])==11
               and int(r['replay_final_rank'])==0 and int(r['instructions_retired'])<=BUDGET for r in rows)
    with (logs/'gcc-vs-assembly-comparison.csv').open(newline='') as f:
        comparisons=list(csv.DictReader(f))
    assert len(comparisons)==10
    assert all(r['replay']=='PASS' and int(r['static_bytes'])<=131072 for r in comparisons)
    gcc_raw=(logs/'gcc-vs-assembly-comparison.txt').read_text(encoding='utf-8')
    for current, historical in [('solve-cube-rv32i.s', 'search.s'), ('solve-cube-gcc-reference.c', 'gcc-reference.c')]:
        sha=hashlib.sha256((ROOT/'stage4'/current).read_bytes()).hexdigest()
        assert any(f'{name} SHA256={sha}' in gcc_raw for name in [current, historical])
    print('Source hashes, 2644 target cases, budget, and 10 GCC comparison records: PASS')
    print('Final maximum:',max(int(r['instructions_retired']) for r in rows))

def check_package():
    with tempfile.TemporaryDirectory(prefix='.package-check-',dir=HERE) as tmp:
        tmp=Path(tmp)
        for mode in ['on','off']:
            dest=tmp/f'{mode}.s'
            sys.argv=[str(__file__),'--renderer',mode,'--output',str(dest)]
            build_led_main()
            expected=HERE/('solve-cube-led-rv32i.s' if mode=='on' else 'solve-cube-rv32i.s')
            assert dest.read_bytes()==expected.read_bytes(), f'{mode} output differs'
        tables=tmp/'tables.s'
        tables.write_text(reference_tables(),encoding='utf-8')
        inp=tmp/'input.s'
        inp.write_text('.section .rodata\n.globl input_state\ninput_state: .asciz "12345671111111"\n')
        elf=tmp/'gcc.elf'
        run(['riscv64-unknown-elf-gcc',*FLAGS,HERE/'solve-cube-gcc-reference.c',tables,inp,'-o',elf])
        assert not run(['riscv64-unknown-elf-nm','-u',elf]).strip()
        text,static,_=sizes(elf)
        assert (text,static)==(1652,40768), (text,static)
        print('GUI/CLI regeneration byte identity and GCC linked size: PASS')
    check_results()

def main():
    commands={'test':test_main,'compare':compare_main,'build-led':build_led_main,
              'check-results':check_results,'check-package':check_package}
    if len(sys.argv)<2 or sys.argv[1] not in commands:
        raise SystemExit('Usage: build-and-verify.py {test|compare|build-led|check-results|check-package} [options]')
    command=sys.argv.pop(1)
    commands[command]()

if __name__=='__main__':
    main()
