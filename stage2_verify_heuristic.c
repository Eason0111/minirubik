#define main baseline_main
#include "solver.c"
#undef main

static int read_table(const char *path, uint8_t *data, size_t size)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
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
    uint8_t pd[PERMUTATIONS], od[ORIENTATIONS];

    if (!read_table("measurements/stage2/permutation-distances.bin",
                    pd, sizeof pd) ||
        !read_table("measurements/stage2/orientation-distances.bin",
                    od, sizeof od)) {
        fputs("Could not read distance tables\n", stderr);
        return 1;
    }

    uint8_t diameter;
    uint8_t *baseline = build_table(&diameter);
    if (!baseline || diameter != 11) {
        fputs("Baseline construction failed\n", stderr);
        free(baseline);
        return 1;
    }

    unsigned violations = 0;
    unsigned checked = 0;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unsigned hp = pd[rank / ORIENTATIONS];
        unsigned ho = od[rank % ORIENTATIONS];
        unsigned h = hp > ho ? hp : ho;

        state_t state;
        unrank_state(rank, &state);
        uint32_t current = rank;
        unsigned distance = 0;

        while (current != 0) {
            uint8_t move = baseline[current];
            if (move >= MOVES || distance >= diameter) {
                fprintf(stderr, "Invalid baseline path at rank %u\n",
                        (unsigned)rank);
                free(baseline);
                return 1;
            }

            state = apply_move(state, move);
            current = rank_state(&state);
            ++distance;
        }

        if (h > distance) {
            if (violations == 0)
                printf("First violation: rank=%u, h=%u, distance=%u\n",
                       (unsigned)rank, h, distance);
            ++violations;
        }
        ++checked;
    }

    free(baseline);
    printf("Checked=%u, violations=%u\n", checked, violations);
    puts(violations == 0 ? "H1 PASS" : "H1 FAIL");
    return violations != 0;
}
