#pragma once

#include <chrono>
#include <format>
#include <functional>

#include "dear_imgui.hpp"
#include "rule.hpp"

/* Not inline */ static const bool check_version = IMGUI_CHECKVERSION();

inline std::mt19937& global_mt19937() {
    static std::mt19937 rand{(uint32_t)time(0)};
    return rand;
}

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

// Experience in MSVC
// It turns out that there are still a lot of messy encoding problems even if `/utf-8` is specified.
// For example, how is main's argv / fs::path.string() / exception.what() / ... encoded?
// (Well, are there even guarantees that we can get a valid fs::path from main's argv?)
// To make things easy the program does not try to deal with these strings.
#endif

// Provide texture for cells.
class screenT {
    int m_w, m_h;
    ImTextureID m_texture;

public:
    screenT(const screenT&) = delete;
    screenT& operator=(const screenT&) = delete;

    screenT() : m_w{}, m_h{}, m_texture{nullptr} {}

    ~screenT();
    void refresh(int w, int h, std::function<const bool*(int)> getline);

    ImTextureID texture() const { return m_texture; }
};

// Provide texture for codes.
class code_icons {
    ImTextureID m_texture;

public:
    code_icons(const code_icons&) = delete;
    code_icons& operator=(const code_icons&) = delete;

    code_icons();
    ~code_icons();

    void image(legacy::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
               const ImVec4& border_col = ImVec4(0, 0, 0, 0)) const {
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

// Managed by `main`.
bool file_nav_add_special_path(const char* u8path, const char* title);
void frame_main(const code_icons& icons, screenT& screen);

// Managed by `frame_main`.
std::optional<legacy::extrT::valT> load_rule();
std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, const code_icons& icons);
std::optional<legacy::moldT> static_constraints();
std::optional<legacy::moldT::lockT> apply_rule(const legacy::ruleT& rule, screenT& screen);

inline const int item_width = 220;

inline ImVec2 square_size() {
    const float r = ImGui::GetFrameHeight();
    return ImVec2(r, r);
}

inline float wrap_len() {
    // The same as the one in `HelpMarker` in "imgui_demo.cpp".
    return ImGui::GetFontSize() * 35.0f;
}

class sequence {
    enum tagE { None, First, Prev, Next, Last };

    inline static ImGuiID bound_id = 0;

    // (`disable` is a workaround for dec/inc pair in `edit_rule`...)
    static tagE seq(const char* label_first, const char* label_prev, const char* label_next, const char* label_last,
                    bool disable) {
        const ImGuiKey key_prev = ImGuiKey_LeftArrow;
        const ImGuiKey key_next = ImGuiKey_RightArrow;

        tagE tag = None;

        if (ImGui::Button(label_first)) {
            tag = First;
        }

        if (disable) {
            ImGui::BeginDisabled();
        }
        const ImGuiID id_prev = ImGui::GetID(label_prev);
        const ImGuiID id_next = ImGui::GetID(label_next);
        assert(id_prev != 0 && id_next != 0 && id_prev != id_next);
        const bool disabled = imgui_Disabled();
        const bool bound = !disabled && (bound_id == id_prev || bound_id == id_next);

        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (ImGui::Button(label_prev) || (bound && imgui_KeyPressed(key_prev, false))) {
            tag = Prev;
        }
        if (bound) {
            imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_ButtonActive, tag == Prev ? 0.3 : 1.0));
        }
        ImGui::SameLine(0, 0), imgui_Str("/"), ImGui::SameLine(0, 0);
        if (ImGui::Button(label_next) || (bound && imgui_KeyPressed(key_next, false))) {
            tag = Next;
        }
        if (bound) {
            imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_ButtonActive, tag == Next ? 0.3 : 1.0));
        }
        if (disable) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (ImGui::Button(label_last)) {
            tag = Last;
        }

        if (tag != None) {
            bound_id = id_prev;
        }
        return tag;
    }

public:
    // `label` should be either `label_prev` or `label_next`.
    static void bind_to(const char* label) { //
        bound_id = ImGui::GetID(label);
    }

    static void seq(const char* label_first, const char* label_prev, const char* label_next, const char* label_last,
                    const auto& act_first, const auto& act_prev, const auto& act_next, const auto& act_last,
                    bool disable = false) {
        switch (seq(label_first, label_prev, label_next, label_last, disable)) {
            case None: return;
            case First: act_first(); return;
            case Prev: act_prev(); return;
            case Next: act_next(); return;
            case Last: act_last(); return;
        }
    }
};

inline bool button_with_shortcut(const char* label, ImGuiKey key) {
    bool ret = ImGui::Button(label);

    if (!imgui_Disabled()) {
        const bool pressed = imgui_KeyPressed(key, false);
        imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_ButtonActive, pressed ? 0.3 : 1.0));
        ret = ret || pressed;
    }
    return ret;
}

class messenger {
    inline static std::vector<std::string> m_strs{};

public:
    messenger() = delete;

    static void add_msg(std::string str) { m_strs.push_back(std::move(str)); }

    template <class... U>
    static void add_msg(std::format_string<const U&...> fmt, const U&... args) {
        m_strs.push_back(std::format(fmt, args...));
    }

    // Managed by `frame_main`.
    static void display() {
        static bool opened = false;

        if (!opened && !m_strs.empty()) {
            ImGui::OpenPopup("Message");
            opened = true;
        }

        if (ImGui::BeginPopup("Message")) {
            assert(opened && !m_strs.empty());
            ImGui::PushTextWrapPos(wrap_len());
            for (bool first = true; const std::string& str : m_strs) {
                if (!std::exchange(first, false)) {
                    ImGui::Separator();
                }
                imgui_StrCopyable(str, imgui_Str);
            }
            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        } else {
            m_strs.clear();
            opened = false;
        }
    }
};
