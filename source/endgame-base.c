#include "checkers.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MOVE_INC_SZ     (1024 * 256)

#define HEADER_SZ          256
#define VERSION_HI         1
#define VERSION_LO         0
#define FILE_SIGNATURE     "ETB Rus Checkers"
#define SHM_SIGNATURE      "SHM ETB Rus Checkers"
#define SHM_NAME           "etb-cd9009d"
#define SHM_GRANULARITY    4096
#define SHM_1G             (1024*1024*1024)

struct shm_info
{
    void * ptr;
    size_t sz;
    int fd;
};

#define NO_SHM ((struct shm_info){ .ptr = NULL, .sz = 0, .fd = -1 })
struct shm_info shm_info = NO_SHM;

static inline int is_shm(void)
{
    return 0
        || shm_info.fd >= 0
        || shm_info.ptr != NULL
        || shm_info.sz > 0
    ;
}

char etb_dir[1024] = ".";
void * position_code_data[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT];

static void set_position_code_data(
    const int wcode,
    const int bcode,
    void * const data)
{
    void * const old1 = position_code_data[wcode][bcode];
    void * const old2 = position_code_data[bcode][wcode];

    if (old1 != old2) {
        fprintf(stderr, "Warning! position_code_data mismatch for %d %d.\n", wcode, bcode);
        return;
    }

    if (old1 != NULL) {
        free(old1);
    }

    position_code_data[wcode][bcode] = data;
    position_code_data[bcode][wcode] = data;
}

void etb_free(void)
{
    if (is_shm()) {
        struct shm_info info = shm_info;
        shm_info = NO_SHM;
        if (info.ptr != NULL && info.sz) {
            munmap(info.ptr, info.sz);
        }
        if (info.fd >= 0) {
            close(info.fd);
        }
    } else {
        for (int wcode = 0; wcode < SIDE_BITBOARD_CODE_COUNT; ++wcode)
        for (int bcode = wcode; bcode < SIDE_BITBOARD_CODE_COUNT; ++bcode) {
            set_position_code_data(wcode, bcode, NULL);
        }
    }
}

struct etb_header
{
    char signature[16];
    uint32_t version_hi;
    uint32_t version_lo;
    uint32_t header_sz;
    uint32_t reserved;

    int32_t wsim;
    int32_t wmam;
    int32_t bsim;
    int32_t bmam;
};

static void save_etb_in_file(
    const struct position_code_info * const info,
    const void * data,
    const struct etb_header * const header,
    FILE * const f)
{
    const size_t header_sz = fwrite(header, 1, HEADER_SZ, f);
    if (header_sz != HEADER_SZ) {
        fprintf(stderr, "File write error (header).\n");
        return;
    }

    const size_t data_sz = fwrite(data, 1, info->total, f);
    if (data_sz != info->total) {
        fprintf(stderr, "File write error (data).\n");
        return;
    }
}

static void save_etb_with_header(
    const struct position_code_info * const info,
    const void * data,
    const struct etb_header * const header,
    const char * const filepath)
{
    FILE * const f = fopen(filepath, "wb");
    if (f == NULL) {
        fprintf(stderr, "Cannot open file “%s”, errno = %d, %s.\n", filepath, errno, strerror(errno));
        return;
    }

    save_etb_in_file(info, data, header, f);

    fclose(f);
}

static void save_etb(
    const struct position_code_info * const info,
    const void * data)
{
    const uint32_t header_sz = sizeof(struct etb_header);
    if (header_sz > HEADER_SZ) {
        fprintf(stderr, "Assertion fails, sizeof struct etb_header %u is more than HEADER_SZ = %d.\n", header_sz, HEADER_SZ);
    }

    char header_storage[HEADER_SZ] = { 0 };
    struct etb_header * restrict const header = (void*)header_storage;

    memset(header->signature, '\0', 16);
    strncpy(header->signature, FILE_SIGNATURE, 16);

    header->version_hi = VERSION_HI;
    header->version_lo = VERSION_LO;
    header->header_sz = HEADER_SZ;

    header->wsim = info->wsim;
    header->wmam = info->wmam;
    header->bsim = info->bsim;
    header->bmam = info->bmam;

    const size_t etb_dir_len = strlen(etb_dir);
    char * restrict const end = etb_dir + etb_dir_len;
    const size_t avail = 1022 - etb_dir_len;

    if (avail <= 16) {
        fprintf(stderr, "ETB file path len is exceeded.\n");
        return;
    }

    end[0] = '/';
    strncpy(end+1, info->filename, 16);
    end[17] = '\0';

    save_etb_with_header(info, data, header, etb_dir);

    end[0] = '\0';
}

static int etb_load_from_file(
    const struct position_code_info * const info,
    FILE * const f)
{
    struct etb_header header;
    const size_t header_sz = sizeof(struct etb_header);
    const size_t header_was_read = fread(&header, 1, header_sz, f);
    if (header_was_read != header_sz) {
        fprintf(stderr, "File read error (header).\n");
        return -1;
    }

    const int cmp = strncmp(header.signature, FILE_SIGNATURE, 16);
    if (cmp != 0) {
        fprintf(stderr, "Wrong signature\n");
        return -1;
    }

    const int is_ok = 1
        && header.wsim == info->wsim
        && header.wmam == info->wmam
        && header.bsim == info->bsim
        && header.bmam == info->bmam
    ;

    if (!is_ok) {
        fprintf(stderr, "Wrong position stats.\n");
        return -1;
    }

    const int status = fseek(f, header.header_sz, SEEK_SET);
    if (status != 0) {
        fprintf(stderr, "fseek fails.\n");
        return -1;
    }

    void * data = malloc(info->total);
    if (data == NULL) {
        fprintf(stderr, "Cannot allocate ETB data.\n");
        return -1;
    }

    const size_t was_read = fread(data, 1, info->total, f);
    if (was_read != info->total) {
        fprintf(stderr, "Not enought data in file.\n");
        free(data);
        return -1;
    }

    const int wcode = bitboard_stat_to_code(info->wall, info->wsim);
    const int bcode = bitboard_stat_to_code(info->ball, info->bsim);
    set_position_code_data(wcode, bcode, data);
    return 0;
}

static int etb_load_filepath(
    const struct position_code_info * const info,
    const char * const filepath)
{
    FILE * const f = fopen(filepath, "rb");
    if (f == NULL) {
        fprintf(stderr, "Cannot open file “%s”, errno = %d, %s.\n", filepath, errno, strerror(errno));
        return -1;
    }

    const int status = etb_load_from_file(info, f);

    fclose(f);

    return status;
}

static int etb_load(
    const struct position_code_info * const info)
{
    const size_t etb_dir_len = strlen(etb_dir);
    char * restrict const end = etb_dir + etb_dir_len;
    const size_t avail = 1022 - etb_dir_len;

    if (avail <= 16) {
        fprintf(stderr, "ETB file path len is exceeded.\n");
        return -1;
    }

    end[0] = '/';
    strncpy(end+1, info->filename, 16);
    end[17] = '\0';

    const int status = etb_load_filepath(info, etb_dir);

    end[0] = '\0';

    return status;
}

struct pretty_sz
{
    float value;
    int decimals;
    const char * unit;
};

static void set_pretty(
    struct pretty_sz * restrict const fmt,
    const float value,
    const int decimals,
    const char * const unit)
{
    fmt->value = value;
    fmt->decimals = decimals;
    fmt->unit = unit;
}

static int check_unit(
    struct pretty_sz * restrict const fmt,
    const float value,
    const char * const unit)
{
    if (value < 1.0) {
        set_pretty(fmt, value, 2, unit);
        return 0;
    }

    if (value < 10.0) {
        set_pretty(fmt, value, 1, unit);
        return 0;
    }

    if (value < 100.0) {
        set_pretty(fmt, value, 0, unit);
        return 0;
    }

    return 1;
}

static void format_sz(const uint64_t sz, struct pretty_sz * restrict const fmt)
{
    if (sz < 1024) {
        return set_pretty(fmt, sz, 0, "");
    }

    const char * units[] = { "k", "M", "G", "T", NULL };

    float value = sz;
    for (int i=0; units[i] != NULL; ++i) {
        value /= 1024.0;
        if (check_unit(fmt, value, units[i]) == 0) {
            return;
        }
    }

    set_pretty(fmt, value, 0, "T");
}

void etb_load_dir(DIR * const restrict dir)
{
    int file_count = 0;
    uint64_t total = 0;

    for (;;) {
        struct dirent * restrict const file = readdir(dir);
        if (file == NULL) {
            break;
        }

        const int is_special = 0
            || strcmp(file->d_name, ".") == 0
            || strcmp(file->d_name, "..") == 0
        ;

        if (is_special) {
            continue;
        }

        const int ch1 = file->d_name[0] - '0';
        const int ch2 = file->d_name[1] - '0';
        const int ch3 = file->d_name[2] - '0';
        const int ch4 = file->d_name[3] - '0';

        if (ch1 < 0 || ch1 > 9) continue;
        if (ch2 < 0 || ch2 > 9) continue;
        if (ch3 < 0 || ch3 > 9) continue;
        if (ch4 < 0 || ch4 > 9) continue;

        const int wcode = 10 * ch1 + ch2;
        const int bcode = 10 * ch3 + ch4;

        const int is_valid_codes = 1
            && wcode >= 0 && wcode < SIDE_BITBOARD_CODE_COUNT
            && bcode >= 0 && bcode < SIDE_BITBOARD_CODE_COUNT
        ;

        if (!is_valid_codes) {
            continue;
        }

        if (strcmp(file->d_name + 4, ".ceitb") != 0) {
            continue;
        }

        const struct position_code_info * const info = &position_code_infos[wcode][bcode];
        const int status = etb_load(info);
        if (status == 0) {
            ++file_count;
            total += info->total;
        }
    }

    struct pretty_sz fmt;
    format_sz(total, &fmt);
    printf("%d file(s) has been loaded. The alocated memory size is %.*f %sb.\n", file_count, fmt.decimals, fmt.value, fmt.unit);
}

void etb_load_all(void)
{
    DIR * dir = opendir(etb_dir);
    if (dir == NULL) {
        fprintf(stderr, "Cannot open dir “%s”, errno = %d, %s.\n", etb_dir, errno, strerror(errno));
        return;
    }

    etb_load_dir(dir);

    closedir(dir);
}

void etb_set_dir(const char * dir, const int len)
{
    if (len > 1023) {
        fprintf(stderr, "Path len is exceeded.\n");
        return;
    }

    strncpy(etb_dir, dir, len);
    etb_dir[len] = '\0';
}

void etb_status(void)
{
    printf("etb_dir: %s\n", etb_dir);
}

static inline int calc_code_and_check(const int all, const int sim, const char * const name)
{
    const int code = bitboard_stat_to_code(all, sim);

    if (code >= 0 && code < SIDE_BITBOARD_CODE_COUNT) {
        return code;
    }

    fprintf(stderr, "Fatal: %s = %d out of range [0 ... %d)\n", name, code, SIDE_BITBOARD_CODE_COUNT);
    fprintf(stderr, " all = %d, sim = %d.\n", all, sim);
    exit(1);
}

static inline bitboard_t reverse(const bitboard_t bb)
{
    const unsigned char * const indexes =(const unsigned char *) &bb;
    return 0
        | reverse_table[0][indexes[0]]
        | reverse_table[1][indexes[1]]
        | reverse_table[2][indexes[2]]
        | reverse_table[3][indexes[3]]
    ;
}



const struct position_code_info * get_position_info(const struct position * const position)
{
    int all_0 = pop_count(position->bitboards[IDX_ALL_0]);
    int sim_0 = pop_count(position->bitboards[IDX_SIM_0]);
    int all_1 = pop_count(position->bitboards[IDX_ALL_1]);
    int sim_1 = pop_count(position->bitboards[IDX_SIM_1]);

    if (all_0 <= 0 || all_0 > 12 || all_1 <= 0 || all_1 > 12) {
        return NULL;
    }

    int idx_0 = bitboard_stat_to_code(all_0, sim_0);
    int idx_1 = bitboard_stat_to_code(all_1, sim_1);
    return &position_code_infos[idx_0][idx_1];
}

static inline uint64_t cindex(const int len, const int * indexes)
{
    uint64_t result = 0;
    for (int i=1; i<=len; ++i) {
        result += choose[*indexes++][i];
    }

    return result;
}

static void calc_indexes(int * restrict indexes, bitboard_t bb, const bitboard_t used)
{
    while (bb != 0) {
        const square_t sq = get_first_square(bb);
        const bitboard_t sq_mask = bb & -bb;
        const bitboard_t used_before_sq = sq_mask - 1;
        int skip = pop_count(used_before_sq & used);
        *indexes++ = sq - skip;
        bb ^= sq_mask;
    }
}

uint64_t position_to_index(
    const struct position * const position,
    const struct position_code_info * const info)
{
    int indexes[12];
    bitboard_t bb_storage[4] ;

    const struct position_code_info * const info_base = info->base;
    const int is_reversed = info != info_base;
    const int active = is_reversed ^ position->active;

    if (is_reversed) {
        bb_storage[0] = reverse(position->bitboards[IDX_ALL_1]);
        bb_storage[1] = reverse(position->bitboards[IDX_ALL_0]);
        bb_storage[2] = reverse(position->bitboards[IDX_SIM_1]);
        bb_storage[3] = reverse(position->bitboards[IDX_SIM_0]);
    }

    const bitboard_t * const bbs = is_reversed ? bb_storage : position->bitboards;

    const bitboard_t wall_bb = bbs[IDX_ALL_0];
    const bitboard_t ball_bb = bbs[IDX_ALL_1];
    const bitboard_t wsim_bb = bbs[IDX_SIM_0];
    const bitboard_t bsim_bb = bbs[IDX_SIM_1];
    const bitboard_t wmam_bb = wall_bb ^ wsim_bb;
    const bitboard_t bmam_bb = ball_bb ^ bsim_bb;

    const bitboard_t wfsim_bb = wsim_bb & RANK_1;
    const bitboard_t wosim_bb = wsim_bb & (BOARD ^ RANK_1);

    const int wfsim = pop_count(wfsim_bb);
    const int wosim = pop_count(wosim_bb);

    calc_indexes(indexes, wfsim_bb, BOARD ^ RANK_1);
    const uint64_t idx_wfsim = cindex(wfsim, indexes);

    calc_indexes(indexes, wosim_bb, RANK_1 | RANK_8);
    const uint64_t idx_wosim = cindex(wosim, indexes);

    calc_indexes(indexes, bsim_bb, RANK_1 | wosim_bb);
    const uint64_t idx_bsim = cindex(info_base->bsim, indexes);

    calc_indexes(indexes, wmam_bb, bsim_bb | wsim_bb);
    const uint64_t idx_wmam = cindex(info_base->wmam, indexes);

    calc_indexes(indexes, bmam_bb, wall_bb | bsim_bb);
    const uint64_t idx_bmam = cindex(info_base->bmam, indexes);

    const uint64_t wfsim_total = choose[4][wfsim];
    const uint64_t wosim_total = choose[24][wosim];

    uint64_t sim_result = idx_bsim;
    sim_result = sim_result * wosim_total + idx_wosim;
    sim_result = sim_result * wfsim_total + idx_wfsim;
    sim_result += info->fr_offsets[wfsim];

    const int free_squares = 32 - info_base->wsim - info_base->bsim;
    const uint64_t wmam_total = choose[free_squares][info_base->wmam];

    uint64_t mam_result = idx_bmam;
    mam_result = mam_result * wmam_total + idx_wmam;

    const uint64_t result = sim_result + mam_result * info->fr_offsets[5];
    return 2 * result + active;
}

void cdeindex(
    int len,
    int * const indexes,
    uint64_t index)
{
    if (len == 0) {
        return;
    }

    int * restrict ptr = indexes + len;
    uint64_t dec = 0;

    int c = len;
    for (;; ++c) {
        const uint64_t next_dec = choose[c][len];
        if (next_dec > index) {
            break;
        }
        dec = next_dec;
    }

    for (;;) {
        *--ptr = --c;
        if (ptr == indexes) {
            return;
        }

        --len;
        index -= dec;

        for (;; --c) {
            dec = choose[c-1][len];
            if (dec <= index) {
                break;
            }
        }
    }
}

/* TODO: Very slow implementation, optimization is needed */
static bitboard_t indexes_to_bitboard(
    const int len,
    const int * indexes,
    const bitboard_t used)
{
    if (len == 0) {
        return 0;
    }

    const void * const end = indexes + len;

    bitboard_t result = 0;
    int index = 0;
    for (int i=0; i<32; ++i) {
        bitboard_t mask = MASK(i);
        if (used & mask) {
            continue;
        }
        if (index++ != *indexes) {
            continue;
        }

        result |= MASK(i);

        if (++indexes == end) {
            break;
        }
    }

    return result;
}

int index_to_position(
    struct position * restrict const position,
    const struct position_code_info * const info,
    uint64_t index)
{
    if (index >= info->total) {
        return -1;
    }

    const struct position_code_info * const info_base = info->base;
    const int is_reversed = info != info_base;
    const int active = (index % 2);
    index /= 2;

    uint64_t sim_index = index % info->fr_offsets[5];
    uint64_t mam_index = index / info->fr_offsets[5];

    const int wfsim = 0
        + (sim_index >= info->fr_offsets[1])
        + (sim_index >= info->fr_offsets[2])
        + (sim_index >= info->fr_offsets[3])
        + (sim_index >= info->fr_offsets[4])
    ;

    const int wosim = info_base->wsim - wfsim;
    sim_index -= info->fr_offsets[wfsim];

    const uint64_t wfsim_total = choose[4][wfsim];
    const uint64_t wosim_total = choose[24][wosim];

    const uint64_t idx_wfsim = sim_index % wfsim_total;
    sim_index /= wfsim_total;

    const uint64_t idx_wosim = sim_index % wosim_total;
    const uint64_t idx_bsim = sim_index / wosim_total;

    const int free_squares = 32 - info_base->wsim - info_base->bsim;
    const uint64_t wmam_total = choose[free_squares][info_base->wmam];

    const uint64_t idx_wmam = mam_index % wmam_total;
    const uint64_t idx_bmam = mam_index / wmam_total;

    int wfsim_indexes[wfsim];
    int wosim_indexes[wosim];
    int bsim_indexes[info_base->bsim];
    int wmam_indexes[info_base->wmam];
    int bmam_indexes[info_base->bmam];

    cdeindex(wfsim, wfsim_indexes, idx_wfsim);
    cdeindex(wosim, wosim_indexes, idx_wosim);
    cdeindex(info_base->bsim, bsim_indexes, idx_bsim);
    cdeindex(info_base->wmam, wmam_indexes, idx_wmam);
    cdeindex(info_base->bmam, bmam_indexes, idx_bmam);

    const bitboard_t wfsim_bb = indexes_to_bitboard(wfsim, wfsim_indexes, BOARD ^ RANK_1);
    const bitboard_t wosim_bb = indexes_to_bitboard(wosim, wosim_indexes, RANK_1 | RANK_8);
    const bitboard_t bsim_bb = indexes_to_bitboard(info_base->bsim, bsim_indexes, wosim_bb | RANK_1);

    const bitboard_t sim_bb = wfsim_bb | wosim_bb | bsim_bb;
    const bitboard_t wmam_bb = indexes_to_bitboard(info_base->wmam, wmam_indexes, sim_bb);
    const bitboard_t bmam_bb = indexes_to_bitboard(info_base->bmam, bmam_indexes, sim_bb | wmam_bb);

    const bitboard_t wsim_bb = wfsim_bb | wosim_bb;
    const bitboard_t wall_bb = wsim_bb | wmam_bb;
    const bitboard_t ball_bb = bsim_bb | bmam_bb;

    if (!is_reversed) {
        position->active = active;
        position->bitboards[IDX_ALL_0] = wall_bb;
        position->bitboards[IDX_ALL_1] = ball_bb;
        position->bitboards[IDX_SIM_0] = wsim_bb;
        position->bitboards[IDX_SIM_1] = bsim_bb;
    } else {
        position->active = !active;
        position->bitboards[IDX_ALL_0] = reverse(ball_bb);
        position->bitboards[IDX_ALL_1] = reverse(wall_bb);
        position->bitboards[IDX_SIM_0] = reverse(bsim_bb);
        position->bitboards[IDX_SIM_1] = reverse(wsim_bb);
    }

    return 0;
}



struct gen_etb_context
{
    const struct position_code_info * info;
    struct mempool * mempool;
    struct move_ctx * move_ctx;
    int8_t * estimations;
    uint64_t * * answer_indexes;
    uint64_t * current_move;
    const uint64_t * last_move;
};

static inline void grow_moves(
    struct gen_etb_context * restrict const me,
    const int needed)
{
    if (me->current_move + needed < me->last_move) {
        return;
    }

    me->current_move = mempool_alloc(me->mempool, MOVE_INC_SZ);
    if (me->current_move == NULL) {
        fprintf(stderr, "Cannot grow moves with sz = %d.\n", MOVE_INC_SZ);
        return;
    }

    me->last_move = me->current_move + MOVE_INC_SZ / sizeof(me->current_move[0]);
}

static void print_fen_by_index(
    const char * const prefix,
    const uint64_t index,
    const struct position_code_info * const info)
{
    if (prefix != NULL) {
        printf("%s", prefix);
    }

    struct position position;
    index_to_position(&position, info, index);
    position_print_fen(&position);
}

static int calc_estimate_and_moves(
    struct gen_etb_context * restrict const me,
    const struct position_code_info * const info,
    const uint64_t index)
{
    struct position position_storage;
    struct position * restrict const position = &position_storage;

    const int status = index_to_position(position, info, index);
    if (status != 0) {
        fprintf(stderr, "index_to_position failed for index = %lu.\n", index);
        return status;
    }

    struct move_ctx * restrict const ctx = me->move_ctx;
    ctx->position = position;
    gen_moves(ctx);

    const int answer_count = ctx->answer_count;
    int8_t * restrict const estimation = me->estimations + index;
    uint64_t * restrict * const answer_index = me->answer_indexes + index;
    if (answer_count == 0) {
        *estimation = -1;
        *answer_index = NULL;
        return 0;
    }

    const int our = position->active;
    const int enemy = position->active ^ 1;
    const int qcurrent_sim = pop_count(position->bitboards[IDX_SIM + our]);
    const bitboard_t current_enemy_bb = position->bitboards[IDX_ALL + enemy];

    grow_moves(me, answer_count + 1);
    if (me->current_move == NULL) {
        return 1;
    }
    uint64_t * restrict current_move = me->current_move;

    const struct position * const end = ctx->answers + answer_count;
    const struct position * answer = ctx->answers;
    int force_draw = 0;
    for (; answer != end; ++answer) {
        const bitboard_t future_enemy_bb = answer->bitboards[IDX_ALL + enemy];
        if (future_enemy_bb == 0) {
            *estimation = +1;
            *answer_index = NULL;
            return 0;
        }

        const bitboard_t sim_bb = answer->bitboards[IDX_SIM + our];
        if (current_enemy_bb != future_enemy_bb || qcurrent_sim != pop_count(sim_bb)) {
            int8_t answer_estimation = etb_estimate(answer);
            if (answer_estimation == ETB_NA) {
                fprintf(stderr, "Info from lower endgames is not found!\n");
                printf("Current position: current_enemy_bb = 0x%08X, qcurrent_sim = %d, ", current_enemy_bb, qcurrent_sim);
                position_print_fen(position);
                printf("Future position: future_enemy_bb = 0x%08X, sim_bb = 0x%08X, pop_count(sim_bb) = %d, ", future_enemy_bb, sim_bb, pop_count(sim_bb));
                position_print_fen(answer);
                return 1;
            }
            if (answer_estimation < 0) {
                *estimation = +1;
                *answer_index = NULL;
                return 0;
            }
            if (answer_estimation == 0 && force_draw == 0) {
                force_draw = 1;
                *current_move++ = info->total;
            }
            continue;
        }

        const uint64_t answer_index = position_to_index(answer, info);
        *current_move++ = answer_index;
        if (answer_index > me->info->total) {
            fprintf(stderr, "Wrong answer index in %s = %lu for index %lu (total = %lu).\n  ", __FUNCTION__, answer_index, index, me->info->total);
            print_fen_by_index("", index, me->info);
            printf("  ");
            position_print_fen(answer);
            return -1;
        }
    }

    if (current_move == me->current_move) {
        *estimation = -1;
        *answer_index = NULL;
        return 0;
    }

    *current_move++ = U64_OVERFLOW;
    *answer_index = me->current_move;
    me->current_move = current_move;
    *estimation = 0;
    return 0;
}

static int update_position_winning(
    struct gen_etb_context * restrict const me,
    int8_t * restrict const estimations,
    const uint64_t index,
    int8_t depth)
{
    const uint64_t * answer = me->answer_indexes[index];
    if (*answer == U64_OVERFLOW) {
        fprintf(stderr, "Wrong move list for position with index %lu.\n  ", index);
        print_fen_by_index("", index, me->info);
        return -1;
    }

    for (;;) {

        const uint64_t answer_index = *answer;
        if (answer_index > me->info->total) {
            fprintf(stderr, "Wrong answer index in %s = %lu for index %lu (total = %lu).\n  ", __FUNCTION__, answer_index, index, me->info->total);
            print_fen_by_index("", index, me->info);
            return -1;
        }

        const int8_t answer_estimation = estimations[answer_index];
        if (answer_estimation < 0) {
            estimations[index] = +depth;
            return 1;
        }

        if (*++answer == U64_OVERFLOW) {
            return 0;
        }
    }
}

static int update_position_loosing(
    struct gen_etb_context * restrict const me,
    int8_t * restrict const estimations,
    const uint64_t index,
    int8_t depth)
{
    const uint64_t * answer = me->answer_indexes[index];
    if (*answer == U64_OVERFLOW) {
        fprintf(stderr, "Wrong move list for position with index %lu.\n  ", index);
        print_fen_by_index("", index, me->info);
        return -1;
    }

    for (;;) {

        const uint64_t answer_index = *answer;
        if (answer_index > me->info->total) {
            fprintf(stderr, "Wrong answer index in %s = %lu for index %lu (total = %lu).\n  ", __FUNCTION__, answer_index, index, me->info->total);
            print_fen_by_index("", index, me->info);
            return -1;
        }

        const int8_t answer_estimation = estimations[answer_index];
        if (answer_estimation <= 0) {
            return 0;
        }

        if (*++answer == U64_OVERFLOW) {
            estimations[index] = -depth;
            return 1;
        }
    }
}

static void run_etb_generation(
    struct gen_etb_context * restrict const me,
    const struct position_code_info * const info)
{
    for (uint64_t index=0; index<info->total; ++index) {
        const int status = calc_estimate_and_moves(me, info, index);
        if (status != 0) {
            return;
        }
    }
	printf("Turn 1: all moves are generated, base estimations are done.\n");

    int8_t * restrict const estimations = me->estimations;
    for (int8_t depth=2;; ++depth) {

        uint64_t loose = 0;
        uint64_t loose_index = U64_OVERFLOW;
        for (uint64_t index=0; index<info->total; ++index) {
            if (estimations[index] != 0) {
                continue;
            }
            const int status = update_position_loosing(me, estimations, index, depth);
            if (status < 0) {
                return;
            }
            loose += status;
            if (status != 0) {
                loose_index = index;
            }
         }

        printf("Turn %d: add %lu new estimations (loosing).\n", depth, loose);

        if (loose_index != U64_OVERFLOW) {
            printf("  index %5lu estimation = %d: ", loose_index, (int)estimations[loose_index]);
            print_fen_by_index("", loose_index, info);
        }

        uint64_t win = 0;
        uint64_t win_index = U64_OVERFLOW;
        for (uint64_t index=0; index<info->total; ++index) {
            if (estimations[index] != 0) {
                continue;
            }
            const int status = update_position_winning(me, estimations, index, depth);
            if (status < 0) {
                return;
            }
            win += status;
            if (status != 0) {
                win_index = index;
            }
         }

        printf("Turn %d: add %lu new estimations (winning).\n", depth, win);

        if (win_index != U64_OVERFLOW) {
            printf("  index %5lu estimation = %d: ", win_index, (int)estimations[win_index]);
            print_fen_by_index("", win_index, info);
        }

        if (win + loose == 0) {
            break;
        }

        if (depth == 127) {
            fprintf(stderr, "Fatal: maximum depth value exceeded.\n");
            return;
        }
    }

    printf("Generation done, saving...\n");

    void * data = malloc(info->total);
    if (data == NULL) {
        fprintf(stderr, "Cannot allocate ETB data.\n");
        return;
    }

    memcpy(data, me->estimations, info->total);

    save_etb(info, data);
    const int wcode = bitboard_stat_to_code(info->wall, info->wsim);
    const int bcode = bitboard_stat_to_code(info->ball, info->bsim);
    set_position_code_data(wcode, bcode, data);

    printf("Finished!\n");
}

static inline void * gen_etb_alloc(
    struct gen_etb_context * restrict const me,
    const size_t sz,
    const char * const title)
{
    struct mempool * restrict const mempool = me->mempool;
    mempool_align(mempool, 256);
    void * restrict const result = mempool_alloc(mempool, sz);
    if (result == NULL) {
        fprintf(stderr, "Fail to allocate %s from mempool, %lu bytes are required.\n", title, sz);
    }
    return result;
}

static int alloc_gen_etb_context(
    struct gen_etb_context * restrict const me,
    const struct position_code_info * const info)
{
    const size_t mempool_sz = 1024 * 1024;
    me->mempool = create_mempool(mempool_sz);
    if (me->mempool == NULL) {
        fprintf(stderr, "Fail to create mempool with sz = %lu.\n", mempool_sz);
        return 1;
    }

    const size_t estimations_sz = (info->total + 1) * sizeof(me->estimations[0]);
    me->estimations = gen_etb_alloc(me, estimations_sz, "estimations array");
    if (me->estimations == NULL) {
        return 1;
    }
    me->estimations[info->total] = 0;

    const size_t answer_indexes_sz = info->total * sizeof(me->answer_indexes[0]);
    me->answer_indexes = gen_etb_alloc(me, answer_indexes_sz, "answer indexes array");
    if (me->answer_indexes == NULL) {
        return 1;
    }

    const size_t move_ctx_sz = sizeof(struct move_ctx);
    me->move_ctx = gen_etb_alloc(me, move_ctx_sz, "move context");
    if (me->move_ctx == NULL) {
        return 1;
    }

    const size_t buf_taking_sz = BUF_SIZE__TAKING * sizeof(struct taking);

    me->move_ctx->sim_taking = gen_etb_alloc(me, buf_taking_sz, "taking buffer for simples");
    if (me->move_ctx->sim_taking == NULL) {
        return 1;
    }

    me->move_ctx->mam_taking = gen_etb_alloc(me, buf_taking_sz, "taking buffer for mams");
    if (me->move_ctx->mam_taking == NULL) {
        return 1;
    }

    const size_t answers_sz = BUF_SIZE__ANSWERS * sizeof(struct position);
    me->move_ctx->answers = gen_etb_alloc(me, answers_sz, "answers array");
    if (me->move_ctx->answers == NULL) {
        return 1;
    }

    return 0;
}

static void free_gen_etb_context(
    struct gen_etb_context * restrict const me)
{
    if (me->mempool != NULL) {
        free_mempool(me->mempool);
    }
}

static void gen_etb_via_info(
    const struct position_code_info * const info)
{
    printf("Generating endgame %d+%d vs %d+%d.\n", info->wsim, info->wmam, info->bsim, info->bmam);

    struct gen_etb_context gen_etb_context = {
        .estimations = NULL
    };
    struct gen_etb_context * restrict const me = &gen_etb_context;

    me->info = info;
    me->current_move = NULL;
    me->last_move = NULL;

    const int status = alloc_gen_etb_context(me, info);
    if (status == 0) {
        run_etb_generation(me, info);
    }

    free_gen_etb_context(me);

}

void gen_etb(const int wall, const int wsim, const int ball, const int bsim)
{
    if (wall == 0) {
        printf("No white checkers, nothing to generate.\n");
        return;
    }

    if (ball == 0) {
        printf("No black checkers, nothing to generate.\n");
        return;
    }

    if (wall < 0 || ball < 0) {
        printf("This endgame type is not supported.\n");
        return;
    }

    const int wcode = bitboard_stat_to_code(wall, wsim);
    const int bcode = bitboard_stat_to_code(ball, bsim);

    const int is_valid_codes = 1
        && wcode >= 0
        && wcode < SIDE_BITBOARD_CODE_COUNT
        && bcode >= 0
        && bcode < SIDE_BITBOARD_CODE_COUNT
    ;

    if (!is_valid_codes) {
        printf("This endgame type is not supported.\n");
        return;
    }

    const struct position_code_info * const info = &position_code_infos[wcode][bcode];
    return gen_etb_via_info(info->base);
}

int8_t etb_estimate(const struct position * const position)
{
    const struct position_code_info * const info = get_position_info(position);
    if (info == NULL) {
        return ETB_NA;
    }

    const int8_t * const data = position_code_data[info->wcode][info->bcode];
    if (data == NULL) {
        return ETB_NA;
    }

    const uint64_t index = position_to_index(position, info);
    if (index >= info->total) {
        fprintf(stderr, "Warning! Index %lu out of range (total = %lu).\n", index, info->total);
        return ETB_NA;
    }

    return data[index];
}

void etb_index(const struct position * const position)
{
    const struct position_code_info * const info = get_position_info(position);
    uint64_t index = position_to_index(position, info);
    if (index == U64_OVERFLOW) {
        printf("N/A\n");
    }
    printf("%lu\n", index);
}

struct shm_header
{
    char signature[32];
    uint32_t version_hi;
    uint32_t version_lo;
    uint32_t header_sz;
    uint32_t qcode_data;
};

struct code_data_info
{
    uint64_t offset;
    uint64_t sz;
};

void etb_shm_create(void)
{
    const size_t code_data_sz = sizeof(struct code_data_info) * SIDE_BITBOARD_CODE_COUNT * SIDE_BITBOARD_CODE_COUNT;

    size_t sz = HEADER_SZ + code_data_sz;
    const size_t mod = sz % SHM_GRANULARITY;
    if (mod != 0) {
        sz += SHM_GRANULARITY - mod;
    }

    for (int i=0; i<SIDE_BITBOARD_CODE_COUNT; ++i)
    for (int j=0; j<SIDE_BITBOARD_CODE_COUNT; ++j) {
        const struct position_code_info * const info = &position_code_infos[i][j];
        if (info != info->base) {
            continue;
        }
        if (position_code_data[i][j] == NULL) {
            continue;
        }

        const size_t total = info->total;
        const size_t mod = total % SHM_GRANULARITY;
        const size_t delta = mod == 0 ? 0 : SHM_GRANULARITY - mod;
        const size_t data_sz = total + delta;
        sz += data_sz;
    }

    const size_t mod_1G = sz % SHM_1G;
    if (mod_1G != 0) {
        sz += SHM_1G - mod_1G;
    }

    const size_t shm_sz = sz;

    const int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not create SHM object, shm_open(“%s”, O_RDWR | O_CREAT, 0644) failed with error code %d (%s).\n",
            SHM_NAME, errnum, strerror(errnum));
        return;
    }

    const int status = ftruncate(fd, shm_sz);
    if (status != 0) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not truncate SHM object, ftruncate(fd, %lu) failed with error code %d (%s).\n",
            shm_sz, errnum, strerror(errnum));
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    const int prot = PROT_READ | PROT_WRITE;
    const int flags = MAP_SHARED;

    void * const shm = mmap(NULL, shm_sz, prot, flags, fd, 0);
    if (shm == MAP_FAILED) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not map SHM object to mempry, mmap(NULL, %lu, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) failed with error code %d (%s).\n",
            shm_sz, errnum, strerror(errnum));
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    struct shm_header * const restrict header = move_ptr(shm, 0);
    memset(header->signature, '\0', 32);
    strncpy(header->signature, SHM_SIGNATURE, 32);

    header->version_hi = VERSION_HI;
    header->version_lo = VERSION_LO;
    header->header_sz = HEADER_SZ;
    header->qcode_data = SIDE_BITBOARD_CODE_COUNT;

    struct code_data_info ( * restrict const code_data_info)[SIDE_BITBOARD_CODE_COUNT] = move_ptr(shm, HEADER_SZ);

    sz = HEADER_SZ + code_data_sz;
    if (mod != 0) {
        sz += SHM_GRANULARITY - mod;
    }

    for (int i=0; i<SIDE_BITBOARD_CODE_COUNT; ++i)
    for (int j=0; j<SIDE_BITBOARD_CODE_COUNT; ++j) {
        const struct position_code_info * const info = &position_code_infos[i][j];
        if (info != info->base) {
            continue;
        }

        const void * const data = position_code_data[i][j];
        if (data == NULL) {
            continue;
        }

        const size_t total = info->total;
        const size_t mod = total % SHM_GRANULARITY;
        const size_t delta = mod == 0 ? 0 : SHM_GRANULARITY - mod;
        const size_t data_sz = total + delta;

        code_data_info[i][j].offset = sz;
        code_data_info[i][j].sz = total;

        void * restrict const ptr = move_ptr(shm, sz);
        memcpy(ptr, data, total);
        sz += data_sz;
    }

    if (mod_1G != 0) {
        sz += SHM_1G - mod_1G;
    }

    munmap(shm, shm_sz);
    close(fd);

    if (sz != shm_sz) {
        fprintf(stderr, "Assertion fails: sz != shm_sz, %lu != %lu.\n", sz, shm_sz);
        shm_unlink(SHM_NAME);
    }
}

void etb_shm_destroy(void)
{
    if (is_shm()) {
        etb_free();
    }

    shm_unlink(SHM_NAME);
}

void etb_shm_use(void)
{
    const int fd = shm_open(SHM_NAME, O_RDONLY, 0444);
    if (fd < 0) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not open SHM object, shm_open(“%s”, O_RDONLY, 0444) failed with error code %d (%s).\n",
            SHM_NAME, errnum, strerror(errnum));
        return;
    }

    struct stat shm_stat;
    const int status = fstat(fd, &shm_stat);
    if (status != 0) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not stat SHM object, fstat(fd, &shm_stat) failed with error code %d (%s).\n",
            errnum, strerror(errnum));
        close(fd);
        return;
    }

    if (shm_stat.st_size < sizeof(struct shm_header)) {
        fprintf(stderr,
            "Stange st_size value %ld has been returned by fstat(fd, &shm_stat).\n",
            shm_stat.st_size);
        close(fd);
        return;
    }

    const size_t shm_sz = (size_t)shm_stat.st_size;

    void * const shm = mmap(NULL, shm_sz, PROT_READ, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        const int errnum = errno;
        fprintf(stderr,
            "Can not map SHM object to mempry, mmap(NULL, %lu, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) failed with error code %d (%s).\n",
            shm_sz, errnum, strerror(errnum));
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    const struct shm_header * const header = move_ptr(shm, 0);

    const int cmp = strncmp(header->signature, SHM_SIGNATURE, 32);
    if (cmp != 0) {
        fprintf(stderr, "Wrong signature in SHM object.\n");
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    if (header->version_hi > VERSION_HI) {
        fprintf(stderr,
            "Unsupported version %u, current version id %d.\n",
            header->version_hi, VERSION_HI);
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    const uint32_t qcode_data = header->qcode_data;
    if (qcode_data > SIDE_BITBOARD_CODE_COUNT) {
        fprintf(stderr,
            "Unsupported value for qcode_data in header (%u), maximum supported is %u.\n",
            qcode_data, SIDE_BITBOARD_CODE_COUNT);
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    const size_t code_data_sz = sizeof(struct code_data_info) * qcode_data * qcode_data;
    const size_t header_with_code_data_sz = header->header_sz + code_data_sz;
    if (shm_sz < header_with_code_data_sz) {
        fprintf(stderr,
            "SHM corrupted, total size = %lu, but header (%u) with code data (%lu) required %lu.\n",
            shm_sz, header->header_sz, code_data_sz, header_with_code_data_sz);
        close(fd);
        shm_unlink(SHM_NAME);
        return;
    }

    const struct code_data_info * code_data_ptr = move_ptr(shm, header->header_sz);
    for (int i=0; i<qcode_data; ++i)
    for (int j=0; j<qcode_data; ++j) {

        const uint64_t offset = code_data_ptr->offset;
        const uint64_t sz = code_data_ptr->sz;
        ++code_data_ptr;

        if (offset == 0 || sz == 0) {
            continue;
        }

        const size_t data_end = offset + sz;
        const size_t total = position_code_infos[i][j].total;
        if (sz < total) {
            fprintf(stderr,
                "SHM corrupted, code data for %d/%d has too small size %lu (expected at least %lu).\n",
                i, j, sz, total);
            close(fd);
            shm_unlink(SHM_NAME);
            return;
        }
        if (shm_sz < data_end) {
            fprintf(stderr,
                "SHM corrupted, code data for %d/%d with offset %lu and size %lu (ends at %lu) is out of SHM file with size %lu.\n",
                i, j, offset, sz, offset+sz, shm_sz);
            close(fd);
            shm_unlink(SHM_NAME);
            return;
        }
    }

    etb_free();

    code_data_ptr = move_ptr(shm, header->header_sz);
    for (int i=0; i<qcode_data; ++i)
    for (int j=0; j<qcode_data; ++j) {

        const uint64_t offset = code_data_ptr->offset;
        const uint64_t sz = code_data_ptr->sz;
        ++code_data_ptr;

        if (offset == 0 || sz == 0) {
            continue;
        }

        set_position_code_data(i, j, move_ptr(shm, offset));
    }

    shm_info = (struct shm_info){ .ptr = shm, .sz = shm_sz, .fd = fd };
}



/*
 * Tests
 */

#ifdef WITH_TESTS

void gen_moves(struct move_ctx * restrict ctx)
{
    test_fail("Call to undefinite function “%s”.\n", __FUNCTION__);
}

void position_print_fen(const struct position * const position)
{
    test_fail("Call to undefinite function “%s”.\n", __FUNCTION__);
}

static void check_choose(const int n, const int k, const uint64_t value)
{
    const uint64_t result = choose[n][k];
    if (result != value) {
        test_fail("Wrong choose C(%d, %d) = %lu, expected %lu.", n, k, result, value);
    }
}

static int test_choose(void)
{
    check_choose(20,  2,       190);
    check_choose(32,  5,    201376);
    check_choose(28, 10,  13123110);
    check_choose(32, 16, 601080390);
    check_choose(31, 29,       465);
    check_choose(31, 32,         0);
    return 0;
}

static void check_offset(
    const struct position_code_info * const info,
    const int index,
    const uint64_t value)
{
    const uint64_t result = info->fr_offsets[index];
    if (result != value) {
        test_fail("Wrong offset %lu, expected %lu for index %d in position info struct.", result, value, index);
    }
}

static int test_fr_offsets(void)
{
    const int wcode = calc_code_and_check(3, 2, "wcode");
    const int bcode = calc_code_and_check(2, 1, "bcode");
    const struct position_code_info * const info = &position_code_infos[wcode][bcode];

    check_offset(info, 0, 0);
    check_offset(info, 1, 7176);
    check_offset(info, 2, 9768);
    check_offset(info, 3, 9936);
    check_offset(info, 4, 9936);
    check_offset(info, 5, 9936);
    return 0;
}

static int test_index_deindex(void)
{
    static const int N = 4;
    int indexes[N];
    for (uint64_t index=0; index<5000; ++index) {
        cdeindex(N, indexes, index);
        uint64_t calculated = cindex(N, indexes);
        if (index != calculated) {
            test_fail("Wring index/deindex for %lu (%d, %d, %d, %d), calculated %lu.",
                index, indexes[0], indexes[1], indexes[2], indexes[3], calculated);
        }
    }

    return 0;
}

struct position_with_index
{
    struct position position;
    struct position rposition;
    uint64_t index;
    uint64_t total;
};

struct position_with_index position_index_data[] = {
    {
        .position = {
            .active = WHITE,
            .bitboards = {
                [IDX_ALL_0] = MASK(G1) | MASK(B2) | MASK(G3),
                [IDX_ALL_1] = MASK(A5) | MASK(E7),
                [IDX_SIM_0] = MASK(G1) | MASK(B2),
                [IDX_SIM_1] = MASK(A5)
            }
        },
        .rposition = {
            .active = BLACK,
            .bitboards = {
                [IDX_ALL_0] = MASK(H4) | MASK(D2),
                [IDX_ALL_1] = MASK(B8) | MASK(G7) | MASK(B6),
                [IDX_SIM_0] = MASK(H4),
                [IDX_SIM_1] = MASK(B8) | MASK(G7),
            },
        },
        .index = 2778902,
        .total = 2*8068032
    }
};

static void test_one_position_to_index(
    const char * const title,
    const struct position * const position,
    const uint64_t index)
{
    const struct position_code_info * const info = get_position_info(position);
    const uint64_t calculated = position_to_index(position, info);
    if (calculated != index) {
        test_fail("Wrong %s position index, expected %lu, calculated %lu.", title, index, calculated);
    }
}

static int test_position_to_index(void)
{
    const int qtest_positions = sizeof(position_index_data) / sizeof(position_index_data[0]);
    const struct position_with_index * const end = position_index_data + qtest_positions;
    const struct position_with_index * ptr = position_index_data;

    for (; ptr != end; ++ptr) {
        test_one_position_to_index("normal", &ptr->position, ptr->index);
        test_one_position_to_index("reversed", &ptr->rposition, ptr->index);
    }

    return 0;
}

static void diff_positions(const struct position * const a, const struct position * const b)
{
    if (a->active != b->active) {
        fprintf(stderr, "Active a->%d, expected %d.\n", a->active, b->active);
    }
    if (a->bitboards[IDX_ALL_0] != b->bitboards[IDX_ALL_0]) {
        fprintf(stderr, "WALL a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_ALL_0], b->bitboards[IDX_ALL_0]);
    }
    if (a->bitboards[IDX_ALL_1] != b->bitboards[IDX_ALL_1]) {
        fprintf(stderr, "BALL a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_ALL_1], b->bitboards[IDX_ALL_1]);
    }
    if (a->bitboards[IDX_SIM_0] != b->bitboards[IDX_SIM_0]) {
        fprintf(stderr, "WSIM a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_SIM_0], b->bitboards[IDX_SIM_0]);
    }
    if (a->bitboards[IDX_SIM_1] != b->bitboards[IDX_SIM_1]) {
        fprintf(stderr, "BSIM a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_SIM_1], b->bitboards[IDX_SIM_1]);
    }
}

static inline int cmp_positions(const struct position * const a, const struct position * const b)
{
    if (memcmp(a, b, sizeof(struct position)) == 0) {
        return 0;
    }

    diff_positions(a, b);
    return 1;
}

static void test_one_index_to_position(
    const char * const title,
    const struct position * const position,
    const uint64_t index)
{
    struct position calculated;
    const struct position_code_info * const info = get_position_info(position);
    const int status = index_to_position(&calculated, info, index);

    if (status != 0) {
        test_fail("Wrong status %d for %s position.", status, title);
    }

    if (cmp_positions(&calculated, position) != 0) {
        test_fail("Position (%s) mismatch!", title);
    }
}

static int test_index_to_position(void)
{
    const int qtest_positions = sizeof(position_index_data) / sizeof(position_index_data[0]);
    const struct position_with_index * const end = position_index_data + qtest_positions;
    const struct position_with_index * ptr = position_index_data;

    for (; ptr != end; ++ptr) {
        test_one_index_to_position("normal", &ptr->position, ptr->index);
        test_one_index_to_position("reversed", &ptr->rposition, ptr->index);
    }

    return 0;
}

static void check_reverse(const bitboard_t a, const bitboard_t b)
{
    const bitboard_t c = reverse(a);
    const bitboard_t d = reverse(b);

    if (b != c) {
        test_fail("Wrong reverse for bitboards 0x%08X (0x%08X) and 0x%08X (0x%08X).", a, c, b, d);
    }

    if (a != d) {
        test_fail("Wrong reverse for bitboards 0x%08X (0x%08X) and 0x%08X (0x%08X).", b, d, a, c);
    }
}

static int test_reverse(void)
{
    check_reverse(BOARD, BOARD);
    check_reverse(RANK_1, RANK_8);
    check_reverse(RANK_2, RANK_7);
    check_reverse(RANK_3, RANK_6);
    check_reverse(RANK_4, RANK_5);
    check_reverse(MASK(A1), MASK(H8));
    check_reverse(MASK(E3), MASK(D6));
    check_reverse(MASK(F4), MASK(C5));
    check_reverse(MASK(A5) | MASK(E7), MASK(H4) | MASK(D2));

    return 0;
}

static int test_position_info_total(void)
{
    const int qtest_positions = sizeof(position_index_data) / sizeof(position_index_data[0]);
    const struct position_with_index * const end = position_index_data + qtest_positions;
    const struct position_with_index * ptr = position_index_data;

    for (; ptr != end; ++ptr) {
        const struct position_code_info * const info = get_position_info(&ptr->position);
        if (info->total != ptr->total) {
            test_fail("Total mismatch, table %lu, expected %lu.\n", info->total, ptr->total);
        }
    }

    return 0;
}

static void test_code_index(
    const struct position_code_info * const info,
    const uint64_t index)
{
    struct position position;
    int status = index_to_position(&position, info, index);
    if (status != 0) {
        test_fail("index_to_position fails for index %lu.", index);
    }
    uint64_t index0 = position_to_index(&position, info);
    if (index != index0) {
        test_fail("Rotation index mismatch: %lu -> position -> %lu.", index, index0);
    }
}

static int test_code(const int wcode, const int bcode)
{
    const struct position_code_info * const info = &position_code_infos[wcode][bcode];
    static const uint64_t SAMPLE_COUNT = 2014;
    const uint64_t delta = info->total > SAMPLE_COUNT ? info->total / SAMPLE_COUNT : 1;
    for (uint64_t index = 0; index < info->total; index += delta) {
        test_code_index(info, index);
    }

    return 0;
}

static int test_code_N(const int n)
{
    printf("%d\n", n);
    for (int i=0; i<=n; ++i) {
        test_code(i, n-i);
    }

    return 0;
}

static int test_code_0(void)
{
    return test_code_N(0);
}

static int test_code_1(void)
{
    return test_code_N(1);
}

static int test_code_2(void)
{
    return test_code_N(2);
}

static int test_code_3(void)
{
    return test_code_N(3);
}

static int test_code_4(void)
{
    return test_code_N(4);
}

static int test_code_5(void)
{
    return test_code_N(5);
}

static const int expected_pos_codes[13][13] = {
    { -1,   1,   4,   8,  13,  19,  26,  34,  43,  53,  64,  76,  89 },
    {  0,   3,   7,  12,  18,  25,  33,  42,  52,  63,  75,  88,  -1 },
    {  2,   6,  11,  17,  24,  32,  41,  51,  62,  74,  87,  -1,  -1 },
    {  5,  10,  16,  23,  31,  40,  50,  61,  73,  86,  -1,  -1,  -1 },
    {  9,  15,  22,  30,  39,  49,  60,  72,  85,  -1,  -1,  -1,  -1 },
    { 14,  21,  29,  38,  48,  59,  71,  84,  -1,  -1,  -1,  -1,  -1 },
    { 20,  28,  37,  47,  58,  70,  83,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 27,  36,  46,  57,  69,  82,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 35,  45,  56,  68,  81,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 44,  55,  67,  80,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 54,  66,  79,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 65,  78,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 },
    { 77,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 }
};

static int test_position_code(void)
{
    for (int sim = 0; sim < 13; ++sim)
    for (int mam = 0; mam < 13; ++mam) {
        const int expected = expected_pos_codes[sim][mam];
        if (expected == -1) {
            continue;
        }
        const int all = sim + mam;
        const int code = bitboard_stat_to_code(all, sim);
        if (code != expected) {
            test_fail("bitboard_stat_to_code(%d, %d) fails: returned value %d, expected %d.", all, sim, code, expected);
        }
    }

    return 0;
}

#endif
