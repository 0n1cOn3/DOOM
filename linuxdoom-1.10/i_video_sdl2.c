// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Simplified SDL2 video backend for linuxdoom.
//
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t *video_buffer = NULL;
static uint32_t palette_table[256];

static int ScreenWidth = SCREENWIDTH;
static int ScreenHeight = SCREENHEIGHT;

static int mouse_button_state = 0;

static uint8_t scale_palette_value(uint8_t value)
{
    /* The original palette data tops out around 63; scale for SDL. */
    uint32_t scaled = (uint32_t)value * 4;
    return (scaled > 255u) ? 255u : (uint8_t)scaled;
}

static int sdl_translate_key(SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_LEFT: return KEY_LEFTARROW;
        case SDLK_RIGHT: return KEY_RIGHTARROW;
        case SDLK_DOWN: return KEY_DOWNARROW;
        case SDLK_UP: return KEY_UPARROW;
        case SDLK_ESCAPE: return KEY_ESCAPE;
        case SDLK_RETURN: return KEY_ENTER;
        case SDLK_TAB: return KEY_TAB;
        case SDLK_F1: return KEY_F1;
        case SDLK_F2: return KEY_F2;
        case SDLK_F3: return KEY_F3;
        case SDLK_F4: return KEY_F4;
        case SDLK_F5: return KEY_F5;
        case SDLK_F6: return KEY_F6;
        case SDLK_F7: return KEY_F7;
        case SDLK_F8: return KEY_F8;
        case SDLK_F9: return KEY_F9;
        case SDLK_F10: return KEY_F10;
        case SDLK_F11: return KEY_F11;
        case SDLK_F12: return KEY_F12;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_PAUSE: return KEY_PAUSE;
        case SDLK_EQUALS: return KEY_EQUALS;
        case SDLK_KP_EQUALS: return KEY_EQUALS;
        case SDLK_MINUS: return KEY_MINUS;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: return KEY_RSHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL: return KEY_RCTRL;
        case SDLK_LALT:
        case SDLK_RALT: return KEY_RALT;
        default:
            if (key >= SDLK_SPACE && key <= SDLK_z)
            {
                return (int)key;
            }
            return 0;
    }
}

void I_ShutdownGraphics(void)
{
    if (video_buffer)
    {
        free(video_buffer);
        video_buffer = NULL;
    }

    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = NULL;
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

void I_InitGraphics(void)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        I_Error("SDL video init failed: %s", SDL_GetError());
    }

    window = SDL_CreateWindow(
        "Doom",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ScreenWidth * 2,
        ScreenHeight * 2,
        SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        I_Error("SDL window creation failed: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        I_Error("SDL renderer creation failed: %s", SDL_GetError());
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ScreenWidth, ScreenHeight);
    if (!texture)
    {
        I_Error("SDL texture creation failed: %s", SDL_GetError());
    }

    video_buffer = (uint32_t *)malloc(ScreenWidth * ScreenHeight * sizeof(uint32_t));
    if (!video_buffer)
    {
        I_Error("Failed to allocate video buffer");
    }

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void I_UpdateNoBlit(void)
{
    /* SDL2 backend blits directly during I_FinishUpdate. */
}

void I_FinishUpdate(void)
{
    if (!screens[0])
    {
        return;
    }

    for (int y = 0; y < ScreenHeight; ++y)
    {
        for (int x = 0; x < ScreenWidth; ++x)
        {
            uint8_t index = screens[0][y * ScreenWidth + x];
            video_buffer[y * ScreenWidth + x] = palette_table[index];
        }
    }

    SDL_UpdateTexture(texture, NULL, video_buffer, ScreenWidth * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void I_ReadScreen(byte *scr)
{
    memcpy(scr, screens[0], ScreenWidth * ScreenHeight);
}

void I_SetPalette(byte *palette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r = gammatable[usegamma][scale_palette_value(palette[3 * i])];
        uint8_t g = gammatable[usegamma][scale_palette_value(palette[3 * i + 1])];
        uint8_t b = gammatable[usegamma][scale_palette_value(palette[3 * i + 2])];

        palette_table[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
}

void I_StartFrame(void)
{
    // SDL_PumpEvents() is not needed here because SDL_PollEvent() in I_StartTic() already pumps events.
}

void I_StartTic(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                I_Quit();
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                int key = sdl_translate_key(event.key.keysym.sym);
                if (key)
                {
                    event_t doom_event;
                    doom_event.type = (event.type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;
                    doom_event.data1 = key;
                    doom_event.data2 = 0;
                    doom_event.data3 = 0;
                    D_PostEvent(&doom_event);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                event_t doom_event;
                doom_event.type = ev_mouse;
                
                // Update button state - set the bit for this button
                if (event.button.button == SDL_BUTTON_LEFT)
                    mouse_button_state |= 1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_state |= 2;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    mouse_button_state |= 4;
                
                doom_event.data1 = mouse_button_state;
                doom_event.data2 = 0;
                doom_event.data3 = 0;
                D_PostEvent(&doom_event);
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                event_t doom_event;
                doom_event.type = ev_mouse;
                
                // Update button state - clear the bit for this button
                if (event.button.button == SDL_BUTTON_LEFT)
                    mouse_button_state &= ~1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_state &= ~2;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    mouse_button_state &= ~4;
                
                doom_event.data1 = mouse_button_state;
                doom_event.data2 = 0;
                doom_event.data3 = 0;
                D_PostEvent(&doom_event);
                break;
            }
            case SDL_MOUSEMOTION:
            {
                event_t doom_event;
                doom_event.type = ev_mouse;
                doom_event.data1 = mouse_button_state;
                // Scale mouse movement like X11 backend (shift left by 2)
                doom_event.data2 = event.motion.xrel << 2;
                doom_event.data3 = -event.motion.yrel << 2;  // Invert Y axis
                D_PostEvent(&doom_event);
                break;
            }
            default:
                break;
        }
    }
}
