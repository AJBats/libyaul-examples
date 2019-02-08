/*
 * Copyright (c) 2012-2017 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <yaul.h>

#include <stdio.h>
#include <stdlib.h>

#define FRT_INTERRUPT_PRIORITY_LEVEL 8

#define TIMER_MAX_TIMERS_COUNT 16

struct timer;

struct timer_event {
        uint32_t id;

        const struct timer *timer;
        void *work;

        uint32_t next_interval;
} __packed;

struct timer {
        uint32_t interval; /* Time in milliseconds */
        void (*callback)(struct timer_event *);
        void *work;
} __packed;

struct timer_state {
        bool valid;
        uint32_t id;
        struct timer event;
        uint32_t remaining;
} __packed;

static struct timer_state _timer_states[TIMER_MAX_TIMERS_COUNT];

static uint32_t _next_timer = 0;
static uint32_t _id_count = 0;

static volatile uint32_t _ovi_count = 0;
static volatile uint32_t _ocb_count = 0;

static void _timer_init(void);
static int32_t _timer_add(const struct timer *);
static int32_t _timer_remove(uint32_t) __unused;

static void _frt_compare_output_handler(void);
static void _frt_ovi_handler(void);
static void _frt_ocb_handler(void);
static void _timer_handler(struct timer_event *);

static void _hardware_init(void);

static char _buffer[256];

static volatile uint32_t _counter_1 = 0;
static volatile uint32_t _counter_2 = 0;
static volatile uint32_t _counter_3 = 0;
static volatile uint32_t _counter_4 = 0;

void
main(void)
{
        _hardware_init();

        dbgio_dev_default_init(DBGIO_DEV_VDP2);
        dbgio_dev_set(DBGIO_DEV_VDP2);

        _timer_init();

        struct timer match1 __unused = {
                .interval = 1000,
                .callback = _timer_handler,
                .work = (void *)&_counter_1
        };

        struct timer match2 __unused = {
                .interval = 2000,
                .callback = _timer_handler,
                .work = (void *)&_counter_2
        };

        struct timer match3 __unused = {
                .interval = 3,
                .callback = _timer_handler,
                .work = (void *)&_counter_3
        };

        struct timer match4 __unused = {
                .interval = 500,
                .callback = _timer_handler,
                .work = (void *)&_counter_4
        };

        _timer_add(&match1);
        _timer_add(&match2);
        _timer_add(&match3);
        _timer_add(&match4);

        while (true) {
                dbgio_buffer("[1;1H[2J");

                (void)sprintf(_buffer, "\n"
                    " counter_1: %10lu (1s)\n"
                    " counter_2: %10lu (2s)\n"
                    " counter_3: %10lu (3ms)\n"
                    " counter_4: %10lu (.5s)\n"
                    " ovi_count: %10lu\n"
                    " ocb_count: %10lu\n",
                    _counter_1,
                    _counter_2,
                    _counter_3,
                    _counter_4,
                    _ovi_count,
                    _ocb_count);
                dbgio_buffer(_buffer);

                dbgio_flush();
                vdp_sync(0);
        }
}

static void
_hardware_init(void)
{
        vdp2_tvmd_display_res_set(TVMD_INTERLACE_NONE, TVMD_HORZ_NORMAL_A,
            TVMD_VERT_224);

        vdp2_scrn_back_screen_color_set(VRAM_ADDR_4MBIT(3, 0x01FFFE),
            COLOR_RGB555(0, 3, 3));

        cpu_intc_mask_set(0);

        vdp2_tvmd_display_set();
}

static void
_frt_compare_output_handler(void)
{
        uint32_t frt_count;
        frt_count = cpu_frt_count_get();

        int32_t count_diff __unused;
        count_diff = frt_count - FRT_NTSC_320_8_COUNT_1MS;

        if (count_diff >= 0) {
                cpu_frt_count_set(count_diff);
        }

        uint32_t i;
        for (i = 0; i < TIMER_MAX_TIMERS_COUNT; i++) {
                struct timer_state *timer_state;
                timer_state = &_timer_states[i];

                /* Invalid timer */
                if (!timer_state->valid) {
                        continue;
                }

                timer_state->remaining--;

                if (timer_state->remaining != 0) {
                        continue;
                }

                struct timer_event event;

                event.id = timer_state->id;
                event.timer = &timer_state->event;
                event.work = timer_state->event.work;
                event.next_interval = timer_state->event.interval;

                timer_state->event.callback (&event);

                if (event.next_interval > 0) {
                        /* Choose the next non-zero interval */
                        timer_state->remaining = event.next_interval;

                        continue;
                }

                /* Invalidate the timer */
                timer_state->valid = false;
        }
}

static void
_timer_init(void)
{
        uint32_t timer;
        for (timer = 0; timer < TIMER_MAX_TIMERS_COUNT; timer++) {
                _timer_states[timer].valid = false;
        }

        cpu_frt_init(FRT_CLOCK_DIV_8);
        cpu_frt_oca_set(FRT_NTSC_320_8_COUNT_1MS, _frt_compare_output_handler);
        /* Match every 9.525μs */
        cpu_frt_ocb_set(32, _frt_ocb_handler);
        cpu_frt_count_set(0);
        cpu_frt_ovi_set(_frt_ovi_handler);
        cpu_frt_interrupt_priority_set(FRT_INTERRUPT_PRIORITY_LEVEL);
}

static int32_t
_timer_add(const struct timer *timer)
{
        /* Disable interrupts */
        uint32_t i_mask;
        i_mask = cpu_intc_mask_get();

        cpu_intc_mask_set(15);

        if (timer->callback == NULL) {
                return -1;
        }

        if (timer->interval == 0) {
                return -1;
        }

        struct timer_state *timer_state;
        timer_state = &_timer_states[_next_timer & 0x1F];

        if (timer_state->valid) {
                /* Look for a free timer */
                uint32_t timer;
                for (timer = 0; timer < TIMER_MAX_TIMERS_COUNT; timer++) {
                        timer_state = &_timer_states[timer];

                        if (!timer_state->valid) {
                                break;
                        }
                }

                _next_timer = timer;
        } else {
                _next_timer++;
        }

        if (_next_timer > TIMER_MAX_TIMERS_COUNT) {
                return -1;
        }

        timer_state->id = _id_count;
        memcpy(&timer_state->event, timer, sizeof(struct timer));
        timer_state->remaining = timer->interval;

        timer_state->valid = true;

        _id_count++;

        /* Enable interrupts */
        cpu_intc_mask_set(i_mask);

        return 0;
}

static int32_t
_timer_remove(uint32_t id)
{
        int32_t ret;

        /* Disable interrupts */
        uint32_t i_mask;
        i_mask = cpu_intc_mask_get();

        cpu_intc_mask_set(15);

        struct timer_state *timer_state;

        uint32_t timer;
        for (timer = 0; timer < TIMER_MAX_TIMERS_COUNT; timer++) {
                timer_state = &_timer_states[timer];

                if (timer_state->valid && (id == timer_state->id)) {
                        break;
                }
        }

        if (timer == TIMER_MAX_TIMERS_COUNT) {
                ret = -1;

                goto exit;
        }

        /* Point to next free timer */
        _next_timer = timer;

        timer_state->valid = false;

        ret = 0;

exit:
        cpu_intc_mask_set(i_mask);

        return ret;
}

static void
_frt_ovi_handler(void)
{
        _ovi_count++;
}

static void
_frt_ocb_handler(void)
{
        _ocb_count++;
}

static void
_timer_handler(struct timer_event *event)
{
        uint32_t *counter = (uint32_t *)event->work;

        (*counter)++;

        /* Set the next interval to zero to cancel this timer */
}