/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef ENGINE_COMPONENT_H
#define ENGINE_COMPONENT_H

#include "engine.h"

#define COMPONENT_ID_TRANSFORM          0x2000
#define COMPONENT_ID_CAMERA             0x2001
#define COMPONENT_ID_COLLIDER           0x2002
#define COMPONENT_ID_RIGID_BODY         0x2003
#define COMPONENT_ID_SPRITE             0x2004
#define COMPONENT_ID_LAYER              0x2005

#define COMPONENT_ID_BUILTIN_MASK       0xD000
#define COMPONENT_ID_USER_MASK          0xE000

#define COMPONENT_DECLARATIONS                                                 \
    bool active;                                                               \
    bool initialized;                                                          \
    int32_t id;                                                                \
    const struct object *object;                                               \
    /* Events */                                                               \
    void (*on_init)(struct component *);                                       \
    void (*on_update)(struct component *);                                     \
    void (*on_draw)(struct component *);                                       \
    void (*on_destroy)(struct component *);

#define COMPONENT(x, member)                                                   \
        ((x)->CC_CONCAT(, member))

#define COMPONENT_EVENT(x, event)                                              \
        ((x)->CC_CONCAT(on_, event))

#define COMPONENT_FUNCTION(type, member)                                       \
        ((x)->functions.CC_CONCAT(m_, function))

#define COMPONENT_FUNCTION_CALL(x, name, args...) do {                         \
        assert(((struct component *)(x)) != NULL);                             \
        assert(COMPONENT(((struct component *)(x)), initialized));             \
        assert(((x))->functions.CC_CONCAT(m_, name) != NULL);                  \
        ((x))->functions.CC_CONCAT(m_, name)((struct component *)(x), ##args); \
} while (0)

#define THIS(type, member)                                                     \
        (((struct type *)this)->CC_CONCAT(, member))

#define THIS_P_DATA(type, member)                                              \
        (((struct type *)this)->private_data.CC_CONCAT(m_, member))

#define THIS_FUNCTION(type, member)                                            \
        (((struct type *)this)->functions.CC_CONCAT(m_, member))

struct component {
        COMPONENT_DECLARATIONS
} __may_alias;

#endif /* !ENGINE_COMPONENT_H */
