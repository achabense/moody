#include "common.hpp"

#ifndef NDEBUG
#define SET_FRAME_RATE
#endif

#ifdef SET_FRAME_RATE
#include <thread>

class timerT {
    // (Typically the framerate is limited by VSync in this case, like 60 fps.)
    static constexpr int max_fps = 100;
    int fps = max_fps;

    using clockT = std::chrono::steady_clock;
    clockT::time_point last{};

public:
    void wait() {
        using namespace std::chrono_literals;
        const auto now = clockT::now();
        const auto until = last + 1000ms / fps;
        if (now < until) {
            std::this_thread::sleep_until(until);
            last = until; // Instead of another `clockT::now()` call.
        } else {
            last = now;
        }
    }

    void set_fps() {
        imgui_RadioButton("Auto", &fps, max_fps);
        ImGui::SameLine();
        imgui_StrTooltip("(?)", "Typically limited by VSync.");

        ImGui::SameLine();
        imgui_RadioButton("50 fps", &fps, 50);
        ImGui::SameLine();
        imgui_RadioButton("10 fps", &fps, 10);
    }
};
#endif

static void get_reversal_dual(const bool button_result, sync_point& sync) {
    if (button_result) {
        sync.set(rule_algo::trans_reverse(sync.rule));
    }
    imgui_ItemTooltip([&] {
        imgui_Str("Get the 0/1 reversal dual of the current rule.");
        ImGui::Separator();
        const aniso::ruleT rev = rule_algo::trans_reverse(sync.rule);
        if (rev != sync.rule) {
            imgui_Str("Preview:");
            ImGui::SameLine();
            previewer::preview(-1, previewer::configT::_220_160, rev, false);
        } else {
            imgui_Str("(The result will be the same as the current rule, as the current rule is self-complementary.)");
        }
    });
}

void frame_main() {
#ifdef SET_FRAME_RATE
    static timerT timer;
    timer.wait();
#endif // SET_FRAME_RATE

    global_timer::begin_frame();
    guide_mode::begin_frame();
    shortcuts::begin_frame();
    sequence::begin_frame();
    previewer::begin_frame();

    static aniso::ruleT sync_rule = aniso::game_of_life();
    sync_point sync = sync_rule;

    rule_record::tested(sync.rule);
    if (sync_point_override::begin_frame(&sync.rule)) {
        messenger::set_msg("This is the same as the current rule.");
    }

    messenger::display();

    // TODO: add a way to specify the contents for the left panel. These loading windows can fit well.
    static bool show_file = false;
    static bool show_clipboard = false;
    static bool show_doc = false;
    static bool show_record = false;
    auto load_rule = [&](bool& open, const char* title, void (*load_fn)(sync_point&)) {
        if (ImGui::Checkbox(title, &open) && open) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        }

        // This is a workaround to support shortcut for clipboard-reading.
        // TODO: using 'W' to avoid conflicts with pattern-pasting; not quite conventional...
        if (&open == &show_clipboard) {
            if (shortcuts::keys_avail_and_window_hoverable() && shortcuts::test(ImGuiKey_W)) {
                open = true;
                ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
                ImGui::SetNextWindowFocus();
            }
        }

        if (open) {
            ImGui::SetNextWindowPos(ImGui::GetItemRectMin() + ImVec2(0, ImGui::GetFrameHeight() + 4),
                                    ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(FLT_MAX, FLT_MAX));
            if (auto window = imgui_Window(title, &open, ImGuiWindowFlags_NoSavedSettings)) {
                load_fn(sync);
            }
        }
    };

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (auto window = imgui_Window("Main", nullptr, flags)) {
        load_rule(show_file, "Files", load_file);
        guide_mode::item_tooltip("Load rules from files.");
        ImGui::SameLine();
        load_rule(show_clipboard, "Clipboard", load_clipboard);
        guide_mode::item_tooltip("Load rules from the clipboard. Shortcut: 'W'.\n\n"
                                 "('V' is for pasting patterns in the right panel.)");
        ImGui::SameLine();
        load_rule(show_doc, "Documents", load_doc);
        guide_mode::item_tooltip("Concepts, example rules, etc.");

        const int wide_spacing = ImGui::CalcTextSize(" ").x * 3;
        ImGui::SameLine(0, wide_spacing);
        load_rule(show_record, "Record", rule_record::load_record);
        guide_mode::item_tooltip("Record for the current rule and copied rules.");

        ImGui::SameLine(0, wide_spacing);
        ImGui::Text("(%d FPS)", (int)round(ImGui::GetIO().Framerate));
#ifdef SET_FRAME_RATE
        ImGui::SameLine();
        ImGui::SmallButton("Set");
        if (begin_popup_for_item()) {
            timer.set_fps();
            ImGui::EndPopup();
        }
#endif // SET_FRAME_RATE

#ifndef NDEBUG
        ImGui::SameLine(0, wide_spacing);
        imgui_Str("(Debug mode)");
        ImGui::SameLine();
        static bool show_demo = false;
        ImGui::Checkbox("Demo window", &show_demo);
        if (show_demo) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            ImGui::ShowDemoWindow(&show_demo);
        }
        ImGui::SameLine(0, wide_spacing);
        ImGui::Text("Frame:%d", ImGui::GetFrameCount());
#endif // !NDEBUG

        ImGui::Separator();

        // !!TODO: rewrite...
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(
            "(...)",
            "(Press 'H' to enter help mode for more tooltips; press 'H' again to quit the mode.)\n\n"
            "'<| Prev/Next |>' represents the record for the current rule (as shown in the right panel). You can switch to previously tested rules with it.\n\n"
            "Right-click 'Total:.. At:..' to clear the record.\n\n"
            "Right-click the MAP-string (for the current rule) to copy to the clipboard.");

        // !!TODO: find a new place for this...
        // When a button is clicked, or when the window is focused and you press the left/right arrow key, the
        // left/right arrow key will begin to serve as the shortcuts for 'Prev/Next'. This also applies to sequences
        // in other windows.

        ImGui::SameLine();
        imgui_Str("Current rule ~");
        ImGui::SameLine();
        get_reversal_dual(ImGui::Button("Rev"), sync);
        ImGui::SameLine();
        if (imgui_StrCopyable(aniso::to_MAP_str(sync.rule), imgui_Str, set_clipboard_and_notify)) {
            rule_record::copied(sync.rule);
        }
        guide_mode::item_tooltip("Right-click to copy.");

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            const char* const label_hidden = "H\ni\nd\nd\ne\nn";
            const float min_w = imgui_CalcButtonSizeX(label_hidden);

            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 500);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
            imgui_LockTableLayoutWithMinColumnWidth(min_w);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // The child window is required here (for stable scrolling).
            if (auto child = imgui_ChildWindow("Edit", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (ImGui::GetContentRegionAvail().x == min_w) { // TODO: working, but looks fragile...
                    ImGui::BeginDisabled();
                    ImGui::Button(label_hidden, ImGui::GetContentRegionAvail());
                    ImGui::EndDisabled();
                } else {
                    edit_rule(sync);
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Apply", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (ImGui::GetContentRegionAvail().x == min_w) {
                    ImGui::BeginDisabled();
                    ImGui::Button(label_hidden, ImGui::GetContentRegionAvail());
                    ImGui::EndDisabled();
                } else {
                    apply_rule(sync);
                }
            }
            ImGui::EndTable();
        }
    }

    if (sync.out_rule) {
        sync_rule = *sync.out_rule;
    }
}
