#pragma once

#include <chrono>
#include <format>
#include <functional>

#include "app_imgui.hpp"
#include "rule.hpp"

#if 0
// Enforce that ordinary string literals are encoded with utf-8.
// This requires certain compiler flags to be set (e.g. `/utf-8` in MSVC).
// Currently not necessary, as the program is not using non-ascii characters.
// (u8"..." is not usable in this project, as it becomes `char8_t[]` since C++20.)
inline void assert_utf8_encoding() {
    constexpr auto a = std::to_array("中文");
    constexpr auto b = std::to_array(u8"中文");

    static_assert(std::equal(a.begin(), a.end(), b.begin(), b.end(), [](auto l, auto r) {
        return static_cast<unsigned char>(l) == static_cast<unsigned char>(r);
    }));
}
#endif

using std::chrono_literals::operator""ms;
using clockT = std::chrono::steady_clock;

inline std::mt19937& global_mt19937() {
    static std::mt19937 rand(time(0));
    return rand;
}

// TODO: look for better names for tile_image, code_image, edit_tile (and edit_tile.cpp)
// (especially as "tile.hpp" is now hidden as impl details for "edit_tile.cpp")
class tile_image {
    int m_w, m_h;
    ImTextureID m_texture;

public:
    tile_image(const tile_image&) = delete;
    tile_image& operator=(const tile_image&) = delete;

    tile_image() : m_w{}, m_h{}, m_texture{nullptr} {}

    ~tile_image();
    // TODO: explain
    void update(int w, int h, std::function<const bool*(int)> getline);

    ImTextureID texture() const { return m_texture; }
};

class code_image {
    ImTextureID m_texture;

public:
    code_image(const code_image&) = delete;
    code_image& operator=(const code_image&) = delete;

    code_image();
    ~code_image();

    void image(legacy::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
               const ImVec4 border_col = ImVec4(0, 0, 0, 0)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::Image(m_texture, size, uv0, uv1, tint_col, border_col);
    }

    bool button(legacy::codeT code, int zoom, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
                const ImVec4& tint_col = ImVec4(1, 1, 1, 1)) const {
        const ImVec2 size(3 * zoom, 3 * zoom);
        const ImVec2 uv0(0, code * (1.0f / 512));
        const ImVec2 uv1(1, (code + 1) * (1.0f / 512));
        ImGui::PushID(code);
        const bool hit = ImGui::ImageButton("Code", m_texture, size, uv0, uv1, bg_col, tint_col);
        ImGui::PopID();
        return hit;
    }
};

// TODO: this workaround is ugly...
bool file_nav_add_special_path(const char* u8path, const char* title);
std::optional<legacy::moldT> load_rule(const legacy::moldT& test_sync);

std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_image& icons);

std::optional<legacy::moldT::lockT> edit_tile(const legacy::ruleT& rule, tile_image& img);

// TODO: better name...
// TODO: or add back to main.cpp? or directly define in this header?
void frame(const code_image& icons, tile_image& img);

// TODO: rename...
const int FixedItemWidth = 220;

// TODO: support rollbacking diff rules?
// TODO: for editing opt, support in-lock and outof-lock mode?
// TODO: Right-click must either to open a submenu, or to toggle on/off the tooltip.

// TODO: better name...
inline ImVec2 square_size() {
    const float r = ImGui::GetFrameHeight();
    return ImVec2(r, r);
}

// TODO: reconsider binding and scrolling logic...
inline void iter_pair(const char* tag_first, const char* tag_prev, const char* tag_next, const char* tag_last,
                      auto act_first, auto act_prev, auto act_next, auto act_last, bool allow_binding = true,
                      bool allow_scrolling = true) {
    if (ImGui::Button(tag_first)) {
        act_first();
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    // TODO: _Left, _Right to toggle?
    if (allow_binding) {
        if (imgui_enterbutton(tag_prev)) {
            act_prev();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (imgui_enterbutton(tag_next)) {
            act_next();
        }
    } else {
        if (ImGui::Button(tag_prev)) {
            act_prev();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button(tag_next)) {
            act_next();
        }
    }

    ImGui::EndGroup();
    if (allow_scrolling && ImGui::IsItemHovered()) {
        if (imgui_scrollup()) {
            act_prev();
        } else if (imgui_scrolldown()) {
            act_next();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(tag_last)) {
        act_last();
    }
};

// TODO: better name...
class messenger {
    inline static std::vector<std::string> m_strs{};

public:
    messenger() = delete;

    template <class... U>
    static void add_msg(std::format_string<const U&...> fmt, const U&... args) noexcept {
        m_strs.emplace_back(std::format(fmt, args...));
    }

    static void display() {
        static bool opened = false;

        if (!opened && !m_strs.empty()) {
            ImGui::OpenPopup("Message");
            opened = true;
        }

        if (opened) {
            if (ImGui::BeginPopup("Message")) {
                for (const std::string& str : m_strs) {
                    imgui_str(str);
                }
                ImGui::EndPopup();
            } else {
                m_strs.clear();
                opened = false;
            }
        }
    }
};
