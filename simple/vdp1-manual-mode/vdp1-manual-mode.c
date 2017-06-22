/*
 * Copyright (c) 2012-2017 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#define ASSERT                  1
#define FIXMATH_NO_OVERFLOW     1
#define FIXMATH_NO_ROUNDING     1

#include <yaul.h>

#include <stdio.h>

#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   224

#define RGB888_TO_RGB555(r, g, b) (0x8000 | (((b) >> 3) << 10) |               \
    (((g) >> 3) << 5) | ((r) >> 3))

#define FCT     (1 << 0)
#define FCM     (1 << 1)

#define VBE     (1 << 3)

#define BEF     (1 << 0)
#define CEF     (1 << 1)

static char text_buffer[256] __unused;

static volatile uint32_t vblank_tick = 0;
static volatile uint32_t hblank_tick = 0;
static volatile uint32_t vblank_in_scanline = 0;
static volatile uint32_t vblank_out_scanline = 0;

static void hblank_in_handler(irq_mux_handle_t *);
static void hardware_init(void);
static void vblank_in_handler(irq_mux_handle_t *);
static void vblank_out_handler(irq_mux_handle_t *);
static void draw_polygon(color_rgb555_t);

void
main(void)
{
        hardware_init();

        MEMORY_WRITE(16, VDP1(TVMR), 0x0000);
        MEMORY_WRITE(16, VDP1(FBCR), 0x0000);
        MEMORY_WRITE(16, VDP1(PTMR), 0x0000);

        /* Erase FB */
        MEMORY_WRITE(16, VDP1(TVMR), 0x0000);
        MEMORY_WRITE(16, VDP1(FBCR), FCM);

        /* Since display is off, we're in VBLANK-IN */
        draw_polygon(COLOR_RGB555(0, 31, 0));
        /* Commit VDP1 command tables to VDP1 VRAM */
        vdp1_cmdt_list_commit();
        /* Start drawing immediately */
        MEMORY_WRITE(16, VDP1(PTMR), 0x0001);

        /* Wait until VDP1 finishes drawing */
        while ((MEMORY_READ(16, VDP1(EDSR)) & CEF) != CEF) {
        }

        /* Idle */
        MEMORY_WRITE(16, VDP1(PTMR), 0x0000);

        /* Change FB */
        MEMORY_WRITE(16, VDP1(TVMR), 0x0000);
        MEMORY_WRITE(16, VDP1(FBCR), FCM | FCT);

        /* Wait for change of frame */
        while ((MEMORY_READ(16, VDP1(EDSR)) & CEF) == CEF) {
        }

        /* Turn on display */
        vdp2_tvmd_display_set();

        while (true) {
        }
}

static void
hardware_init(void)
{
        /* VDP2 */
        vdp2_init();

        /* VDP1 */
        vdp1_init();

        /* Disable interrupts */
        cpu_intc_disable();

        irq_mux_t *hblank_in;
        irq_mux_t *vblank_in;
        irq_mux_t *vblank_out;

        hblank_in = vdp2_tvmd_hblank_in_irq_get();
        irq_mux_handle_add(hblank_in, hblank_in_handler, NULL);

        vblank_in = vdp2_tvmd_vblank_in_irq_get();
        irq_mux_handle_add(vblank_in, vblank_in_handler, NULL);

        vblank_out = vdp2_tvmd_vblank_out_irq_get();
        irq_mux_handle_add(vblank_out, vblank_out_handler, NULL);

        /* Enable interrupts */
        cpu_intc_enable();

        vdp2_scrn_back_screen_color_set(VRAM_ADDR_4MBIT(2, 0x01FFFE),
            COLOR_RGB555(0, 0, 7));

        vdp2_tvmd_display_res_set(TVMD_INTERLACE_NONE, TVMD_HORZ_NORMAL_A,
            TVMD_VERT_224);

        /* VBE FCM FCT
         *  0   0   0     1-cycle mode                  Every 16.7ms
         *  0   0   1
         *  0   1   0     Manual mode (erase)           Erase next field
         *  0   1   1     Manual mode (change)          Change next field
         *  1   0   0
         *  1   0   1
         *  1   1   0
         *  1   1   1     Manual mode (erase/change)    Erase V-blank
         *
         * VBE: V-Blank Erase/Write
         * FCM: Frame Buffer Mode
         * FCT: Frame Buffer Trigger
         *
         * A field is the time it takes a scanning line to scan one
         * screen. For NTSC, this is 1/60s.
         *
         * A frame is the time it takes from one change of the frame
         * buffer to the next change.
         *
         * VDP1 can erase frame buffer during display, or in the
         * vertical blanking interval (VBI).
         *
         * Instruct VDP1 to erase the frame buffer the next time it is
         * displayed. This method requires you to know one frame in
         * advance when you'll be finished with drawing.
         *
         * Not knowing when you'll be ready to swap, you can tell VDP1
         * to start erasing the frame buffer at the beginning of the
         * next vertical blanking interval. Draw a normal polygon,
         * covering the entire screen to erase the screen.
         *         *
         * - When the change mode of the frame buffer is in 1-cycle
         *   mode, one frame is equal to one field.
         *
         * - When the frame buffer is changed every TWO fields, one
         *   frame comprises two fields.
         *
         * - When the frame buffer is changed only once a second in
         *   manual mode, one frame comprises 60 fields in NTSC.
         */
}

static void
draw_polygon(color_rgb555_t color __unused)
{
        vdp1_cmdt_list_begin(0); {
                struct vdp1_cmdt_local_coord local_coord;

                local_coord.lc_coord.x = SCREEN_WIDTH / 2;
                local_coord.lc_coord.y = SCREEN_HEIGHT / 2;

                struct vdp1_cmdt_system_clip_coord system_clip;

                system_clip.scc_coord.x = SCREEN_WIDTH - 1;
                system_clip.scc_coord.y = SCREEN_HEIGHT - 1;

                struct vdp1_cmdt_user_clip_coord user_clip;
                user_clip.ucc_coords[0].x = 0;
                user_clip.ucc_coords[0].y = 0;
                user_clip.ucc_coords[1].x = SCREEN_WIDTH - 1;
                user_clip.ucc_coords[1].y = SCREEN_HEIGHT - 1;

                vdp1_cmdt_system_clip_coord_set(&system_clip);
                vdp1_cmdt_user_clip_coord_set(&user_clip);

                local_coord.lc_coord.x = 0;
                local_coord.lc_coord.y = 0;
                vdp1_cmdt_local_coord_set(&local_coord);

                static struct vdp1_cmdt_polygon polygon;

                polygon.cp_color = (uint16_t)color.raw;
                polygon.cp_mode.transparent_pixel = true;
                polygon.cp_mode.end_code = false;
                polygon.cp_vertex.a.x = 0;
                polygon.cp_vertex.a.y = SCREEN_HEIGHT - 1;

                polygon.cp_vertex.b.x = SCREEN_WIDTH - 1;
                polygon.cp_vertex.b.y = SCREEN_HEIGHT - 1;

                polygon.cp_vertex.c.x = SCREEN_WIDTH - 1;
                polygon.cp_vertex.c.y = 0;

                polygon.cp_vertex.d.x = 0;
                polygon.cp_vertex.d.y = 0;

                vdp1_cmdt_polygon_draw(&polygon);

                vdp1_cmdt_end();
        } vdp1_cmdt_list_end(0);
}

static void
hblank_in_handler(irq_mux_handle_t *irq_mux __unused)
{
        hblank_tick++;
}

static void
vblank_in_handler(irq_mux_handle_t *irq_mux __unused)
{
        vblank_in_scanline = vdp2_tvmd_vcount_get();
}

static void
vblank_out_handler(irq_mux_handle_t *irq_mux __unused)
{
        vblank_out_scanline = vdp2_tvmd_vcount_get();
        vblank_tick++;
}