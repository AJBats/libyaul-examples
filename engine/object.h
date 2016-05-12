/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef ENGINE_OBJECT_H
#define ENGINE_OBJECT_H

#include "engine.h"

#define OBJECT_COMPONENT_LIST_MAX       32

#define OBJECT_ID_BUILTIN_MASK          0xB000

struct component;

#define OBJECT_DECLARATIONS                                                    \
        bool active;                                                           \
        int32_t id;                                                            \
        struct {                                                               \
                struct component *component;                                   \
                uint32_t size;                                                 \
        } component_list[OBJECT_COMPONENT_LIST_MAX];                           \
        uint32_t component_count;                                              \
        struct {                                                               \
                /* Context used by objects system */                           \
                const void *instance;                                          \
                bool instantiated;                                             \
        } context;

#define OBJECT(x, member)                                                      \
        ((x)->CC_CONCAT(, member))

#define OBJECT_COMPONENT(x, index)                                             \
        ((struct object *)x)->component_list[(index)].component

#define OBJECT_COMPONENT_SIZE(x, index)                                        \
        ((struct object *)x)->component_list[(index)].size

#define OBJECT_COMPONENT_INITIALIZER(type, x)                                  \
        {                                                                      \
                .component = (struct component *)(x),                          \
                .size = (uint32_t)sizeof(struct type)                          \
        }

struct object {
        OBJECT_DECLARATIONS
} __may_alias;

extern void object_init(struct object *);
extern void object_destroy(struct object *);
extern void object_update(const struct object *);
extern void object_draw(const struct object *);
extern void object_instantiate(const struct object *, struct object *,
    uint32_t);
extern const struct component *object_component_find(const struct object *,
    int32_t);
extern void object_component_add(const struct object *, struct component *,
    uint32_t);
extern void object_component_remove(const struct object *,
    const struct component *);

#endif /* !ENGINE_OBJECT_H */
