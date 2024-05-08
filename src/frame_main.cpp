#include "common.hpp"

// TODO: improve recorder logic if possible...
// Never empty.
class recorderT {
    std::vector<aniso::moldT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back({.rule = aniso::game_of_life(), .lock{}});
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const aniso::moldT& mold) {
        if (mold != m_record[m_pos]) {
            const int last = size() - 1;
            if (m_pos == last && last != 0 && mold == m_record[last - 1]) {
                m_pos = last - 1;
            } else if (m_pos == last - 1 && mold == m_record[last]) {
                m_pos = last;
            } else {
                // TODO: or reverse([m_pos]...[last])? or do nothing?
                if (m_pos != last) {
                    std::swap(m_record[m_pos], m_record[last]);
                }
                m_record.push_back(mold);
                set_last();
            }
        }
    }

    const aniso::moldT* current() const {
        assert(m_pos >= 0 && m_pos < size());
        return &m_record[m_pos];
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    void clear() {
        // `m_record = {m_record[m_pos]}` will not free the memory.
        m_record = std::vector<aniso::moldT>{m_record[m_pos]};
        m_pos = 0;
    }

private:
    void set_pos(int pos) { m_pos = std::clamp(pos, 0, size() - 1); }
};

// TODO: make some tooltips interactive?
void frame_main() {
    messenger::display();

    static recorderT recorder;
    bool freeze = false;
    sync_point sync(recorder.current());

    static bool show_file = false;
    static bool show_clipboard = false;
    static bool show_doc = false;
    auto load_rule = [&](bool& flag, const char* title, void (*load_fn)(sync_point&)) {
        ImGui::Checkbox(title, &flag);
        if (flag) {
            ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            if (auto window = imgui_Window(title, &flag)) {
                load_fn(sync);
            }
        }
    };

    static bool show_static = false;
    if (show_static) {
        assert(manage_lock::enabled());
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
        if (auto window = imgui_Window("Static constraints", &show_static, ImGuiWindowFlags_AlwaysAutoResize)) {
            // TODO: bind-undo in this case?
            static_constraints(sync);
        }
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (auto window = imgui_Window("Main", nullptr, flags)) {
        load_rule(show_file, "Load file", load_file);
        ImGui::SameLine();
        load_rule(show_clipboard, "Clipboard", load_clipboard);
        ImGui::SameLine();
        load_rule(show_doc, "Documents", load_doc);
        ImGui::SameLine();
        manage_lock::checkbox();
        manage_lock::display([](bool visible) {
            if (visible) {
                ImGui::Checkbox("Static constraints", &show_static);
            }
        });
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
        ImGui::Text("| (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());
#else
        ImGui::SameLine();
        ImGui::Text("(%.1f FPS)", ImGui::GetIO().Framerate);
#endif // !NDEBUG

        ImGui::Separator();

        // TODO: rename 'Prev/Next'? is 'Undo/Redo' suitable?
        // Use icons? https://github.com/ocornut/imgui/blob/master/docs/FONTS.md#using-icon-fonts
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(
            "(...)",
            "Below is the MAP-string for the current rule (as shown in the right plane). You can right-click the rule "
            "to copy to the clipboard.\n\n"
            "Here '<| Prev/Next |>' represents the record for the current rule. You can undo/redo the editions "
            "with it. When you click the button the left/right arrow key will be bound to 'Prev/Next'.\n"
            "(If you want to clear the record, you can right-click the 'Total:.. At:..' text, and then click 'Clear' "
            "in the popup to confirm.)");
        ImGui::SameLine();

        const ImGuiID id_prev =
            ImGui::GetID("Prev"); // For `sequence::bind_to` (when the rule is gotten by randomization.)
        ImGui::BeginGroup();
        sequence::seq(
            "<|", "Prev", "Next", "|>", //
            [&] { freeze = true, recorder.set_first(); }, [&] { freeze = true, recorder.set_prev(); },
            [&] { freeze = true, recorder.set_next(); }, [&] { freeze = true, recorder.set_last(); });
        ImGui::EndGroup();
        quick_info("v For undo/redo.");
        ImGui::SameLine();
        ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1 /* [1, size()] */);
        quick_info("< Right-click to clear.");
        if (ImGui::BeginPopupContextItem("", ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::Selectable("Clear")) {
                freeze = true, recorder.clear();
            }
            ImGui::EndPopup();
        }

        {
            static bool with_lock = false;
            if (with_lock) {
                assert(manage_lock::enabled());
                imgui_StrCopyable(aniso::to_MAP_str(sync.current), imgui_Str);
                with_lock = ImGui::IsItemHovered();
            } else {
                imgui_StrCopyable(aniso::to_MAP_str(sync.current.rule), imgui_Str);
                quick_info("< Right-click to copy to the clipboard.");
                if (manage_lock::enabled()) {
                    ImGui::SameLine(0, ImGui::CalcTextSize(" ").x - 1 /* For correct alignment */);
                    std::string lock_str = "[";
                    aniso::_misc::to_MAP(lock_str, sync.current.lock);
                    lock_str += "]";
                    imgui_StrDisabled(lock_str);
                    with_lock = ImGui::IsItemHovered();
                }
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Edit", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                bool bind_undo = false;
                edit_rule(sync, bind_undo);
                if (bind_undo) {
                    sequence::bind_to(id_prev);
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Apply", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                apply_rule(sync);
            }
            ImGui::EndTable();
        }
    }

    if (!freeze && sync.out) {
        recorder.update(*sync.out);
    }
}
