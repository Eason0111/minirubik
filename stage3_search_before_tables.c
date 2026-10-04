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
int main(void)
{
    unsigned depth = 0;
    unsigned long long attempted = 0;
    unsigned long long generated = 0;
    unsigned long long heuristic_pruned = 0;
    if (!load_table("measurements/stage2/position-dist.bin",
                    position_dist, sizeof position_dist) ||
        !load_table("measurements/stage2/orientation-dist.bin",
                    orientation_dist, sizeof orientation_dist)) {
        fputs("Could not load distance tables\n", stderr);
        return 1;
    }

    printf("Tables loaded: position=%zu, orientation=%zu bytes\n",
           sizeof position_dist, sizeof orientation_dist);
    /* 先用已解狀態測試根節點。 */
    stack[0].p = 0;
    stack[0].o = 1;
    stack[0].next_move = 0;
    unsigned hp = position_dist[stack[0].p];
    unsigned ho = orientation_dist[stack[0].o];
    unsigned h = hp > ho ? hp : ho;
    unsigned bound = h;

    printf("Position bound=%u, orientation bound=%u\n", hp, ho);
    printf("Combined bound=%u, initial search limit=%u\n", h, bound);
    printf("Root: p=%u, o=%u, next_move=%u\n",
           (unsigned)stack[depth].p,
           (unsigned)stack[depth].o,
           (unsigned)stack[depth].next_move);

    printf("Stack=%zu bytes, path=%zu bytes\n",
           sizeof stack, sizeof path);
        while (1) {
        if (stack[depth].p == 0 && stack[depth].o == 0) {
            printf("Solved at depth %u\n", depth);
            printf("Moves:");
            for (unsigned i = 0; i < depth; ++i) {
                printf(" %s", move_names[path[i]]);
            }
            putchar('\n');

            state_t replay;
            uint32_t start_rank =
                (uint32_t)stack[0].p * ORIENTATIONS + stack[0].o;
            unrank_state(start_rank, &replay);

            for (unsigned i = 0; i < depth; ++i) {
                replay = apply_move(replay, path[i]);
            }

            uint32_t final_rank = rank_state(&replay);
            printf("Replay final rank=%u: %s\n",
                   (unsigned)final_rank,
                   final_rank == 0 ? "PASS" : "FAIL");

            if (final_rank != 0)
                return 1;
	    printf("Attempted moves: %llu\n", attempted);
            printf("Generated children: %llu\n", generated);
            printf("Heuristic-pruned children: %llu\n", heuristic_pruned);
            break;
        }

        if (depth >= bound || stack[depth].next_move >= MOVES) {
                        if (depth == 0) {
                printf("No solution within %u moves\n", bound);

                if (bound >= 11) {
                    fputs("Search failed through limit 11\n", stderr);
                    return 1;
                }

                ++bound;
                stack[0].next_move = 0;
                printf("Trying limit %u\n", bound);
                continue;
            }
            --depth;
            continue;
        }
        ++attempted;
    uint8_t move = stack[depth].next_move++;

    if (depth > 0 && move / 3 == path[depth - 1] / 3) {
        continue;
    }

    /* 確定需要產生子狀態後，才解碼。 */
    ++generated;

    state_t current;
    uint32_t current_rank =
        (uint32_t)stack[depth].p * ORIENTATIONS + stack[depth].o;
    unrank_state(current_rank, &current);

    state_t next = apply_move(current, move);
    uint32_t next_rank = rank_state(&next);

    uint16_t next_p = (uint16_t)(next_rank / ORIENTATIONS);
    uint16_t next_o = (uint16_t)(next_rank % ORIENTATIONS);

    unsigned next_hp = position_dist[next_p];
    unsigned next_ho = orientation_dist[next_o];
    unsigned next_h = next_hp > next_ho ? next_hp : next_ho;
    unsigned next_depth = depth + 1;


    if (next_depth + next_h > bound) {
      ++heuristic_pruned;
      continue;
    } else {
                if (next_depth >= sizeof stack / sizeof stack[0]) {
            fputs("Search stack capacity exceeded\n", stderr);
            return 1;
        }

        path[depth] = move;

        ++depth;
        stack[depth].p = next_p;
        stack[depth].o = next_o;
        stack[depth].next_move = 0;

    }
    }
    return 0;
}
