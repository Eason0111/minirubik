#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <time.h>
#define main baseline_main
#include "solver.c"
#undef main

typedef struct {
    uint16_t p;
    uint16_t o;
    uint8_t next_move;
} Frame;

static Frame stack[12];
static uint8_t path[11];
static const uint8_t move_face[9] = {
    0, 0, 0, 1, 1, 1, 2, 2, 2
};

static const uint8_t move_turns[9] = {
    1, 2, 3, 1, 2, 3, 1, 2, 3
};
static uint8_t position_dist[5040];
static uint8_t orientation_dist[729];

static int load_table(const char *name, uint8_t *data, size_t size)
{
    FILE *file = fopen(name, "rb");
    if (file == NULL) {
        perror(name);
        return 0;
    }

    size_t count = fread(data, 1, size, file);
    int extra = fgetc(file);
    int ok = count == size && extra == EOF && !ferror(file);
    if (fclose(file) != 0)
        ok = 0;
    return ok;
}
static uint16_t permutation[3][PERMUTATIONS];
static uint16_t orientation[3][ORIENTATIONS];

/* Ubuntu 測試版：搜尋開始前，先建立小型轉移表。 */
static void init_transitions(void)
{
    state_t state;

    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t)rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t)(rank_state(&next) / ORIENTATIONS);
        }
    }

    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t)(rank_state(&next) % ORIENTATIONS);
        }
    }
}
static int check_transitions(void)
{
    unsigned checked = 0;
    for (unsigned move = 0; move < MOVES; ++move) {
        if (move_face[move] != move / 3 ||
            move_turns[move] != move % 3 + 1) {
            fprintf(stderr, "Move mapping FAIL: move=%u\n", move);
            return 0;
        }
    }
    puts("Move mapping checks=9: PASS");
    for (unsigned kind = 0; kind < 2; ++kind) {
        unsigned count = kind == 0 ? PERMUTATIONS : ORIENTATIONS;

        for (unsigned rank = 0; rank < count; ++rank) {
            state_t state;
            uint32_t full_rank =
                kind == 0 ? rank * ORIENTATIONS : rank;
            unrank_state(full_rank, &state);

            for (uint8_t move = 0; move < MOVES; ++move) {
                /* 原始函式算出的參考結果。 */
                state_t next = apply_move(state, move);
                uint32_t encoded = rank_state(&next);
                unsigned expected = kind == 0
                    ? encoded / ORIENTATIONS
                    : encoded % ORIENTATIONS;

                /* 轉移表算出的結果。 */
                unsigned actual = rank;
                unsigned face = move / 3;
                unsigned turns = move % 3 + 1;

                for (unsigned t = 0; t < turns; ++t) {
                    actual = kind == 0
                        ? permutation[face][actual]
                        : orientation[face][actual];
                }

                if (actual != expected) {
                    printf("Transition FAIL: kind=%u rank=%u move=%u\n",
                           kind, rank, (unsigned)move);
                    return 0;
                }
                ++checked;
            }
        }
    }

    printf("Transition checks=%u: PASS\n", checked);
    return 1;
}

/* Host verification only: extracted from stage3_search_with_tables.c without changing
 * move order, pruning, or iterative-deepening bounds. */
typedef struct {
    unsigned long long attempted, generated, heuristic_pruned;
} SearchCounts;

static int solve_rank(uint32_t input_rank, SearchCounts *counts, int verbose)
{
    if (input_rank >= STATES)
        return -1;
    *counts = (SearchCounts){0, 0, 0};
    unsigned depth = 0;
    stack[0].p = (uint16_t)(input_rank / ORIENTATIONS);
    stack[0].o = (uint16_t)(input_rank % ORIENTATIONS);
    stack[0].next_move = 0;
    unsigned hp = position_dist[stack[0].p];
    unsigned ho = orientation_dist[stack[0].o];
    unsigned h = hp > ho ? hp : ho;
    unsigned bound = h;
    if (bound > 11)
        return -1;

    if (verbose) {
        printf("Position bound=%u, orientation bound=%u\n", hp, ho);
        printf("Combined bound=%u, initial search limit=%u\n", h, bound);
        printf("Root: p=%u, o=%u, next_move=%u\n",
               (unsigned)stack[0].p, (unsigned)stack[0].o,
               (unsigned)stack[0].next_move);
        printf("Stack=%zu bytes, path=%zu bytes\n", sizeof stack, sizeof path);
    }

    while (1) {
        if (stack[depth].p == 0 && stack[depth].o == 0)
            return (int)depth;
        if (depth >= bound || stack[depth].next_move >= MOVES) {
            if (depth == 0) {
                if (verbose)
                    printf("No solution within %u moves\n", bound);
                if (bound >= 11)
                    return -1;
                ++bound;
                stack[0].next_move = 0;
                if (verbose)
                    printf("Trying limit %u\n", bound);
                continue;
            }
            --depth;
            continue;
        }
        ++counts->attempted;
        uint8_t move = stack[depth].next_move++;
        uint8_t face = move_face[move];

        if (depth > 0 && face == move_face[path[depth - 1]])
            continue;

        ++counts->generated;
        uint8_t turns = move_turns[move];
        uint16_t next_p = stack[depth].p;
        uint16_t next_o = stack[depth].o;
        for (uint8_t turn = 0; turn < turns; ++turn) {
            next_p = permutation[face][next_p];
            next_o = orientation[face][next_o];
        }
        unsigned next_hp = position_dist[next_p];
        unsigned next_ho = orientation_dist[next_o];
        unsigned next_h = next_hp > next_ho ? next_hp : next_ho;
        unsigned next_depth = depth + 1;
        if (next_depth + next_h > bound) {
            ++counts->heuristic_pruned;
            continue;
        }
        if (next_depth >= sizeof stack / sizeof stack[0])
            return -1;
        path[depth] = move;
        ++depth;
        stack[depth].p = next_p;
        stack[depth].o = next_o;
        stack[depth].next_move = 0;
    }
}

/* Replay via original cubie operations, independently of transition tables. */
static int replay_ok(uint32_t rank, int length)
{
    if (length < 0 || length > 11)
        return 0;
    state_t state;
    unrank_state(rank, &state);
    for (int i = 0; i < length; ++i) {
        if (path[i] >= MOVES)
            return 0;
        state = apply_move(state, path[i]);
    }
    return rank_state(&state) == 0;
}

/* Original exhaustive BFS stores moves, so count its path to obtain distance. */
static int exact_distance(uint32_t rank, const uint8_t *oracle)
{
    state_t state;
    unrank_state(rank, &state);
    int distance = 0;
    while (rank != 0) {
        if (rank >= STATES || oracle[rank] >= MOVES || distance >= 11)
            return -1;
        state = apply_move(state, oracle[rank]);
        rank = rank_state(&state);
        ++distance;
    }
    return distance;
}

static double wall_seconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

static int check_distances(void)
{
    if (position_dist[0] != 0 || orientation_dist[0] != 0)
        return 0;
    unsigned max_p = 0, max_o = 0;
    for (unsigned i = 0; i < PERMUTATIONS; ++i) {
        if (position_dist[i] > 7) return 0;
        if (position_dist[i] > max_p) max_p = position_dist[i];
    }
    for (unsigned i = 0; i < ORIENTATIONS; ++i) {
        if (orientation_dist[i] > 6) return 0;
        if (orientation_dist[i] > max_o) max_o = orientation_dist[i];
    }
    return max_p == 7 && max_o == 6;
}

static int run_batch(uint32_t count, int full)
{
    double begin = wall_seconds();
    uint8_t diameter;
    uint8_t *oracle = build_table(&diameter);
    if (!oracle || diameter != 11) {
        free(oracle);
        fputs("Baseline construction failed\n", stderr);
        return 1;
    }
    double ready = wall_seconds();
    printf("Mode=%s, requested=%u, oracle_seconds=%.3f\n",
           full ? "FULL H3" : "SAMPLE ONLY", (unsigned)count, ready - begin);
    fflush(stdout);
    unsigned histogram[12] = {0};
    unsigned long long attempted = 0, generated = 0, pruned = 0;
    unsigned long long max_generated = 0;
    uint32_t max_rank = 0;
    for (uint32_t i = 0; i < count; ++i) {
        /* Deterministic sample spread across the domain, not just easy ranks. */
        uint32_t rank = full ? i :
            (uint32_t)((uint64_t)i * STATES / count);
        int expected = exact_distance(rank, oracle);
        SearchCounts counts;
        int actual = solve_rank(rank, &counts, 0);
        int replay = replay_ok(rank, actual);
        if (expected < 0 || actual != expected || !replay) {
            fprintf(stderr,
                    "FAIL rank=%u expected=%d actual=%d replay=%d checked=%u\n",
                    (unsigned)rank, expected, actual, replay, (unsigned)i);
            free(oracle);
            return 1;
        }
        ++histogram[expected];
        attempted += counts.attempted;
        generated += counts.generated;
        pruned += counts.heuristic_pruned;
        if (counts.generated > max_generated) {
            max_generated = counts.generated;
            max_rank = rank;
        }
        if ((i + 1) % 10000 == 0) {
            printf("Progress=%u/%u, validation_seconds=%.3f\n",
                   (unsigned)(i + 1), (unsigned)count, wall_seconds() - ready);
            fflush(stdout);
        }
    }
    double end = wall_seconds();
    free(oracle);
    printf("Checked=%u, length_mismatches=0, replay_failures=0\n", (unsigned)count);
    printf("Validation wall seconds=%.6f\n", end - ready);
    printf("Oracle + validation wall seconds=%.6f\n", end - begin);
    printf("Attempted=%llu, generated=%llu, heuristic_pruned=%llu\n",
           attempted, generated, pruned);
    printf("Largest generated count=%llu at rank=%u\n",
           max_generated, (unsigned)max_rank);
    printf("Distance histogram:");
    for (unsigned d = 0; d <= 11; ++d)
        printf(" %u:%u", d, histogram[d]);
    putchar('\n');
    puts(full ? "H3 PASS (full domain, with path replay)" :
                "SAMPLE PASS (not full H3)");
    return 0;
}

int main(int argc, char **argv)
{
    int full = argc == 2 && strcmp(argv[1], "--h3") == 0;
    int sample = argc == 3 && strcmp(argv[1], "--sample") == 0;
    uint32_t count = STATES;
    state_t start;
    if (sample) {
        char *end;
        errno = 0;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || value == 0 || value > STATES) {
            fputs("Sample count must be 1..3674160\n", stderr);
            return 2;
        }
        count = (uint32_t)value;
    } else if (!full) {
        const char *input = argc == 2 ? argv[1] : "12345671111123";
        if (argc > 2 || !parse_state(input, &start)) {
            fputs("Usage: stage3_search_with_move_lookup [STATE | --sample N | --h3]\n", stderr);
            return 2;
        }
    }
    init_transitions();
    if (!check_transitions()) return 1;
    if (!load_table("measurements/stage2/permutation-distances.bin", position_dist,
                    sizeof position_dist) ||
        !load_table("measurements/stage2/orientation-distances.bin", orientation_dist,
                    sizeof orientation_dist) || !check_distances()) {
        fputs("Invalid distance tables\n", stderr);
        return 1;
    }
    printf("Tables loaded: position=%zu, orientation=%zu bytes\n",
           sizeof position_dist, sizeof orientation_dist);
    if (full || sample)
        return run_batch(count, full);

    uint32_t rank = rank_state(&start);
    SearchCounts counts;
    int length = solve_rank(rank, &counts, 1);
    if (length < 0) {
        fputs("Search failed\n", stderr);
        return 1;
    }
    printf("Solved at depth %d\nMoves:", length);
    for (int i = 0; i < length; ++i)
        printf(" %s", move_names[path[i]]);
    putchar('\n');
    int replay = replay_ok(rank, length);
    if (!replay) {
        fputs("Replay FAIL\n", stderr);
        return 1;
    }
    puts("Replay final rank=0: PASS");
    printf("Attempted moves: %llu\n", counts.attempted);
    printf("Generated children: %llu\n", counts.generated);
    printf("Heuristic-pruned children: %llu\n", counts.heuristic_pruned);
    return 0;
}
