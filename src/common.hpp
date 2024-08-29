#pragma once

#include <chrono>
#include <format>
#include <functional>

#include "rule.hpp"

#include "dear_imgui.hpp"

/* Not inline */ static const bool check_version = IMGUI_CHECKVERSION();

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

inline std::mt19937& global_mt19937() {
    static std::mt19937 rand{(uint32_t)time(0)};
    return rand;
}

// Managed by `main`.
bool set_home(const char* u8path = nullptr); // nullptr ~ filesystem::current_path.
void frame_main();

// Managed by `frame_main`.
class sync_point;
void load_file(sync_point&);
void load_clipboard(sync_point&);
void load_doc(sync_point&);
// (`bind_undo` is a workaround to allow for binding to undo/redo for certain operations.)
void edit_rule(sync_point&, bool& bind_undo);
void static_constraints(sync_point&);
void apply_rule(sync_point&);

struct rule_algo {
    static aniso::moldT trans_reverse(const aniso::moldT&);
    static bool is_hexagonal_rule(const aniso::ruleT&);
};

// "tile_base.hpp"
namespace aniso::_misc {
    template <class>
    struct tile_ref_;
} // namespace aniso::_misc

// The texture is only valid for the current frame.
enum class scaleE { Nearest, Linear };
[[nodiscard]] ImTextureID make_screen(aniso::_misc::tile_ref_<const bool> tile, scaleE scale);

// ImGui::Image and ImGui::ImageButton for `codeT`.
void code_image(aniso::codeT code, int zoom, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
                const ImVec4& border_col = ImVec4(0, 0, 0, 0));
bool code_button(aniso::codeT code, int zoom, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
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

// TODO: consider using ImGui::Shortcut?
// Some features cannot easily be satisfied with `ImGui::Shortcut` and `ImGui::SetNextItemShortcut`.
// !!TODO: whether to focus item/window?
struct shortcuts {
    static bool global_flag(ImGuiKey key) { //
        return !ImGui::GetIO().WantTextInput && ImGui::IsKeyDown(key);
    }

    static bool keys_avail() {
        // (`!IsAnyItemActive` seems to be implied by `!WantCaptureKeyboard`; added anyway.)
        return !ImGui::GetIO().WantCaptureKeyboard && !ImGui::IsAnyItemActive();
    }

    static bool keys_avail_and_window_hoverable() { // Not blocked by popup.
        return keys_avail() && imgui_IsWindowHoverable();
    }

    static bool test(ImGuiKey key, bool repeat = false) {
        assert(key != ImGuiKey_None);
        {
            // Resolve shortcut competition when multiple keys are pressed.
            static unsigned latest = ImGui::GetFrameCount();
            static bool occupied = false;
            if (const unsigned frame = ImGui::GetFrameCount(); frame != latest) {
                latest = frame;
                occupied = false;
            }
            if (occupied) {
                return false;
            }
            if (ImGui::IsKeyDown(key)) {
                occupied = true;
            }
        }
        return ImGui::IsKeyPressed(key, repeat);
    }

    static bool highlight(ImGuiID id = 0) {
        if (id == 0) {
            id = ImGui::GetItemID();
        }
        ImGui::NavHighlightActivated(id);
        return true;
    }

    // Should be called after the item.
    static bool item_shortcut(ImGuiKey key, bool repeat = false, std::optional<bool> cond = std::nullopt) {
        if (key != ImGuiKey_None && !imgui_TestItemFlag(ImGuiItemFlags_Disabled)) {
            if (cond.has_value() ? *cond : keys_avail_and_window_hoverable()) { // `value_or` won't short-circuit.
                return test(key, repeat) && highlight();
            }
        }
        return false;
    }
};

inline void quick_info(std::string_view msg) {
    if (shortcuts::global_flag(ImGuiKey_H) && ImGui::IsItemVisible()) {
        imgui_ItemRect(IM_COL32_WHITE);

        assert(!msg.empty());
        const char *const text_beg = msg.data(), *const text_end = msg.data() + msg.size();
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const ImVec2 msg_size = ImGui::CalcTextSize(text_beg, text_end) + padding * 2;
        const ImVec2 msg_min = [&]() -> ImVec2 {
            const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            if (msg[0] == '<') {
                return ImVec2(max.x + ImGui::GetStyle().ItemInnerSpacing.x, min.y);
            } else if (msg[0] == '^') {
                return ImVec2(min.x, max.y + ImGui::GetStyle().ItemSpacing.y);
            } else {
                assert(msg[0] == 'v');
                return ImVec2(min.x, min.y - ImGui::GetStyle().ItemSpacing.y - msg_size.y);
            }
        }();
        const ImVec2 msg_max = msg_min + msg_size;
        // TODO: ideally, the message should be rendered on the foreground of individual windows...
        ImDrawList* const drawlist = ImGui::GetForegroundDrawList();
        drawlist->AddRectFilled(msg_min, msg_max, IM_COL32_GREY(48, 255));
        drawlist->AddRect(msg_min, msg_max, IM_COL32_WHITE);
        drawlist->AddText(msg_min + padding, IM_COL32_WHITE, text_beg, text_end);
    }
}

// There is intended to be at most one call to this function in each window hierarchy.
inline void set_scroll_by_up_down(float dy) {
    // TODO: are there easy ways to query "is current window or its parent windows focused"?
    const ImGuiFocusedFlags flags = ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy;
    if (shortcuts::keys_avail() && ImGui::IsWindowFocused(flags)) {
        if (shortcuts::test(ImGuiKey_DownArrow, true)) {
            ImGui::SetScrollY(ImGui::GetScrollY() + dy);
            shortcuts::highlight(ImGui::GetWindowScrollbarID(ImGui::GetCurrentWindowRead(), ImGuiAxis_Y));
        } else if (shortcuts::test(ImGuiKey_UpArrow, true)) {
            ImGui::SetScrollY(ImGui::GetScrollY() - dy);
            shortcuts::highlight(ImGui::GetWindowScrollbarID(ImGui::GetCurrentWindowRead(), ImGuiAxis_Y));
        }
    }
}

// (Referring to ImGui::BeginPopupContextItem().)
inline bool begin_popup_for_item(bool open, const char* str_id = nullptr) {
    if (GImGui->CurrentWindow->SkipItems) {
        return false;
    }
    const ImGuiID id = str_id ? ImGui::GetID(str_id) : ImGui::GetItemID();
    assert(id != 0);
    if (open) { // Instead of MouseReleased && ItemHovered
        ImGui::OpenPopupEx(id);
    }
    return ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoSavedSettings);
}

class sequence {
    static bool button_with_shortcut(const char* label, ImGuiKey shortcut = ImGuiKey_None) {
        bool ret = ImGui::Button(label);
        if (shortcut != ImGuiKey_None && !imgui_TestItemFlag(ImGuiItemFlags_Disabled)) {
            imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_ButtonActive, 1.0f));
            if (!ret && shortcuts::item_shortcut(shortcut)) {
                ret = true;
            }
        }
        return ret;
    }

    enum tagE { None, First, Prev, Next, Last };

    inline static ImGuiID bound_id = 0;

    // (`disable` is a workaround for a sequence in `edit_rule`...)
    static tagE seq(const char* label_first, const char* label_prev, const char* label_next, const char* label_last,
                    const char* const disable) {
        tagE tag = None;

        if (ImGui::Button(label_first)) {
            tag = First;
        }
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());

        if (disable) {
            ImGui::BeginDisabled();
            ImGui::BeginGroup();
        }
        const ImGuiID id_prev = ImGui::GetID(label_prev);
        const ImGuiID id_next = ImGui::GetID(label_next);
        assert(id_prev != 0 && id_next != 0 && id_prev != id_next);
        const bool disabled = imgui_TestItemFlag(ImGuiItemFlags_Disabled);
        const bool bound = !disabled && (bound_id == id_prev || bound_id == id_next);
        const ImGuiKey shortcut_prev = bound ? ImGuiKey_LeftArrow : ImGuiKey_None;
        const ImGuiKey shortcut_next = bound ? ImGuiKey_RightArrow : ImGuiKey_None;
        if (button_with_shortcut(label_prev, shortcut_prev)) {
            tag = Prev;
        }
        ImGui::SameLine(0, 0), imgui_Str("/"), ImGui::SameLine(0, 0);
        if (button_with_shortcut(label_next, shortcut_next)) {
            tag = Next;
        }
        if (disable) {
            ImGui::EndGroup();
            ImGui::EndDisabled();
            if (!imgui_TestItemFlag(ImGuiItemFlags_Disabled)) {
                imgui_ItemTooltip(disable);
            }
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
    sequence() = delete;

    // `id` should be the same as one of prev/next button.
    static void bind_to(ImGuiID id) { bound_id = id; }

    static void seq(const char* label_first, const char* label_prev, const char* label_next, const char* label_last,
                    const auto& act_first, const auto& act_prev, const auto& act_next, const auto& act_last,
                    const char* disable = nullptr) {
        switch (seq(label_first, label_prev, label_next, label_last, disable)) {
            case None: return;
            case First: act_first(); return;
            case Prev: act_prev(); return;
            case Next: act_next(); return;
            case Last: act_last(); return;
        }
    }
};

class imgui_StepSliderShortcuts {
    friend bool imgui_StepSliderInt(const char* label, int* v, int v_min, int v_max, const char* format);

    inline static ImGuiKey minus = ImGuiKey_None;
    inline static ImGuiKey plus = ImGuiKey_None;
    inline static std::optional<bool> cond = std::nullopt; // Shared by both buttons.

public:
    static void reset() {
        minus = plus = ImGuiKey_None;
        cond.reset();
    }

    static void set(ImGuiKey m, ImGuiKey p, std::optional<bool> c = std::nullopt) {
        minus = m;
        plus = p;
        cond = c;
    }
};

// (Referring to ImGui::InputScalar; moved here to support shortcuts.)
inline bool imgui_StepSliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d") {
    if (GImGui->CurrentWindow->SkipItems) {
        return false;
    }

    int v2 = *v;

    const float r = ImGui::GetFrameHeight();
    const float s = imgui_ItemInnerSpacingX();
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - 2 * (r + s)));
    ImGui::SliderInt("", &v2, v_min, v_max, format, ImGuiSliderFlags_NoInput);
    ImGui::PushButtonRepeat(true);
    ImGui::SameLine(0, s);
    // (`InputScalar` makes .FramePadding.x = y for these buttons, not added here.)
    if (ImGui::Button("-", ImVec2(r, r)) ||
        shortcuts::item_shortcut(imgui_StepSliderShortcuts::minus, true, imgui_StepSliderShortcuts::cond)) {
        --v2;
    }
    ImGui::SameLine(0, s);
    if (ImGui::Button("+", ImVec2(r, r)) ||
        shortcuts::item_shortcut(imgui_StepSliderShortcuts::plus, true, imgui_StepSliderShortcuts::cond)) {
        ++v2;
    }
    ImGui::PopButtonRepeat();
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end) {
        ImGui::SameLine(0, s);
        imgui_Str(std::string_view(label, label_end));
    }
    ImGui::PopID();
    ImGui::EndGroup();

    v2 = std::clamp(v2, v_min, v_max);
    const bool changed = *v != v2;
    *v = v2;
    return changed;
}

class messenger {
    class messageT {
        std::string m_str{};
        std::optional<ImVec2> m_min{};
        int m_count{};

    public:
        // (Defined as a workaround for gcc building.)
        // (Related: https://stackoverflow.com/questions/53408962)
        messageT() : m_str{}, m_min{}, m_count{} {}

        void set(std::string&& str) {
            m_min.reset();
            m_count = 10;

            size_t subsize = 0;
            for (int line = 0; line < 20; ++line) {
                subsize = str.find_first_of('\n', subsize);
                if (subsize == str.npos) {
                    break;
                } else {
                    ++subsize; // Include the '\n'.
                }
            }

            if (subsize >= str.size()) {
                m_str = std::move(str);
            } else {
                m_str = str.substr(0, subsize) + ".....";
            }
        }

        // Won't interfere with normal tooltips or popups.
        void display() {
            if (m_str.empty()) {
                return;
            } else {
                assert(m_count > 0);
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                if ((delta.x || delta.y) && --m_count == 0) {
                    m_str.clear();
                    return;
                }
            }

            assert(!m_str.empty());
            const float text_wrap = wrap_len();
            const char *const text_beg = m_str.c_str(), *const text_end = m_str.c_str() + m_str.size();
            const ImVec2 padding = ImGui::GetStyle().WindowPadding;
            const ImVec2 window_size = ImGui::CalcTextSize(text_beg, text_end, false, text_wrap) + padding * 2;

            if (!m_min) {
                const ImVec2 main_size = ImGui::GetMainViewport()->Size;
                if (ImGui::IsMousePosValid()) {
                    const ImVec2 pos = ImClamp(ImGui::GetMousePos(), ImVec2{0, 0}, main_size);
                    ImGuiDir dir = ImGuiDir_None;
                    m_min = ImGui::FindBestWindowPosForPopupEx(pos, window_size, &dir, {padding, main_size - padding},
                                                               {pos - padding, pos + padding},
                                                               ImGuiPopupPositionPolicy_Default);
                } else {
                    m_min = ImFloor(main_size / 2 - window_size / 2);
                }
            }

            const ImVec2 window_min = *m_min;
            const ImVec2 window_max = window_min + window_size;
            ImDrawList* const drawlist = ImGui::GetForegroundDrawList();
            drawlist->AddRectFilled(window_min, window_max, ImGui::GetColorU32(ImGuiCol_PopupBg));
            drawlist->AddRect(window_min, window_max, ImGui::GetColorU32(ImGuiCol_Border));
            drawlist->AddText(nullptr, 0.0f, window_min + padding, ImGui::GetColorU32(ImGuiCol_Text), text_beg,
                              text_end, text_wrap);
        }
    };

    inline static messageT m_msg;

public:
    messenger() = delete;

    static void set_msg(std::string str) { m_msg.set(std::move(str)); }

    template <class... U>
    static void set_msg(std::format_string<const U&...> fmt, const U&... args) {
        m_msg.set(std::format(fmt, args...));
    }

    // Managed by `frame_main`.
    static void display() { m_msg.display(); }
};

// Preview rules.
class previewer {
public:
    previewer() = delete;

    // TODO: should be more flexible...
    class configT {
    public:
        enum sizeE : int { _160_160 = 0, _220_160, _220_220, _280_220, Count };

    private:
        struct termT {
            int w, h;
            const char* str;
        };
        static constexpr termT size_terms[Count]{
            {160, 160, "160*160"}, {220, 160, "220*160"}, {220, 220, "220*220"}, {280, 220, "280*220"}};

        friend previewer;
        sizeE size; // Cannot be `Count`.
        int seed;
        int step;
        void _set();

    public:
        /*implicit*/ configT(sizeE size) : size(size), seed(0), step(1) { assert(size >= 0 && size < Count); }

        int width() const { return size_terms[size].w; }
        int height() const { return size_terms[size].h; }
        ImVec2 size_imvec() const { return ImVec2(size_terms[size].w, size_terms[size].h); }

        void set(const char* label) {
            const bool clicked = ImGui::Button(label);
            if (begin_popup_for_item(clicked)) {
                _set();
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            imgui_StrTooltip("(?)", [&] {
                imgui_Str("Press 'T' to restart all preview windows.\n\n"
                          "For individual windows:\n"
                          "Right-click to restart, left-click and hold to pause.\n"
                          "'Ctrl + right-click' to copy the rule.");
                ImGui::Separator();
                _set();
            });
            // !!TODO: 'ctrl + right-click' is an important convenience improvement.
            // Should record this feature in 'Documents' as well.
        }
    };

    static void dummy(const configT& config, const ImU32 col) {
        ImGui::Dummy(config.size_imvec());
        if (ImGui::IsItemVisible()) {
            imgui_ItemRectFilled(col);
            imgui_ItemRect(ImGui::GetColorU32(ImGuiCol_TableBorderStrong)); // Instead of `ImGuiCol_Border`
        }
    }

    static void preview(uint32_t id, const configT& config, const aniso::ruleT& rule, bool interactive = true) {
        ImGui::Dummy(config.size_imvec());
        if (ImGui::IsItemVisible()) {
            ImU32 border_col = ImGui::GetColorU32(ImGuiCol_TableBorderStrong);
            _preview((uint64_t(ImGui::GetID("")) << 32) | id, config, rule, interactive, border_col);
            imgui_ItemRect(border_col);
        }
    }

    static void preview(uint32_t id, const configT& config, const std::invocable<> auto& get_rule,
                        bool interactive = true) {
        ImGui::Dummy(config.size_imvec());
        if (ImGui::IsItemVisible()) {
            ImU32 border_col = ImGui::GetColorU32(ImGuiCol_TableBorderStrong);
            _preview((uint64_t(ImGui::GetID("")) << 32) | id, config, get_rule(), interactive, border_col);
            imgui_ItemRect(border_col);
        }
    }

private:
    static void _preview(uint64_t id, const configT& config, const aniso::ruleT& rule, bool interactive,
                         ImU32& border_col);
};

// The program will behave as if the lock feature doesn't exist when !enable_lock.
class sync_point {
    friend void frame_main();

    std::optional<aniso::ruleT> out_rule = std::nullopt;
    std::optional<aniso::moldT::lockT> out_lock = std::nullopt;
    bool enable_lock_next;

    sync_point(const aniso::ruleT& rule, const aniso::moldT::lockT& lock, bool enable_lock)
        : current{rule, enable_lock ? lock : aniso::moldT::lockT{}}, enable_lock(enable_lock) {
        enable_lock_next = enable_lock;
    }

public:
    sync_point(const sync_point&) = delete;
    sync_point& operator=(const sync_point&) = delete;

    const aniso::moldT current;
    const bool enable_lock;

    void set_rule(const aniso::ruleT& rule) {
        out_rule.emplace(rule);
        out_lock.reset();
    }
    void set_lock(const aniso::moldT::lockT& lock) {
        assert(enable_lock);
        out_rule.reset();
        out_lock.emplace(lock);
    }
    void set_mold(const aniso::moldT& mold) {
        out_rule.emplace(mold.rule);
        if (enable_lock) {
            out_lock.emplace(mold.lock);
        } else {
            out_lock.reset();
        }
    }
    void set_val(const aniso::extrT::valT& val) {
        out_rule.emplace(val.rule);
        if (enable_lock && val.lock) {
            out_lock.emplace(*val.lock);
        } else {
            out_lock.reset();
        }
    }

    void display_if_enable_lock(const std::invocable<bool> auto& append) {
        if (enable_lock) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            if (ImGui::IsMousePosValid()) {
                ImGui::SetNextWindowPos(ImGui::GetMousePos() + ImVec2(2, 2), ImGuiCond_Appearing);
            }
            // (wontfix) Will append to the same window if there are to be multiple instances (won't happen).
            auto window = imgui_Window("Lock & capture (experimental)", &enable_lock_next,
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            append(window.visible);
        }
    }
};

inline void set_clipboard_and_notify(const char* c_str) {
    ImGui::SetClipboardText(c_str);
    messenger::set_msg("Copied.");
}

inline void set_clipboard_and_notify(const std::string& str) { //
    set_clipboard_and_notify(str.c_str());
}
