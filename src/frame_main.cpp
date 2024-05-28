#include "common.hpp"

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

void frame_main() {
    messenger::display();

    // TODO: the constraint model (constraint = {recorder.current(), lock}) is very fragile now.
    // The meaning of the constraint may be changed unexpectedly when:
    // 1. `recorder.set_xx` switches to a !compatible rule.
    // 2. `load_fn` loads a !compatible rule.
    // 3. the rule is modified during `!enable_lock`.
    static recorderT recorder;
    static aniso::moldT::lockT lock;
    static bool enable_lock = false;
    bool freeze = false;
    sync_point sync(recorder.current(), lock, enable_lock);

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
        quick_info("< Concepts, example rules, etc.");
        ImGui::SameLine(), imgui_Str(" "), ImGui::SameLine();
        ImGui::Checkbox("Lock & capture", &sync.enable_lock_next);
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
        ImGui::Text("  (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());
#else
        ImGui::SameLine();
        ImGui::Text("  (%.1f FPS)", ImGui::GetIO().Framerate);
#endif // !NDEBUG

        ImGui::Separator();

        // TODO: rename 'Prev/Next'? is 'Undo/Redo' suitable?
        // Use icons? https://github.com/ocornut/imgui/blob/master/docs/FONTS.md#using-icon-fonts
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(
            "(...)",
            "Below is the MAP-string for the current rule (as shown in the right plane). You can right-click the rule "
            "to copy to the clipboard.\n\n"
            "Here '<| Prev/Next |>' represents the record for the current rule. You can undo/redo the modifications "
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
        if (begin_popup_for_item(ImGui::IsItemClicked(ImGuiMouseButton_Right), "")) {
            if (ImGui::Selectable("Clear")) {
                freeze = true, recorder.clear();
            }
            ImGui::EndPopup();
        }

        {
            static bool hover_lock = false;
            if (sync.enable_lock && hover_lock) {
                imgui_StrCopyable(aniso::to_MAP_str(sync.current), imgui_Str);
                hover_lock = ImGui::IsItemHovered();
            } else {
                imgui_StrCopyable(aniso::to_MAP_str(sync.current.rule), imgui_Str);
                quick_info("< Right-click to copy to the clipboard.");
                if (sync.enable_lock) {
                    ImGui::SameLine(0, ImGui::CalcTextSize(" ").x - 1 /* For correct alignment */);
                    std::string lock_str = "[";
                    aniso::_misc::to_MAP(lock_str, sync.current.lock);
                    lock_str += "]";
                    imgui_StrDisabled(lock_str);
                    hover_lock = ImGui::IsItemHovered();
                } else {
                    hover_lock = false;
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

        sync.display_if_enable_lock([&](bool visible) {
            if (visible) {
                static bool show_static = false;
                ImGui::Separator();
                ImGui::Checkbox("Static constraints", &show_static);
                if (show_static) {
                    // TODO: bind-undo in this case?
                    ImGui::PushID("static");
                    static_constraints(sync);
                    ImGui::PopID();
                }
            }
        });
    }

    enable_lock = sync.enable_lock_next;
    if (!freeze) {
        if (sync.out_rule) {
            recorder.update(*sync.out_rule);
        }
        if (sync.out_lock) {
            assert(sync.enable_lock);
            lock = *sync.out_lock;
        }
    }
}
