void add_mam_takes(struct move_ctx * restrict ctx)
{
    for (int index = 0; index != ctx->mam_taking_last; NEXT_TAKING(index)) {

        const struct mam_take_magic * mtm;
        const struct taking * mt = ctx->mam_taking + index;

        #if USER_FRIENDLY
            const struct move_inner * move_inner = ctx->mam_move_inner + index;
        #endif

        bitboard_t all = ctx->all ^ mt->from;
        bitboard_t enemy = ctx->enemy ^ mt->killed;
        int saved_mam_taking_last = ctx->mam_taking_last;

        /* After subloop variables `f' and `fm' will point to the last field in mt->current */
        square_t sq;
        const struct square_magic * sm;

        bitboard_t current = mt->current;
        do {
            sq = get_first_square(current);
            sm = square_magic + sq;

            if (mt->way.is_7) {
                uint32_t index1 = ((all >> sm->shift1) & 0xFF);
                mtm = mam_take_magic_1[sq] + index1;
            } else {
                uint32_t index7 = (sm->factor7 * (all & sm->mask7)) >> 24;
                mtm = mam_take_magic_7[sq] + index7;
            }

            bitboard_t dead_0 = mtm->dead[0];
            bitboard_t dead_1 = mtm->dead[1];

            dead_0 &= enemy;
            dead_1 &= enemy;

            if (dead_0) {
                struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
                new_mt->from = mt->from;
                new_mt->killed = mt->killed | dead_0;
                new_mt->current = mtm->next[0];
                new_mt->way.is_7 = !mt->way.is_7;
                new_mt->way.is_down = 0;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = MASK(sq);
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            }

            if (dead_1) {
                struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
                new_mt->from = mt->from;
                new_mt->killed = mt->killed | dead_1;
                new_mt->current = mtm->next[1];
                new_mt->way.is_7 = !mt->way.is_7;
                new_mt->way.is_down = 1;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = MASK(sq);
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            }

            current &= current - 1; // OR clear_bit(current, field);

        }  while(current);

        if (mt->way.is_7) {
            uint32_t index7 = (sm->factor7 * (all & sm->mask7)) >> 24;
            mtm = mam_take_magic_7[sq] + index7;
        } else {
            uint32_t index1 = ((all >> sm->shift1) & 0xFF);
            mtm = mam_take_magic_1[sq] + index1;
        }

        bitboard_t dead = mtm->dead[mt->way.is_down] & enemy;
        if (dead) {
            struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
            new_mt->from = mt->from;
            new_mt->killed = mt->killed | dead;
            new_mt->current = mtm->next[mt->way.is_down];
            new_mt->way.is_7 = mt->way.is_7;
            new_mt->way.is_down = mt->way.is_down;

            #if USER_FRIENDLY
                struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                copy_inner(new_move_inner, move_inner);
                new_move_inner->values[new_move_inner->count++] = mt->current;;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        }



        if (saved_mam_taking_last == ctx->mam_taking_last) {

            bitboard_t tmp = mt->current;

            #ifdef PORTUGAL
                const bitboard_t * bbs = ctx->position->bitboards;
                bitboard_t sim = bbs[IDX_SIM_0] | bbs[IDX_SIM_1];
                bitboard_t mam = (bbs[IDX_ALL_0] | bbs[IDX_ALL_1]) ^ sim;
                int taked_sim = pop_count(sim & mt->killed);
                int taked_mam = pop_count(mam & mt->killed);
                int taking_score = 256 * (taked_sim + taked_mam) + taked_mam;
                if (taking_score < ctx->taking_score) {
                    tmp = 0;
                }
                if (taking_score > ctx->taking_score) {
                    ctx->taking_score = taking_score;
                    ctx->answer_count = 0;
                }
            #endif

            while (tmp) {
                struct position * restrict answer = ctx->answers + ctx->answer_count;
                answer->active = ctx->position->active ^ 1;

                int OUR_ALL = IDX_ALL | ctx->position->active;
                int OUR_SIM = IDX_SIM | ctx->position->active;
                int HIS_ALL = OUR_ALL ^ 1;
                int HIS_SIM = OUR_SIM ^ 1;

                const bitboard_t * now = ctx->position->bitboards;

                answer->bitboards[OUR_ALL] = now[OUR_ALL] ^ mt->from;
                answer->bitboards[OUR_SIM] = now[OUR_SIM] & answer->bitboards[OUR_ALL];
                answer->bitboards[OUR_ALL] ^= tmp & (-tmp);

                answer->bitboards[HIS_ALL] = now[HIS_ALL] ^ mt->killed;
                answer->bitboards[HIS_SIM] = now[HIS_SIM] & answer->bitboards[HIS_ALL];

                #if USER_FRIENDLY
                    struct move * restrict move = ctx->moves + ctx->answer_count;
                    move->from = mt->from;
                    move->to = tmp & (-tmp);
                    move->killed = mt->killed;
                    move->is_mam = 1;
                    copy_inner(&move->inner, move_inner);
                #endif

                ++ctx->answer_count;
                tmp &= tmp - 1;
            }
        }
    }
}

void gen_mam_takes(struct move_ctx * restrict ctx)
{
    const struct position * position = ctx->position;
    side_t active = position->active;
    const bitboard_t * bitboards = position->bitboards;
    bitboard_t mams = bitboards[IDX_ALL | active] ^ bitboards[IDX_SIM | active];
    ctx->all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];
    ctx->enemy = bitboards[IDX_ALL_1 ^ active];

    bitboard_t from;

    for (; mams; mams ^= from) {
        from = mams & (-mams);
        square_t sq = get_first_square(mams);
        const struct square_magic * sm = square_magic + sq;
        uint32_t index1 = (ctx->all >> sm->shift1) & 0xFF;
        uint32_t index7 = (sm->factor7 * (ctx->all & sm->mask7)) >> 24;
        const struct mam_take_magic * mtm1 = mam_take_magic_1[sq] + index1;
        const struct mam_take_magic * mtm7 = mam_take_magic_7[sq] + index7;

        bitboard_t dead;

        dead = mtm1->dead[0] & ctx->enemy;
        if (dead) {
            struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
            new_mt->from = from;
            new_mt->killed = dead;
            new_mt->current = mtm1->next[0];
            new_mt->way.is_7 = 0;
            new_mt->way.is_down = 0;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        }

        dead = mtm1->dead[1] & ctx->enemy;
        if (dead) {
            struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
            new_mt->from = from;
            new_mt->killed = dead;
            new_mt->current = mtm1->next[1];
            new_mt->way.is_7 = 0;
            new_mt->way.is_down = 1;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        }

        dead = mtm7->dead[0] & ctx->enemy;
        if (dead) {
            struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
            new_mt->from = from;
            new_mt->killed = dead;
            new_mt->current = mtm7->next[0];
            new_mt->way.is_7 = 1;
            new_mt->way.is_down = 0;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        }

        dead = mtm7->dead[1] & ctx->enemy;
        if (dead) {
            struct taking * restrict new_mt = ctx->mam_taking + ctx->mam_taking_last;
            new_mt->from = from;
            new_mt->killed = dead;
            new_mt->current = mtm7->next[1];
            new_mt->way.is_7 = 1;
            new_mt->way.is_down = 1;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        }
    }

    if (ctx->mam_taking_last) {
        add_mam_takes(ctx);
    }
}

void add_sim_takes(struct move_ctx * restrict ctx, side_t active)
{
    for (int index = 0; index != ctx->sim_taking_last; NEXT_TAKING(index)) {

        const struct taking * st = ctx->sim_taking + index;

        #if USER_FRIENDLY
            const struct move_inner * move_inner = ctx->sim_move_inner + index;
        #endif

        bitboard_t empty = (~ctx->all) ^ st->from;
        bitboard_t enemy = ctx->enemy ^ st->killed;
        int is_over = 1;

        bitboard_t try_u1 = ((((st->current & possible_u1) << 1) & enemy) << 1) & empty;
        bitboard_t try_d1 = ((((st->current & possible_d1) >> 1) & enemy) >> 1) & empty;
        bitboard_t try_u7 = rotate_u7(rotate_u7(st->current & possible_u7) & enemy) & empty;
        bitboard_t try_d7 = rotate_d7(rotate_d7(st->current & possible_d7) & enemy) & empty;

        if (try_u1) {
            is_over = 0;
            bitboard_t current = try_u1;
            bitboard_t killed = current >> 1;

            struct taking * restrict taking;
            if (current & promotion[active]) {
                taking = ctx->mam_taking + ctx->mam_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            } else {
                taking = ctx->sim_taking + ctx->sim_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->sim_taking_last);
            }

            taking->from = st->from;
            taking->killed = st->killed | killed;
            taking->current = current;
            taking->way.is_7 = 0;
            taking->way.is_down = 0;
        }

        if (try_d1) {
            is_over = 0;
            bitboard_t current = try_d1;
            bitboard_t killed = current << 1;

            struct taking * restrict taking;
            if (current & promotion[active]) {
                taking = ctx->mam_taking + ctx->mam_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            } else {
                taking = ctx->sim_taking + ctx->sim_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->sim_taking_last);
            }

            taking->from = st->from;
            taking->killed = st->killed | killed;
            taking->current = current;
            taking->way.is_7 = 0;
            taking->way.is_down = 1;
        }

        if (try_u7) {
            is_over = 0;
            bitboard_t current = try_u7;
            bitboard_t killed = rotate_d7(current);

            struct taking * restrict taking;
            if (current & promotion[active]) {
                taking = ctx->mam_taking + ctx->mam_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            } else {
                taking = ctx->sim_taking + ctx->sim_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->sim_taking_last);
            }

            taking->from = st->from;
            taking->killed = st->killed | killed;
            taking->current = current;
            taking->way.is_7 = 1;
            taking->way.is_down = 0;
        }

        if (try_d7) {
            is_over = 0;
            bitboard_t current = try_d7;
            bitboard_t killed = rotate_u7(current);

            struct taking * restrict taking;
            if (current & promotion[active]) {
                taking = ctx->mam_taking + ctx->mam_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->mam_taking_last);
            } else {
                taking = ctx->sim_taking + ctx->sim_taking_last;

                #if USER_FRIENDLY
                    struct move_inner * restrict new_move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                    copy_inner(new_move_inner, move_inner);
                    new_move_inner->values[new_move_inner->count++] = st->current;
                #endif

                NEXT_TAKING(ctx->sim_taking_last);
            }

            taking->from = st->from;
            taking->killed = st->killed | killed;
            taking->current = current;
            taking->way.is_7 = 1;
            taking->way.is_down = 1;
        }

        if (is_over) {

            #ifdef PORTUGAL
                const bitboard_t * bbs = ctx->position->bitboards;
                bitboard_t sim = bbs[IDX_SIM_0] | bbs[IDX_SIM_1];
                bitboard_t mam = (bbs[IDX_ALL_0] | bbs[IDX_ALL_1]) ^ sim;
                int taked_sim = pop_count(sim & st->killed);
                int taked_mam = pop_count(mam & st->killed);
                int taking_score = 256 * (taked_sim + taked_mam) + taked_mam;
                if (taking_score < ctx->taking_score) {
                    continue;
                }
                if (taking_score > ctx->taking_score) {
                    ctx->taking_score = taking_score;
                    ctx->answer_count = 0;
                }
            #endif

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            int OUR_ALL = IDX_ALL | ctx->position->active;
            int OUR_SIM = IDX_SIM | ctx->position->active;
            int HIS_ALL = OUR_ALL ^ 1;
            int HIS_SIM = OUR_SIM ^ 1;

            const bitboard_t * now = ctx->position->bitboards;

            answer->bitboards[OUR_ALL] = now[OUR_ALL] ^ st->from;
            answer->bitboards[OUR_SIM] = now[OUR_SIM] & answer->bitboards[OUR_ALL];
            answer->bitboards[OUR_ALL] ^= st->current;
            answer->bitboards[OUR_SIM] ^= st->current;

            answer->bitboards[HIS_ALL] = now[HIS_ALL] ^ st->killed;
            answer->bitboards[HIS_SIM] = now[HIS_SIM] & answer->bitboards[HIS_ALL];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = st->from;
                move->to = st->current;
                move->killed = st->killed;
                move->is_mam = 0;
                copy_inner(&move->inner, move_inner);
            #endif

            ++ctx->answer_count;
        }
    }
}

void gen_sim_takes(struct move_ctx * restrict ctx)
{
    const struct position * position = ctx->position;
    enum side_t active = position->active;
    const bitboard_t * bitboards = position->bitboards;

    ctx->all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];
    ctx->enemy = bitboards[IDX_ALL_1 ^ active];
    bitboard_t empty = ~ctx->all;
    bitboard_t singles = bitboards[IDX_SIM | active];

    bitboard_t try_u1 = ((((singles & possible_u1) << 1) & ctx->enemy) << 1) & empty;
    bitboard_t try_d1 = ((((singles & possible_d1) >> 1) & ctx->enemy) >> 1) & empty;
    bitboard_t try_u7 = rotate_u7(rotate_u7(singles & possible_u7) & ctx->enemy) & empty;
    bitboard_t try_d7 = rotate_d7(rotate_d7(singles & possible_d7) & ctx->enemy) & empty;

    #ifdef PORTUGAL
        if (active == WHITE) {
            try_d1 = 0;
            try_d7 = 0;
        }
        if (active == BLACK) {
            try_u1 = 0;
            try_u7 = 0;
        }
    #endif

    while (try_u1) {
        bitboard_t current = try_u1 & (-try_u1);
        bitboard_t killed = current >> 1;
        bitboard_t from = killed >> 1;
        try_u1 ^= current;

        struct taking * restrict taking;
        if (current & promotion[active]) {
            taking = ctx->mam_taking + ctx->mam_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        } else {
            taking = ctx->sim_taking + ctx->sim_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->sim_taking_last);
        }

        taking->from = from;
        taking->killed = killed;
        taking->current = current;
        taking->way.is_7 = 0;
        taking->way.is_down = 0;
    }

    while (try_d1) {
        bitboard_t current = try_d1 & (-try_d1);
        bitboard_t killed = current << 1;
        bitboard_t from = killed << 1;
        try_d1 ^= current;

        struct taking * restrict taking;
        if (current & promotion[active]) {
            taking = ctx->mam_taking + ctx->mam_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        } else {
            taking = ctx->sim_taking + ctx->sim_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->sim_taking_last);
        }

        taking->from = from;
        taking->killed = killed;
        taking->current = current;
        taking->way.is_7 = 0;
        taking->way.is_down = 1;
    }

    while (try_u7) {
        bitboard_t current = try_u7 & (-try_u7);
        bitboard_t killed = rotate_d7(current);
        bitboard_t from = rotate_d7(killed);
        try_u7 ^= current;

        struct taking * restrict taking;
        if (current & promotion[active]) {
            taking = ctx->mam_taking + ctx->mam_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        } else {
            taking = ctx->sim_taking + ctx->sim_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->sim_taking_last);
        }

        taking->from = from;
        taking->killed = killed;
        taking->current = current;
        taking->way.is_7 = 1;
        taking->way.is_down = 0;
    }

    while (try_d7) {
        bitboard_t current = try_d7 & (-try_d7);
        bitboard_t killed = rotate_u7(current);
        bitboard_t from = rotate_u7(killed);
        try_d7 ^= current;

        struct taking * restrict taking;
        if (current & promotion[active]) {
            taking = ctx->mam_taking + ctx->mam_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->mam_move_inner + ctx->mam_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->mam_taking_last);
        } else {
            taking = ctx->sim_taking + ctx->sim_taking_last;

            #if USER_FRIENDLY
                struct move_inner * restrict move_inner = ctx->sim_move_inner + ctx->sim_taking_last;
                move_inner->count = 0;
            #endif

            NEXT_TAKING(ctx->sim_taking_last);
        }

        taking->from = from;
        taking->killed = killed;
        taking->current = current;
        taking->way.is_7 = 1;
        taking->way.is_down = 1;
    }

    if (ctx->sim_taking_last) {
        add_sim_takes(ctx, active);
    }
}

void gen_sim_moves(struct move_ctx * restrict ctx)
{
    const struct position * position = ctx->position;
    enum side_t active = position->active;
    const bitboard_t * bitboards = position->bitboards;

    bitboard_t our_sim = bitboards[IDX_SIM | active];
    ctx->all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];
    bitboard_t empty = ~ctx->all;

    int OUR_ALL = IDX_ALL | ctx->position->active;
    int OUR_SIM = IDX_SIM | ctx->position->active;
    int HIS_ALL = OUR_ALL ^ 1;
    int HIS_SIM = OUR_SIM ^ 1;

    if (active == WHITE) {
        bitboard_t try_1 = ((our_sim & can_move_u1) << 1) & empty;
        bitboard_t try_7 = rotate_u7(our_sim & can_move_u7) & empty;

        while (try_1) {
            bitboard_t current = try_1 & (-try_1);
            try_1 ^= current;

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            bitboard_t from = current >> 1;

            answer->bitboards[OUR_ALL] = position->bitboards[OUR_ALL] ^ current ^ from;
            answer->bitboards[OUR_SIM] = position->bitboards[OUR_SIM] ^ current ^ from;
            answer->bitboards[OUR_SIM] &= ~promotion[WHITE];

            answer->bitboards[HIS_ALL] = position->bitboards[HIS_ALL];
            answer->bitboards[HIS_SIM] = position->bitboards[HIS_SIM];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = from;
                move->to = current;
                move->killed = 0;
                move->is_mam = !! (current & promotion[WHITE]);
                move->inner.count = 0;
            #endif

            ++ctx->answer_count;
        }

        while (try_7) {
            bitboard_t current = try_7 & (-try_7);
            try_7 ^= current;

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            bitboard_t from = rotate_d7(current);

            answer->bitboards[OUR_ALL] = position->bitboards[OUR_ALL] ^ current ^ from;
            answer->bitboards[OUR_SIM] = position->bitboards[OUR_SIM] ^ current ^ from;
            answer->bitboards[OUR_SIM] &= ~promotion[WHITE];

            answer->bitboards[HIS_ALL] = position->bitboards[HIS_ALL];
            answer->bitboards[HIS_SIM] = position->bitboards[HIS_SIM];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = from;
                move->to = current;
                move->killed = 0;
                move->is_mam = !! (current & promotion[WHITE]);
                move->inner.count = 0;
            #endif

            ++ctx->answer_count;
        }
    }

    if (active == BLACK) {
        bitboard_t try_1 = ((our_sim & can_move_d1) >> 1) & empty;
        bitboard_t try_7 = rotate_d7(our_sim & can_move_d7) & empty;

        while (try_1) {
            bitboard_t current = try_1 & (-try_1);
            try_1 ^= current;

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            bitboard_t from = current << 1;

            answer->bitboards[OUR_ALL] = position->bitboards[OUR_ALL] ^ current ^ from;
            answer->bitboards[OUR_SIM] = position->bitboards[OUR_SIM] ^ current ^ from;
            answer->bitboards[OUR_SIM] &= ~promotion[BLACK];

            answer->bitboards[HIS_ALL] = position->bitboards[HIS_ALL];
            answer->bitboards[HIS_SIM] = position->bitboards[HIS_SIM];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = from;
                move->to = current;
                move->killed = 0;
                move->is_mam = !! (current & promotion[BLACK]);
                move->inner.count = 0;
            #endif

            ++ctx->answer_count;
        }

        while (try_7) {
            bitboard_t current = try_7 & (-try_7);
            try_7 ^= current;

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            bitboard_t from = rotate_u7(current);

            answer->bitboards[OUR_ALL] = position->bitboards[OUR_ALL] ^ current ^ from;
            answer->bitboards[OUR_SIM] = position->bitboards[OUR_SIM] ^ current ^ from;
            answer->bitboards[OUR_SIM] &= ~promotion[BLACK];

            answer->bitboards[HIS_ALL] = position->bitboards[HIS_ALL];
            answer->bitboards[HIS_SIM] = position->bitboards[HIS_SIM];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = from;
                move->to = current;
                move->killed = 0;
                move->is_mam = !! (current & promotion[BLACK]);
                move->inner.count = 0;
            #endif

            ++ctx->answer_count;
        }
    }
}

void gen_mam_moves(struct move_ctx * restrict ctx)
{
    const struct position * position = ctx->position;
    side_t active = position->active;
    const bitboard_t * bitboards = position->bitboards;
    bitboard_t mams = bitboards[IDX_ALL | active] ^ bitboards[IDX_SIM | active];
    ctx->all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];
    ctx->enemy = bitboards[IDX_ALL_1 ^ active];

    int OUR_ALL = IDX_ALL | ctx->position->active;
    int OUR_SIM = IDX_SIM | ctx->position->active;
    int HIS_ALL = OUR_ALL ^ 1;
    int HIS_SIM = OUR_SIM ^ 1;

    bitboard_t from;

    for (; mams; mams ^= from) {

        from = mams & (-mams);
        square_t sq = get_first_square(from);

        bitboard_t neighbor_u1 = (from & can_move_u1) << 1;
        bitboard_t neighbor_d1 = (from & can_move_d1) >> 1;
        bitboard_t neighbor_u7 = rotate_u7(from & can_move_u7);
        bitboard_t neighbor_d7 = rotate_d7(from & can_move_d7);

        bitboard_t u1 = (neighbor_u1 & ctx->all) ? 0 : BOARD;
        bitboard_t d1 = (neighbor_d1 & ctx->all) ? 0 : BOARD;
        bitboard_t u7 = (neighbor_u7 & ctx->all) ? 0 : BOARD;
        bitboard_t d7 = (neighbor_d7 & ctx->all) ? 0 : BOARD;

        bitboard_t neighbor = neighbor_u1 | neighbor_d1 | neighbor_u7 | neighbor_d7;
        bitboard_t fake_all = ctx->all | neighbor;

        const struct square_magic * sm = square_magic + sq;
        uint32_t index1 = (fake_all >> sm->shift1) & 0xFF;
        uint32_t index7 = (sm->factor7 * (fake_all & sm->mask7)) >> 24;
        const struct mam_take_magic * mtm1 = mam_take_magic_1[sq] + index1;
        const struct mam_take_magic * mtm7 = mam_take_magic_7[sq] + index7;

        u1 &= mtm1->next[0];
        d1 &= mtm1->next[1];
        u7 &= mtm7->next[0];
        d7 &= mtm7->next[1];

        bitboard_t next = u1 | d1 | u7 | d7 | neighbor;
        next &= ~ctx->all;

        bitboard_t current;
        for (; next; next ^= current) {

            current = next & (-next);

            struct position * restrict answer = ctx->answers + ctx->answer_count;
            answer->active = ctx->position->active ^ 1;

            answer->bitboards[OUR_ALL] = position->bitboards[OUR_ALL] ^ current ^ from;
            answer->bitboards[OUR_SIM] = position->bitboards[OUR_SIM];

            answer->bitboards[HIS_ALL] = position->bitboards[HIS_ALL];
            answer->bitboards[HIS_SIM] = position->bitboards[HIS_SIM];

            #if USER_FRIENDLY
                struct move * restrict move = ctx->moves + ctx->answer_count;
                move->from = from;
                move->to = current;
                move->killed = 0;
                move->is_mam = 1;
                move->inner.count = 0;
            #endif

            ++ctx->answer_count;
        }
    }
}

void gen_moves(struct move_ctx * restrict ctx)
{
    ctx->taking_score = 0;
    ctx->answer_count = 0;
    ctx->sim_taking_last = 0;
    ctx->mam_taking_last = 0;

    gen_sim_takes(ctx);
    gen_mam_takes(ctx);

    if (ctx->answer_count == 0) {
        gen_mam_moves(ctx);
        gen_sim_moves(ctx);
    }
}
