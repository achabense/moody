#pragma once

#include <SDL.h>
#include <cassert>
#include <imgui.h>

#include "imgui_sdl2/imgui_impl_sdl2.h"
#include "imgui_sdl2/imgui_impl_sdlrenderer2.h"
#include "tile.hpp"

// TODO: problematic...
class tile_image {
    int m_w, m_h;
    std::vector<Uint32> m_pixels;
    SDL_Texture* m_texture;

public:
    tile_image() = delete;
    tile_image(SDL_Renderer* renderer, const legacy::tileT& tile)
        : m_w(tile.width()), m_h(tile.height()), m_pixels(tile.width() * tile.height()) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
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

    void update(const legacy::tileT& tile) {
        auto [h, w] = tile.shape();
        assert(w == m_w && h == m_h);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                m_pixels[y * m_w + x] = tile.at(y, x) ? -1 /* white */ : 0;
            }
        }
        SDL_UpdateTexture(m_texture, NULL, m_pixels.data(), m_w * sizeof(Uint32));
    }

    SDL_Texture* texture() {
        return m_texture;
    }
};
