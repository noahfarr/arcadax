#ifndef ARC_GAMES_SB26_H
#define ARC_GAMES_SB26_H

#include "arc/engine.h"

#define SB26_MAX_FRAMES 4
#define SB26_MAX_CARDS 12
#define SB26_MAX_STACK 6
#define SB26_MAX_TRAY 9
#define SB26_CURSOR_SLOTS 5
#define SB26_MAX_UNDO 65

struct sb26_aux {
	int32_t num_slots;

	int32_t energy;
	int32_t selected;
	int32_t stack_frame[SB26_MAX_STACK];
	int32_t stack_slot[SB26_MAX_STACK];
	int32_t stack_depth;
	int32_t cursor_count;
	int32_t pmygakdvy;
	uint8_t ppsxsxiod;

	uint8_t *fading;
	int32_t *fade_target;

	int32_t *undo_x;
	int32_t *undo_y;
	uint8_t *undo_vis;
	int32_t undo_depth;

	int32_t japgbruyb;
	int32_t lmvwmlqtw;
	int32_t xjxrqgaqw;
	int32_t bbiavyren;
	int32_t ftyhvmeft;
	int32_t artsfnufc;
	int32_t modqnpqfi;
	int32_t jlcrtmkes;

	int32_t tw_role_a, tw_fx_a, tw_fy_a, tw_fw_a, tw_tx_a, tw_ty_a, tw_tw_a;
	int32_t tw_role_b, tw_fx_b, tw_fy_b, tw_fw_b, tw_tx_b, tw_ty_b, tw_tw_b;

	uint8_t peek_active;
	uint8_t *peek_hidden;
	int8_t snapshot[ARC_FRAME_SIZE * ARC_FRAME_SIZE];

	uint8_t *click_off;
};

struct sb26_static {
	int32_t num_levels;
	int32_t num_slots;

	int32_t item_tag;
	int32_t frame_tag;
	int32_t spot_tag;
	int32_t click_tag;

	int32_t tray_ghost_base;
	int32_t mrokwhyjs0;
	int32_t mrokwhyjs1;
	int32_t mjeqtdqvm;
	int32_t ayaigjtxp;
	int32_t ohvavdnio;
	int32_t oyvbxwyug;
	int32_t cursor_base;

	const int32_t *qaagahahj;
	const int32_t *frame_children;
	const int32_t *card_slots;
	const int32_t *num_cards;
	const uint8_t *card_mask;
	const uint8_t *is_frameref;
	const int32_t *tray_item_slot;
	const int32_t *n_tray;
	const uint8_t *bg_mask;
	const int32_t *zpwrpmkvsv_slot;
	const uint8_t *allow_fixed;

	int32_t tween_val_min;
	int32_t tween_width;
	const int32_t *tween_table;
};

void sb26_aux_alloc(struct sb26_aux *aux, int32_t num_slots);
void sb26_aux_free(struct sb26_aux *aux);

void sb26_zero_aux(struct sb26_aux *aux);

void sb26_on_set_level(struct arc_sprites *sprites,
		       const struct sb26_static *st, int32_t level,
		       struct sb26_aux *aux, int32_t *next_order);

void sb26_step_once(struct arc_sprites *sprites,
		    const struct arc_camera *camera,
		    struct arc_render_scratch *scratch,
		    const struct sb26_static *st, int32_t level,
		    int32_t action_id, int32_t action_x, int32_t action_y,
		    int32_t action_count, struct sb26_aux *aux,
		    int32_t *next_order, int32_t *score, int32_t *status,
		    uint8_t *next_level, uint8_t *action_complete);

void sb26_render_interface(int8_t *frame, const struct arc_sprites *sprites,
			   const struct arc_camera *camera,
			   struct arc_render_scratch *scratch,
			   const struct sb26_static *st,
			   const struct sb26_aux *aux);

#endif
