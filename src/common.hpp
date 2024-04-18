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

// Managed by `main`.
bool set_home(const char* u8path = nullptr); // nullptr ~ filesystem::current_path.
void frame_main();

// Managed by `frame_main`.
std::optional<legacy::extrT::valT> load_file();
std::optional<legacy::extrT::valT> load_clipboard();
std::optional<legacy::extrT::valT> load_doc();
// (`bind_undo` is a workaround to allow for binding to undo/redo for certain operations.)
std::optional<legacy::moldT> edit_rule(const legacy::moldT& mold, bool& bind_undo);
std::optional<legacy::moldT> static_constraints();
std::optional<legacy::moldT::lockT> apply_rule(const legacy::ruleT& rule);

// Returns a texture with width/height exactly = w/h, for the (cell) data represented by `getline`.
// There must be: getline(0...h-1) -> bool[w].
// The texture is only valid for the current frame.
[[nodiscard]] ImTextureID make_screen(int w, int h, std::function<const bool*(int)> getline);

// ImGui::Image and ImGui::ImageButton for `codeT`.
void code_image(legacy::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
                const ImVec4& border_col = ImVec4(0, 0, 0, 0));
bool code_button(legacy::codeT code, int zoom, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
                 const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

inline const int item_width = 220;

inline ImVec2 square_size() {
    const float r = ImGui::GetFrameHeight();
    return ImVec2(r, r);
}

inline float wrap_len() {
    // The same as the one in `HelpMarker` in "imgui_demo.cpp".
    return ImGui::GetFontSize() * 35.0f;
}

// TODO: whether to support shortcut in `imgui_StepSliderInt`?
inline bool button_with_shortcut(const char* label, ImGuiKey shortcut = ImGuiKey_None, const ImVec2& size = {}) {
    bool ret = ImGui::Button(label, size);
    if (shortcut != ImGuiKey_None && !imgui_TestItemFlag(ImGuiItemFlags_Disabled)) {
        const bool pressed = imgui_KeyPressed(shortcut, imgui_TestItemFlag(ImGuiItemFlags_ButtonRepeat));
        imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_ButtonActive, pressed ? 0.3 : 1.0));
        ret = ret || pressed;
    }
    return ret;
}

class sequence {
    enum tagE { None, First, Prev, Next, Last };

    inline static ImGuiID bound_id = 0;

    // (`disable` is a workaround for dec/inc pair in `edit_rule`...)
    static tagE seq(const char* label_first, const char* label_prev, const char* label_next, const char* label_last,
                    bool disable) {
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
        const bool disabled = imgui_TestItemFlag(ImGuiItemFlags_Disabled);
        const bool bound = !disabled && (bound_id == id_prev || bound_id == id_next);
        const ImGuiKey shortcut_prev = bound ? ImGuiKey_LeftArrow : ImGuiKey_None;
        const ImGuiKey shortcut_next = bound ? ImGuiKey_RightArrow : ImGuiKey_None;

        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (button_with_shortcut(label_prev, shortcut_prev)) {
            tag = Prev;
        }
        ImGui::SameLine(0, 0), imgui_Str("/"), ImGui::SameLine(0, 0);
        if (button_with_shortcut(label_next, shortcut_next)) {
            tag = Next;
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
    // `id` should be the same as one of prev/next button.
    static void bind_to(ImGuiID id) { bound_id = id; }

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

        ImGui::SetNextWindowSizeConstraints({}, ImVec2(FLT_MAX, ImGui::GetTextLineHeight() * 30));
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

// Preview rules.
class previewer {
public:
    class configT {
    public:
        enum sizeE : int { _160_160 = 0, _220_160, _220_220, _280_220, Count };

    private:
        static constexpr const char* size_labels[Count]{"160*160", "220*160", "220*220", "280*220"};
        static constexpr int size_w[Count]{160, 220, 220, 280};
        static constexpr int size_h[Count]{160, 160, 220, 220};

        friend previewer;
        sizeE size; // Cannot be `Count`.
        int seed;
        int pace;
        void _set();

        // (Workaround: managed by configT for convenience.)
        bool restart = false;

    public:
        /*implicit*/ configT(sizeE size) : size(size), seed(0), pace(1) { assert(size != Count); }

        int width() const { return size_w[size]; }
        int height() const { return size_h[size]; }

        void set(const char* label, const char* label_restart) {
            ImGui::Button(label);
            if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
                _set();
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            restart = ImGui::Button(label_restart);
            imgui_ItemTooltip("For individual preview windows:\n"
                              "Left-press to pause; right-click to restart.");
        }
    };

    static void preview(uint32_t id, const configT& config, const legacy::ruleT& rule, bool interactive = true) {
        ImGui::Dummy(ImVec2(config.width(), config.height()));
        if (ImGui::IsItemVisible()) {
            _preview((uint64_t(ImGui::GetID("")) << 32) | id, config, rule, interactive);
        }
    }

    static void preview(uint32_t id, const configT& config, const std::invocable<> auto& get_rule,
                        bool interactive = true) {
        ImGui::Dummy(ImVec2(config.width(), config.height()));
        if (ImGui::IsItemVisible()) {
            _preview((uint64_t(ImGui::GetID("")) << 32) | id, config, get_rule(), interactive);
        }
    }

private:
    static void _preview(uint64_t id, const configT& config, const legacy::ruleT& rule, bool interactive);
};

// Ensure that users won't touch this feature unexpectedly.
// !!TODO: recheck every tooltip etc that refers to lock but is outside of `display`.
class manage_lock {
    inline static bool open = false, ever_opened = false;

public:
    // Managed by `frame_main`.
    static void checkbox() {
        const ImVec4 col(1, 1, ever_opened ? 0.5 : 1, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Checkbox("Lock & capture", &manage_lock::open);
        ImGui::PopStyleColor();
        if (open) {
            ever_opened = true;
        }
    }

    static bool enabled() { return ever_opened; }
    static void display(const std::invocable<> auto& append) {
        if (open) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            if (auto window = imgui_Window("Lock & capture", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
                append();
            }
        }
    }
};
