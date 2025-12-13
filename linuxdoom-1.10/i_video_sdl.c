// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// SDL2 video backend for linuxdoom.
//
//-----------------------------------------------------------------------------

#include <SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *frame_texture = NULL;
static boolean mouse_grab = false;
static int window_scale = 2;
static int window_width = 0;
static int window_height = 0;

static uint32_t argb_palette[256];
static uint32_t *framebuffer32 = NULL;

extern boolean devparm;
extern byte gammatable[5][256];
extern int usegamma;

static int SdlButtonState(Uint32 state)
{
    int buttons = 0;

    if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
        buttons |= 1;
    if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        buttons |= 2;
    if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        buttons |= 4;

    return buttons;
}

static int SdlKeyToDoomKey(const SDL_Keysym *keysym)
{
    switch (keysym->scancode)
    {
      case SDL_SCANCODE_LEFT:
        return KEY_LEFTARROW;
      case SDL_SCANCODE_RIGHT:
        return KEY_RIGHTARROW;
      case SDL_SCANCODE_DOWN:
        return KEY_DOWNARROW;
      case SDL_SCANCODE_UP:
        return KEY_UPARROW;
      case SDL_SCANCODE_ESCAPE:
        return KEY_ESCAPE;
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER:
        return KEY_ENTER;
      case SDL_SCANCODE_TAB:
        return KEY_TAB;
      case SDL_SCANCODE_F1:
        return KEY_F1;
      case SDL_SCANCODE_F2:
        return KEY_F2;
      case SDL_SCANCODE_F3:
        return KEY_F3;
      case SDL_SCANCODE_F4:
        return KEY_F4;
      case SDL_SCANCODE_F5:
        return KEY_F5;
      case SDL_SCANCODE_F6:
        return KEY_F6;
      case SDL_SCANCODE_F7:
        return KEY_F7;
      case SDL_SCANCODE_F8:
        return KEY_F8;
      case SDL_SCANCODE_F9:
        return KEY_F9;
      case SDL_SCANCODE_F10:
        return KEY_F10;
      case SDL_SCANCODE_F11:
        return KEY_F11;
      case SDL_SCANCODE_F12:
        return KEY_F12;
      case SDL_SCANCODE_BACKSPACE:
      case SDL_SCANCODE_DELETE:
        return KEY_BACKSPACE;
      case SDL_SCANCODE_PAUSE:
        return KEY_PAUSE;
      case SDL_SCANCODE_EQUALS:
      case SDL_SCANCODE_KP_EQUALS:
        return KEY_EQUALS;
      case SDL_SCANCODE_MINUS:
      case SDL_SCANCODE_KP_MINUS:
        return KEY_MINUS;
      case SDL_SCANCODE_LSHIFT:
      case SDL_SCANCODE_RSHIFT:
        return KEY_RSHIFT;
      case SDL_SCANCODE_LCTRL:
      case SDL_SCANCODE_RCTRL:
        return KEY_RCTRL;
      case SDL_SCANCODE_LALT:
      case SDL_SCANCODE_LGUI:
      case SDL_SCANCODE_RALT:
      case SDL_SCANCODE_RGUI:
        return KEY_RALT;
      default:
        break;
    }

    if (keysym->sym >= SDLK_SPACE && keysym->sym <= SDLK_z)
    {
        int c = (int) keysym->sym;
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        return c;
    }

    return 0;
}

static void UpdateTargetDimensions(void)
{
    SDL_GetRendererOutputSize(renderer, &window_width, &window_height);
    if (window_width <= 0)
        window_width = 1;
    if (window_height <= 0)
        window_height = 1;
}

static SDL_Rect ComputeTargetRect(void)
{
    SDL_Rect target;
    const float target_aspect = 4.0f / 3.0f;
    float window_aspect = (float) window_width / (float) window_height;

    if (window_aspect > target_aspect)
    {
        target.h = window_height;
        target.w = (int) (target_aspect * (float) window_height);
        target.x = (window_width - target.w) / 2;
        target.y = 0;
    }
    else
    {
        target.w = window_width;
        target.h = (int) ((float) window_width / target_aspect);
        target.x = 0;
        target.y = (window_height - target.h) / 2;
    }

    return target;
}

static void EnsureFramebuffer(void)
{
    if (framebuffer32)
        return;

    framebuffer32 = malloc(sizeof(uint32_t) * SCREENWIDTH * SCREENHEIGHT);
    if (!framebuffer32)
        I_Error("Failed to allocate framebuffer");
}

static void CreateRendererObjects(void)
{
    if (frame_texture)
        SDL_DestroyTexture(frame_texture);

    frame_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREENWIDTH,
        SCREENHEIGHT);

    if (!frame_texture)
        I_Error("Could not create SDL texture: %s", SDL_GetError());

    UpdateTargetDimensions();
}

void I_ShutdownGraphics(void)
{
    if (framebuffer32)
    {
        free(framebuffer32);
        framebuffer32 = NULL;
    }

    if (frame_texture)
    {
        SDL_DestroyTexture(frame_texture);
        frame_texture = NULL;
    }

    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void I_StartFrame (void)
{
    // Nothing needed; SDL event handling occurs in I_StartTic.
}

static void PostMouseEvent(int buttons, int dx, int dy)
{
    event_t event;
    event.type = ev_mouse;
    event.data1 = buttons;
    event.data2 = dx << 2;
    event.data3 = -dy << 2;
    D_PostEvent(&event);
}

static void HandleEvent(const SDL_Event *ev)
{
    switch (ev->type)
    {
      case SDL_QUIT:
        I_Quit();
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        {
            int key = SdlKeyToDoomKey(&ev->key.keysym);
            if (key)
            {
                event_t event;
                event.type = (ev->type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;
                event.data1 = key;
                event.data2 = event.data3 = 0;
                D_PostEvent(&event);
            }
        }
        break;
      case SDL_MOUSEMOTION:
        PostMouseEvent(SdlButtonState(ev->motion.state), ev->motion.xrel, ev->motion.yrel);
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        {
            int buttons = SdlButtonState(SDL_GetMouseState(NULL, NULL));
            event_t event;
            event.type = ev_mouse;
            event.data1 = buttons;
            event.data2 = event.data3 = 0;
            D_PostEvent(&event);
        }
        break;
      case SDL_WINDOWEVENT:
        if (ev->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            UpdateTargetDimensions();
        break;
      default:
        break;
    }
}

void I_StartTic (void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
        HandleEvent(&ev);
}

void I_UpdateNoBlit (void)
{
}

void I_FinishUpdate (void)
{
    int y;
    EnsureFramebuffer();

    if (devparm)
    {
        static int lasttic;
        int tics = I_GetTime() - lasttic;
        int i;

        lasttic += tics;
        if (tics > 20)
            tics = 20;

        for (i = 0; i < tics * 2; i += 2)
            screens[0][(SCREENHEIGHT - 1) * SCREENWIDTH + i] = 0xff;
        for (; i < 20 * 2; i += 2)
            screens[0][(SCREENHEIGHT - 1) * SCREENWIDTH + i] = 0x0;
    }

    for (y = 0; y < SCREENHEIGHT; ++y)
    {
        int x;
        for (x = 0; x < SCREENWIDTH; ++x)
        {
            framebuffer32[y * SCREENWIDTH + x] = argb_palette[screens[0][y * SCREENWIDTH + x]];
        }
    }

    if (SDL_UpdateTexture(frame_texture, NULL, framebuffer32, SCREENWIDTH * (int) sizeof(uint32_t)) < 0)
        I_Error("SDL_UpdateTexture failed: %s", SDL_GetError());

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_Rect target = ComputeTargetRect();
    SDL_RenderCopy(renderer, frame_texture, NULL, &target);
    SDL_RenderPresent(renderer);
}

void I_WaitVBL(int count)
{
    SDL_Delay((Uint32) (count * (1000 / 70)));
}

void I_ReadScreen (byte* scr)
{
    memcpy(scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetPalette (byte* palette)
{
    int i;
    for (i = 0; i < 256; ++i)
    {
        byte r = gammatable[usegamma][*palette++];
        byte g = gammatable[usegamma][*palette++];
        byte b = gammatable[usegamma][*palette++];
        argb_palette[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
}

static void ConfigureMouse(void)
{
    SDL_SetWindowGrab(window, mouse_grab ? SDL_TRUE : SDL_FALSE);
    SDL_SetRelativeMouseMode(mouse_grab ? SDL_TRUE : SDL_FALSE);
}

void I_InitGraphics(void)
{
    int render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

    if (renderer)
        return;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        I_Error("Could not initialize SDL video: %s", SDL_GetError());

    if (M_CheckParm("-2"))
        window_scale = 2;
    if (M_CheckParm("-3"))
        window_scale = 3;
    if (M_CheckParm("-4"))
        window_scale = 4;

    mouse_grab = !!M_CheckParm("-grabmouse");

    window = SDL_CreateWindow(
        "linuxdoom",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREENWIDTH * window_scale,
        SCREENHEIGHT * window_scale,
        SDL_WINDOW_RESIZABLE);

    if (!window)
        I_Error("Could not create SDL window: %s", SDL_GetError());

    renderer = SDL_CreateRenderer(window, -1, render_flags);
    if (!renderer)
    {
        render_flags = SDL_RENDERER_SOFTWARE;
        renderer = SDL_CreateRenderer(window, -1, render_flags);
        if (!renderer)
            I_Error("Could not create SDL renderer: %s", SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    ConfigureMouse();
    CreateRendererObjects();
}
