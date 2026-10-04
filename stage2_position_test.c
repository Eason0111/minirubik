/* 重用原始函式，將原本的 main 改名以避免衝突。 */
#define main baseline_main
#include "solver.c"
#undef main

int main(void)
{
    state_t state;
    unrank_state(0, &state);

    uint32_t rank = rank_state(&state);
    printf("Solved-state rank: %u\n", (unsigned)rank);
    uint8_t dist[PERMUTATIONS];
    uint16_t queue[PERMUTATIONS];
    unsigned head = 0, tail = 0;

    memset(dist, UINT8_MAX, sizeof dist);
    dist[0] = 0;
    queue[tail++] = 0;

    printf("dist[0]=%u, dist[1]=%u, pending=%u\n",
           (unsigned)dist[0], (unsigned)dist[1], tail - head);
    while (head < tail) {
    uint16_t here = queue[head++];
    unrank_state((uint32_t)here * ORIENTATIONS, &state);

    for (uint8_t move = 0; move < MOVES; ++move) {
        state_t next = apply_move(state, move);
        uint16_t there =
            (uint16_t)(rank_state(&next) / ORIENTATIONS);
        if (dist[there] == UINT8_MAX) {
            dist[there] = dist[here] + 1;
            queue[tail++] = there;
            }
    	}
    }
        printf("Discovered=%u, pending=%u\n", tail, tail - head);
    unsigned missing = 0;
    unsigned max_distance = 0;

    for (unsigned i = 0; i < PERMUTATIONS; ++i) {
        if (dist[i] == UINT8_MAX) {
            ++missing;
        } else if (dist[i] > max_distance) {
            max_distance = dist[i];
        }
    }

    printf("After BFS: dist[0]=%u, missing=%u, max=%u\n",
           (unsigned)dist[0], missing, max_distance);
    FILE *out = fopen("measurements/stage2/position-dist.bin", "wb");
    if (out == NULL) {
        perror("open position distances");
        return 1;
    }

    size_t written = fwrite(dist, sizeof dist[0], PERMUTATIONS, out);
    int close_result = fclose(out);
    if (written != PERMUTATIONS || close_result != 0) {
        fputs("Failed to save position distances\n", stderr);
        return 1;
    }

    printf("Saved %zu position distances\n", written);
    return rank == 0 ? 0 : 1;
}
