/*
Twin - A Tiny Window System
Copyright (c) 2024 National Cheng Kung University, Taiwan
All rights reserved.
*/

#include <aml.h>
#include <assert.h>
#include <neatvnc.h>
#include <pixman.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>
#include "twin_backend.h"
#include "twin_private.h"

#define SCREEN(x) ((twin_context_t *) x)->screen
#define PRIV(x) ((twin_vnc_t *) ((twin_context_t *) x)->priv)

#ifndef DRM_FORMAT_ARGB8888
#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
                 ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DRM_FORMAT_ARGB8888  fourcc_code('A', 'R', '2', '4') 
#endif

typedef struct {
    twin_screen_t *screen;
    struct nvnc *server;
    struct nvnc_display *display;
    struct nvnc_fb_pool *fb_pool;
    struct aml *aml;
    struct aml_handler *aml_handler;
    struct nvnc_fb *current_fb;
    struct pixman_region16 damage_region;
    uint32_t *framebuffer;
    int width;
    int height;
} twin_vnc_t;

struct fb_side_data {
	struct pixman_region16 damage;
};

static void _twin_vnc_put_begin(twin_coord_t left, twin_coord_t top,
    twin_coord_t right, twin_coord_t bottom, void *closure)
{
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    twin_vnc_t *tx = PRIV(closure);
    tx->width = right - left;
    tx->height = bottom - top;
    pixman_region_clear(&tx->damage_region);
}

static void _twin_vnc_put_span(twin_coord_t left,
                               twin_coord_t top,
                               twin_coord_t right,
                               twin_argb32_t *pixels,
                               void *closure)
{
    /*
    twin_screen_t *screen = SCREEN(closure);
    twin_vnc_t *tx = PRIV(closure);
    struct nvnc_fb *fb = nvnc_fb_pool_acquire(tx->fb_pool);
    if (!fb)
        return;

    twin_coord_t width = right - left;
    off_t off = top * screen->width + left;
    uint32_t *dest = (uint32_t *) ((uintptr_t) nvnc_fb_get_addr(fb) +
                                   (off * sizeof(uint32_t)));
    memcpy(dest, pixels, width * sizeof(uint32_t));

    struct pixman_region16 damage;
    pixman_region_init_rect(&damage, left, top, width, 1);
    nvnc_display_feed_buffer(tx->display, fb, &damage);
    pixman_region_fini(&damage);

    nvnc_fb_unref(fb);
    */
    /*
    twin_vnc_t *tx = PRIV(closure);
    struct nvnc_fb *fb;
    fb = nvnc_fb_pool_acquire(tx->fb_pool);
    assert(fb);

    uint32_t *fb_pixels = tx->framebuffer + top * tx->width + left;
    memcpy(fb_pixels, pixels, (right - left) * sizeof(uint32_t));

    struct pixman_region16 span_region;
    pixman_region_init_rect(&span_region, left, top, right - left, 1);
    pixman_region_union(&tx->damage_region, &tx->damage_region, &span_region);
    pixman_region_fini(&span_region);
    */

    twin_vnc_t *tx = PRIV(closure);
    uint32_t *fb_pixels = tx->framebuffer + top * tx->width + left;
    size_t span_width = right - left;
    //printf("%d %d %d\n", top, left, right);

    memcpy(fb_pixels, pixels, span_width * sizeof(uint32_t));

    pixman_region_init_rect(&tx->damage_region, 
                             left, top, span_width, 1);

    if (pixman_region_not_empty(&tx->damage_region)) {
        nvnc_display_feed_buffer(tx->display, tx->current_fb, &tx->damage_region);
        pixman_region_clear(&tx->damage_region);
    }
    aml_poll(tx->aml, 0);
    aml_dispatch(tx->aml);
}

static bool twin_vnc_work(void *closure)
{
    twin_screen_t *screen = SCREEN(closure);
    twin_vnc_t *tx = PRIV(closure);

    if (twin_screen_damaged(screen))
        twin_screen_update(screen);
    /*
    aml_poll(tx->aml, 0);
    aml_dispatch(tx->aml);
    if (pixman_region_not_empty(&tx->damage_region)) {
        nvnc_display_feed_buffer(tx->display, tx->current_fb, &tx->damage_region);
        pixman_region_clear(&tx->damage_region);
    }
    */
    return true;
}

static void vnc_pointer_event(struct nvnc_client *client,
    uint16_t x, uint16_t y, enum nvnc_button_mask button_mask)
{
    twin_vnc_t *tx = nvnc_get_userdata(client);
    twin_event_t event;

    event.kind = TwinEventMotion;
    event.u.pointer.screen_x = x;
    event.u.pointer.screen_y = y;
    event.u.pointer.button = button_mask;

    //twin_screen_dispatch(tx->screen, &event);
}

static void vnc_key_event(struct nvnc_client *client,
    uint32_t keysym, bool is_pressed)
{
    twin_vnc_t *tx = nvnc_get_userdata(client);
    twin_event_t event;

    event.kind = is_pressed ? TwinEventKeyDown : TwinEventKeyUp;
    event.u.key.key = keysym;

    //twin_screen_dispatch(tx->screen, &event);
}

static void vnc_new_client(struct nvnc_client *client)
{
    struct nvnc *server = nvnc_client_get_server(client);
    twin_vnc_t *tx = nvnc_get_userdata(server);

    nvnc_set_userdata(client, tx, NULL);
}

static bool twin_vnc_read_events(int fd, twin_file_op_t op, void *closure)
{
    (void)fd;
    (void)op;
    twin_vnc_t *tx = closure;
    //aml_dispatch(tx->aml);
    return true;
}

twin_context_t *twin_vnc_init(int width, int height)
{
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    if (!ctx)
        return NULL;

    ctx->priv = calloc(1, sizeof(twin_vnc_t));
    if (!ctx->priv) {
        free(ctx);
        return NULL;
    }

    twin_vnc_t *tx = ctx->priv;
    tx->width = width;
    tx->height = height;

    tx->aml = aml_new();
    if (!tx->aml)
        goto bail_priv;
    aml_set_default(tx->aml);
    tx->server = nvnc_open("127.0.0.1", 5900);
    if (!tx->server)
        goto bail_aml;

    tx->display = nvnc_display_new(0, 0);
    if (!tx->display)
        goto bail_server;

    tx->fb_pool = nvnc_fb_pool_new(width, height, DRM_FORMAT_ARGB8888, width);
    if (!tx->fb_pool)
        goto bail_display;

    nvnc_add_display(tx->server, tx->display);
    nvnc_set_name(tx->server, "Twin VNC Backend");
    nvnc_set_new_client_fn(tx->server, vnc_new_client);
    nvnc_set_pointer_fn(tx->server, vnc_pointer_event);
    nvnc_set_key_fn(tx->server, vnc_key_event);
    nvnc_set_userdata(tx->server, tx, NULL);

    //ctx->screen = twin_screen_create(width, height, _twin_vnc_put_begin,
    //                                 _twin_vnc_put_span, ctx);
    ctx->screen = twin_screen_create(width, height, NULL,
                                     _twin_vnc_put_span, ctx);
    if (!ctx->screen)
        goto bail_fb_pool;

    tx->framebuffer = malloc(width * height * sizeof(uint32_t));
    if (!tx->framebuffer)
        goto bail_screen;
    memset(tx->framebuffer, 0xff, width * height * sizeof(uint32_t));

    tx->current_fb = nvnc_fb_from_buffer(tx->framebuffer, width, height,
        DRM_FORMAT_ARGB8888, width );
    if (!tx->current_fb)
        goto bail_framebuffer;

    pixman_region_init_rect(&tx->damage_region, 
                            0, 0,
                            width, height);

    int aml_fd = aml_get_fd(tx->aml);
    //tx->aml_handler = aml_handler_new(aml_get_fd(tx->aml), twin_vnc_work, ctx, NULL);
    twin_set_file(twin_vnc_read_events, aml_fd, TWIN_READ, tx);

    twin_set_work(twin_vnc_work, TWIN_WORK_REDISPLAY, ctx);

    return ctx;

bail_framebuffer:
    free(tx->framebuffer);
bail_screen:
    twin_screen_destroy(ctx->screen);
bail_fb_pool:
    nvnc_fb_pool_unref(tx->fb_pool);
bail_display:
    nvnc_display_unref(tx->display);
bail_server:
    nvnc_close(tx->server);
bail_aml:
    aml_unref(tx->aml);
bail_priv:
    free(ctx->priv);
    free(ctx);
    return NULL;
}
static void twin_vnc_configure(twin_context_t *ctx)
{
    // Implement configuration if needed
}

static void twin_vnc_exit(twin_context_t *ctx)
{
    if (!ctx)
        return;

    twin_vnc_t *tx = PRIV(ctx);

    aml_unref(tx->aml_handler);
    nvnc_fb_pool_unref(tx->fb_pool);
    nvnc_display_unref(tx->display);
    nvnc_close(tx->server);
    aml_unref(tx->aml);

    free(ctx->priv);
    free(ctx);
}

const twin_backend_t g_twin_backend = {
    .init = twin_vnc_init,
    .configure = twin_vnc_configure,
    .exit = twin_vnc_exit,
};
