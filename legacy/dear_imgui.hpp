#pragma once

#include <algorithm>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

inline void imgui_ItemRect(ImU32 col, ImVec2 off_min = {0, 0}) {
    const ImVec2 pos_min = ImGui::GetItemRectMin();
    const ImVec2 pos_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRect(ImVec2(pos_min.x + off_min.x, pos_min.y + off_min.y),
                                        ImVec2(pos_max.x - off_min.x, pos_max.y - off_min.y), col);
}

inline void imgui_ItemRectFilled(ImU32 col, ImVec2 off_min = {0, 0}) {
    const ImVec2 pos_min = ImGui::GetItemRectMin();
    const ImVec2 pos_max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos_min.x + off_min.x, pos_min.y + off_min.y),
                                              ImVec2(pos_max.x - off_min.x, pos_max.y - off_min.y), col);
}

// Unlike ImGui::Text(Wrapped/...), these functions take unformatted string as the argument.
inline void imgui_Str(std::string_view str) { //
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

inline void imgui_StrWrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    imgui_Str(str);
    ImGui::PopTextWrapPos();
}

inline void imgui_StrColored(const ImVec4& col, std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    imgui_Str(str);
    ImGui::PopStyleColor();
}

inline void imgui_StrDisabled(std::string_view str) {
    imgui_StrColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], str);
}

// Using std::string as `SetClipboardText` requires C-style string.
inline void imgui_StrCopyable(const std::string& str, void (*str_func)(std::string_view),
                              ImGuiMouseButton_ mouse_button = ImGuiMouseButton_Right) {
    str_func(str);
    if (ImGui::IsItemClicked(mouse_button)) {
        ImGui::SetClipboardText(str.c_str());
        imgui_ItemRect(IM_COL32_WHITE);
    } else if (ImGui::IsItemHovered()) {
        imgui_ItemRect(IM_COL32(128, 128, 128, 255));
    }
}

// TODO: still clunky...
inline float imgui_ItemInnerSpacingX() { return ImGui::GetStyle().ItemInnerSpacing.x; }

// TODO: referring to ImGui::InputScalar; recheck...
inline bool imgui_StepSliderInt(const char* label, int* v, int v_min, int v_max) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        return false;
    }

    int v2 = *v;

    const float r = ImGui::GetFrameHeight();
    const float s = imgui_ItemInnerSpacingX();
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - 2 * (r + s)));
    ImGui::SliderInt("", &v2, v_min, v_max, "%d", ImGuiSliderFlags_NoInput);
    ImGui::PushButtonRepeat(true);
    ImGui::SameLine(0, s);
    // TODO: whether to push&pop framepadding?
    if (ImGui::Button("-", ImVec2(r, r))) {
        --v2;
    }
    ImGui::SameLine(0, s);
    if (ImGui::Button("+", ImVec2(r, r))) {
        ++v2;
    }
    ImGui::PopButtonRepeat();
    // TODO: FindRenderedTextEnd is a function in imgui_internal.h...
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

// TODO: recheck `bool IsKeyPressed(ImGuiKey, ImGuiID, ImGuiInputFlags)` in "imgui_internal.h"
inline bool imgui_KeyPressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: doesn't work if adding `!ImGui::GetIO().WantCaptureMouse`...
inline bool imgui_MouseScrolling() { return ImGui::GetIO().MouseWheel != 0; }

inline bool imgui_MouseScrollingDown() { return ImGui::GetIO().MouseWheel < 0; }

inline bool imgui_MouseScrollingUp() { return ImGui::GetIO().MouseWheel > 0; }

struct [[nodiscard]] imgui_Window {
    imgui_Window(const imgui_Window&) = delete;
    imgui_Window& operator=(const imgui_Window&) = delete;

    const bool visible;
    explicit imgui_Window(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_Window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_ChildWindow {
    imgui_ChildWindow(const imgui_ChildWindow&) = delete;
    imgui_ChildWindow& operator=(const imgui_ChildWindow&) = delete;

    const bool visible;
    explicit imgui_ChildWindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_ChildWindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

// TODO: Move to common.h as this is not "general" enough...
class [[nodiscard]] imgui_ItemTooltip {
    bool begun = false;

public:
    imgui_ItemTooltip(const imgui_ItemTooltip&) = delete;
    imgui_ItemTooltip& operator=(const imgui_ItemTooltip&) = delete;

    explicit imgui_ItemTooltip(bool& toggle) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            // TODO: still have slight control conflicts...
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
                toggle = !toggle;
            }
            if (toggle) {
                // TODO: wrap according to actual tooltip contents...
                // imgui_Str("Double right click to turn on/off the tooltip");
                ImGui::BeginTooltip();
                begun = true;
            }
        }
    }
    ~imgui_ItemTooltip() {
        if (begun) {
            ImGui::EndTooltip();
        }
    }

    explicit operator bool() const { return begun; }
};

#if 0
// TODO: or this?
inline bool imgui_BeginItemTooltip(bool& toggle) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        // TODO: still have slight control conflicts...
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
            toggle = !toggle;
        }

        return toggle && ImGui::BeginTooltip();
    }

    return false;
}
#endif
