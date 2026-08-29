#ifndef ARCADAX_GAMES_TR87_H
#define ARCADAX_GAMES_TR87_H

#include "../engine.h"

enum {
    TR87_NUM_DIGITS = 7,
    TR87_MAX_TOP = 8,
    TR87_MAX_BOTTOM = 7,
    TR87_MAX_RULES = 8,
    TR87_MAX_CHAIN = 3,
    TR87_MAX_RESOLVED = 6,
    TR87_MAX_COLOR_TARGETS = 8,
    TR87_MAX_HIGHLIGHTS = 10,
    TR87_MAX_STAGES = TR87_MAX_TOP,
    TR87_SUBFRAMES_PER_STAGE = 7,
};

typedef struct {
    int32_t num_slots;
    int32_t *car_digit;
    int32_t cursor_index;
    int32_t budget;
    int32_t anim_frame;
    int32_t anim_stage_count;
    int32_t anim_color_slot[TR87_MAX_STAGES][TR87_MAX_COLOR_TARGETS];
    int32_t anim_color_len[TR87_MAX_STAGES];
    int32_t anim_highlight_slot[TR87_MAX_STAGES][TR87_MAX_HIGHLIGHTS];
    int32_t anim_highlight_len[TR87_MAX_STAGES];
} Tr87Aux;

typedef struct {
    int32_t num_levels;
    int32_t num_slots;
    int32_t ph;
    int32_t pw;
    int32_t cursor_top_slot;
    int32_t cursor_bottom_slot;
    int32_t highlight_base_slot;

    const int32_t *top_len;
    const int32_t *top_slot;
    const int32_t *bottom_len;
    const int32_t *bottom_slot;
    const int32_t *num_rules;
    const int32_t *rule_left_len;
    const int32_t *rule_left_slot;
    const int32_t *rule_right_len;
    const int32_t *rule_right_slot;
    const uint8_t *alter_rules_level;
    const uint8_t *tree_translation_level;
    const uint8_t *double_translation_level;
    const int32_t *budget0;
    const int32_t *slot_group;
    const int32_t *initial_digit;
    const uint8_t *is_car_slot;
    const int8_t *digit_patch;
    const int8_t *cursor_patch_top;
    const int8_t *cursor_patch_bottom;
    const int8_t *highlight_patch;
} Tr87Static;

void tr87_aux_alloc(Tr87Aux *aux, int32_t num_slots);
void tr87_aux_free(Tr87Aux *aux);

void tr87_zero_aux(Tr87Aux *aux);

void tr87_on_set_level(Sprites *sprites, const Tr87Static *st, int32_t level, Tr87Aux *aux);

void tr87_step_once(Sprites *sprites, const Tr87Static *st, int32_t level, int32_t action_id,
                    Tr87Aux *aux, int32_t *score, int32_t *status, uint8_t *next_level,
                    uint8_t *action_complete);

void tr87_render_interface(int8_t *frame, const Tr87Static *st, int32_t level,
                           const Tr87Aux *aux);

#endif
