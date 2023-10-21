#pragma once

#include <SDL.h>
#include <cassert>

#include "tile.hpp"

// TODO: pixel format might be problematic...
// TODO: in namespace or not? better name?
class tile_image {
    int m_w, m_h;
    std::unique_ptr<Uint32[]> m_pixels;

    SDL_Texture* m_texture; // owning.

public:
    tile_image(SDL_Renderer* renderer, const legacy::tileT& tile)
        : m_w(tile.width()), m_h(tile.height()), m_pixels(new Uint32[tile.area()]{}) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
        update(tile);
    }

    ~tile_image() {
        SDL_DestroyTexture(m_texture);
    }

    tile_image(const tile_image&) = delete;
    tile_image& operator=(const tile_image&) = delete;

    int width() const {
        return m_w;
    }
    int height() const {
        return m_h;
    }

    SDL_Texture* texture() {
        return m_texture;
    }

    void update(const legacy::tileT& tile) {
        int w = tile.width(), h = tile.height();
        assert(w == m_w && h == m_h);

        for (int y = 0; y < h; ++y) {
            const bool* line = tile.line(y);
            for (int x = 0; x < w; ++x) {
                m_pixels[y * m_w + x] = line[x] ? -1 /* white */ : 0;
            }
        }
        SDL_UpdateTexture(m_texture, NULL, m_pixels.get(), m_w * sizeof(Uint32));
    }
};
