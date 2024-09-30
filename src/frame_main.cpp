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

// TODO: improve recorder logic if possible...
// Never empty.
class recorderT {
    std::vector<aniso::ruleT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back(aniso::game_of_life());
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    const aniso::ruleT& current() const {
        assert(m_pos >= 0 && m_pos < size());
        return m_record[m_pos];
    }

    void update(const aniso::ruleT& rule) {
        if (rule != m_record[m_pos]) {
            const int last = size() - 1;
            if (m_pos == last && last != 0 && rule == m_record[last - 1]) {
                m_pos = last - 1;
            } else if (m_pos == last - 1 && rule == m_record[last]) {
                m_pos = last;
            } else {
                // TODO: or reverse([m_pos]...[last])? or do nothing?
                if (m_pos != last) {
                    std::swap(m_record[m_pos], m_record[last]);
                }
                m_record.push_back(rule);
                set_last();
            }
        }
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    void clear() {
        // `m_record = {m_record[m_pos]}` will not free the memory.
        m_record = std::vector<aniso::ruleT>{m_record[m_pos]};
        m_pos = 0;
    }

private:
    void set_pos(int pos) { m_pos = std::clamp(pos, 0, size() - 1); }
};

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

    messenger::display();

    static recorderT recorder;
    bool freeze = false;
    sync_point sync = recorder.current();
    if (sync_point_override::begin_frame(&sync.rule)) {
        messenger::set_msg("This is the same as the current rule.");
    }

    // TODO: add a way to specify the contents for the left panel. These loading windows can fit well.
    static bool show_file = false;
    static bool show_clipboard = false;
    static bool show_doc = false;
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

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
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
        ImGui::Text("(%d FPS)", (int)round(ImGui::GetIO().Framerate));
#ifdef SET_FRAME_RATE
        ImGui::SameLine();
        ImGui::SmallButton("Set");
        if (begin_menu_for_item()) {
            timer.set_fps();
            ImGui::EndPopup();
        }
#endif // SET_FRAME_RATE

#ifndef NDEBUG
        ImGui::SameLine();
        imgui_Str("  (Debug mode)");
        ImGui::SameLine();
        static bool show_demo = false;
        ImGui::Checkbox("Demo window", &show_demo);
        if (show_demo) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            ImGui::ShowDemoWindow(&show_demo);
        }
        ImGui::SameLine();
        ImGui::Text("  Frame:%d", ImGui::GetFrameCount());
#endif // !NDEBUG

        ImGui::Separator();

        // TODO: rename 'Prev/Next'? is 'Undo/Redo' suitable?
        // Use icons? https://github.com/ocornut/imgui/blob/master/docs/FONTS.md#using-icon-fonts
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(
            "(...)", "'<| Prev/Next |>' represents the record for the current rule (as shown in the right panel). "
                     "When clicked, the left/right arrow key will be bound to 'Prev/Next'.\n\n"
                     "To clear the record, right-click 'Total:.. At:..' and then click 'Clear' in the "
                     "popup to confirm.\n\n"
                     "To save the current rule, right-click the MAP-string, and it will be copied to the clipboard.");
        // !!TODO: outdated.

        ImGui::SameLine();
        ImGui::BeginGroup();
        sequence::seq(
            "<|", "Prev", "Next", "|>", //
            [&] { freeze = true, recorder.set_first(); }, [&] { freeze = true, recorder.set_prev(); },
            [&] { freeze = true, recorder.set_next(); }, [&] { freeze = true, recorder.set_last(); });
        ImGui::EndGroup();
        imgui_ItemTooltip_StrID = "Seq##Record";
        guide_mode::item_tooltip("You can switch to previously tested rules with this.");

        ImGui::SameLine();
        ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1 /* [1, size()] */);
        if (imgui_ItemClickableDouble()) {
            set_msg_cleared(recorder.size() > 1);
            freeze = true, recorder.clear();
        }
        imgui_ItemTooltip_StrID = "Clear##Rec";
        guide_mode::item_tooltip("Double right-click to clear the record (except the current rule).");

        ImGui::SameLine();
        get_reversal_dual(ImGui::Button("Rev"), sync);

        ImGui::SameLine();
        imgui_StrCopyable(aniso::to_MAP_str(sync.rule), imgui_Str, set_clipboard_and_notify);
        guide_mode::item_tooltip("Right-click to copy.");

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            const char* const label_hidden = "H\ni\nd\nd\ne\nn";
            const float min_w = imgui_CalcButtonSizeX(label_hidden);
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

    if (!freeze && sync.out_rule) {
        recorder.update(*sync.out_rule);
    }
}
