/* Target adapter for stage3_search_with_move_lookup.c.
 * Same move order, turn loop, bounds, same-face pruning and stack algorithm.
 * Host counters, file I/O, timing and oracle are excluded from BOTH measurements.
 * Coordinate input avoids division of a combined rank; table bases use pointers.
 * Renderer absent. Output uses the pinned Ripes syscall interface.
 */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#define MOVES 9
typedef struct { uint16_t p, o; uint8_t next_move; } Frame;
static Frame stack[12];
static uint8_t path[11];
extern const uint16_t perm_R[], perm_B[], perm_D[];
extern const uint16_t orient_R[], orient_B[], orient_D[];
extern const uint8_t position_dist[], orientation_dist[], move_face[], move_turns[];
static const uint16_t *const permutation[3] = {perm_R, perm_B, perm_D};
static const uint16_t *const orientation[3] = {orient_R, orient_B, orient_D};
extern const char input_state[];
static void print(const char *s) {
    register const char *a0 __asm__("a0") = s;
    register unsigned a7 __asm__("a7") = 4;
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a7) : : "memory");
}
static void number(unsigned n) {
    register unsigned a0 __asm__("a0") = n;
    register unsigned a7 __asm__("a7") = 1;
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a7) : : "memory");
}
__attribute__((noreturn)) static void finish(void) {
    register unsigned a7 __asm__("a7") = 10;
    __asm__ volatile("ecall" : "+r"(a7) : : "memory");
    for (;;) {}
}
/* Explicit small factors prevent a variable multiply helper on RV32I. */
static unsigned factor(unsigned p, unsigned n) {
    switch (n) {
    case 7: return (p << 3) - p;
    case 6: return (p << 2) + (p << 1);
    case 5: return (p << 2) + p;
    case 4: return p << 2;
    case 3: return (p << 1) + p;
    case 2: return p << 1;
    default: return p;
    }
}
static int parse(unsigned *p, unsigned *o) {
    unsigned mask=0, sum=0;
    *p=0; *o=0;
    for (unsigned i=0; i<7; ++i) {
        unsigned v=(unsigned)(unsigned char)input_state[i]-'1';
        if (v>=7 || (mask & (1u<<v))) return 0;
        mask |= 1u<<v;
    }
    for (unsigned i=0; i<7; ++i) {
        unsigned less=0;
        for (unsigned j=i+1; j<7; ++j) less += input_state[j]<input_state[i];
        *p=factor(*p,7-i)+less;
        unsigned v=(unsigned)(unsigned char)input_state[7+i]-'1';
        if (v>=3) return 0;
        sum+=v;
        if (i<6) *o=(*o<<1)+*o+v;
    }
    while (sum>=3) sum-=3;
    return sum==0 && input_state[14]==0;
}
static int solve(unsigned input_p, unsigned input_o)
{
    unsigned depth = 0;
    stack[0].p = (uint16_t)(input_p);
    stack[0].o = (uint16_t)(input_o);
    stack[0].next_move = 0;
    unsigned hp = position_dist[stack[0].p];
    unsigned ho = orientation_dist[stack[0].o];
    unsigned h = hp > ho ? hp : ho;
    unsigned bound = h;
    if (bound > 11)
        return -1;



    print("Trying limit: "); number(bound); print("\n");
    while (1) {
        if (stack[depth].p == 0 && stack[depth].o == 0)
            return (int)depth;
        if (depth >= bound || stack[depth].next_move >= MOVES) {
            if (depth == 0) {
                if (bound >= 11)
                    return -1;
                ++bound;
                print("Trying limit: "); number(bound); print("\n");
                stack[0].next_move = 0;
                continue;
            }
            --depth;
            continue;
        }
        uint8_t move = stack[depth].next_move++;
        uint8_t face = move_face[move];

        if (depth > 0 && face == move_face[path[depth - 1]])
            continue;
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



/* Independent cubie replay: no ranked transition or move lookup tables. */
static int replay_cubies(int length) {
    static const uint8_t from[3][7] = {
        {1,4,2,0,3,5,6}, {0,1,2,4,5,6,3}, {0,2,5,3,1,4,6}
    };
    static const uint8_t twist[3][7] = {
        {1,2,0,2,1,0,0}, {0,0,0,1,2,1,2}, {0,0,0,0,0,0,0}
    };
    uint8_t current[14], next[14];
    if (length<0 || length>11) return 0;
    for (unsigned i=0;i<14;++i) current[i]=(uint8_t)(input_state[i]-'1');
    for (int step=0;step<length;++step) {
        unsigned move=path[step], face=0;
        if (move>=9) return 0;
        while (move>=3) { move-=3; ++face; }
        for (unsigned turn=0;turn<=move;++turn) {
            for (unsigned i=0;i<7;++i) {
                unsigned j=from[face][i];
                next[i]=current[j];
                unsigned o=current[7+j]+twist[face][i];
                if (o>=3) o-=3;
                next[7+i]=(uint8_t)o;
            }
            for (unsigned i=0;i<14;++i) current[i]=next[i];
        }
    }
    for (unsigned i=0;i<7;++i)
        if (current[i]!=i || current[7+i]!=0) return 0;
    return 1;
}
__attribute__((noreturn)) static void replay_failure(void) {
    print("Target replay: FAIL\n");
    register unsigned a0 __asm__("a0")=1;
    register unsigned a7 __asm__("a7")=93;
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a7) : : "memory");
    for (;;) {}
}

void main(void) {
    unsigned p,o;
    if (!parse(&p,&o)) { print("Invalid cube input\n"); finish(); }
    int length=solve(p,o);
    if (length<0) { print("Search failed\n"); finish(); }
    if (!replay_cubies(length)) replay_failure();
    print("Target replay: PASS\n");
    static const char names[9][4]={"R  ","R2 ","R' ","B  ","B2 ","B' ","D  ","D2 ","D' "};
    print("Solved at depth "); number((unsigned)length); print("\nMoves: ");
    for (int i=0;i<length;++i) print(names[path[i]]);
    print("\n"); finish();
}
