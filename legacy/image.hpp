#pragma once

#include <SDL.h>
#include <cassert>

#include "tile.hpp"

// TODO: pixel format might be problematic...
// TODO: in namespace or not? better name?
// TODO: able to deal with resized tile...
// TODO: should not update when paused...
class tile_image {
    int m_w, m_h;

    SDL_Texture* m_texture; // owning.

public:
    tile_image(SDL_Renderer* renderer, const legacy::tileT& tile) : m_w(tile.width()), m_h(tile.height()) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
        update(tile);
    }

    ~tile_image() {
        // TODO: this is (horribly) happening after renderer being destroyed...
        SDL_DestroyTexture(m_texture);
    }

    tile_image(const tile_image&) = delete;
    tile_image& operator=(const tile_image&) = delete;

    void update(const legacy::tileT& tile) {
        assert(tile.width() == m_w && tile.height() == m_h);

        void* pixels = NULL;
        int pitch = 0;
        bool successful = SDL_LockTexture(m_texture, NULL, &pixels, &pitch) == 0;
        assert(successful && pitch == m_w * sizeof(Uint32));

        for (int y = 0; y < m_h; ++y) {
            const bool* line = tile.line(y);
            for (int x = 0; x < m_w; ++x) {
                ((Uint32*)pixels)[y * m_w + x] = line[x] ? -1 /* white */ : 0;
            }
        }
        SDL_UnlockTexture(m_texture);
    }

    int width() const {
        return m_w;
    }
    int height() const {
        return m_h;
    }

    SDL_Texture* texture() {
        return m_texture;
    }
};

// TODO: looks horrible...
class code_image {
    SDL_Texture* m_texture;

public:
    code_image(SDL_Renderer* renderer) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width(), height());
        Uint32 pixels[3 * 3 * 512];
        for (int code = 0; code < 512; ++code) {
            auto [q, w, e, a, s, d, z, x, c] = legacy::decode(code);
            bool fill[3][3] = {{q, w, e}, {a, s, d}, {z, x, c}};
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 3; ++x) {
                    pixels[(code * 3 + y) * 3 + x] = fill[y][x] ? -1 : 0;
                }
            }
        }

        SDL_UpdateTexture(m_texture, NULL, pixels, width() * sizeof(Uint32));
    }

    ~code_image() {
        // TODO: do the renderer need to be living at this point?
        SDL_DestroyTexture(m_texture);
    }

    static int width() {
        return 3;
    }

    static int height() {
        return 3 * 512;
    }

    SDL_Texture* texture() {
        return m_texture;
    }
};
