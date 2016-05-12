/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef ENGINE_LAYER_H
#define ENGINE_LAYER_H

#include "../engine.h"

struct layer {
        COMPONENT_DECLARATIONS

        bool visible;
        int16_t number;
} __aligned (64);

extern void component_layer_on_init(struct component *);
extern void component_layer_on_update(struct component *);
extern void component_layer_on_draw(struct component *);
extern void component_layer_on_destroy(struct component *);

#endif /* !ENGINE_LAYER_H */
