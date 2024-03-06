#include "common.hpp"

// TODO: (Must) add help-mode coverage.

// TODO: improve recorder logic (especially `update`...)
// Never empty.
class recorderT {
    std::vector<legacy::moldT> m_record;
    int m_pos;

public:
    recorderT() {
        m_record.push_back({.rule = legacy::game_of_life(), .lock{}});
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    void update(const legacy::moldT& mold) {
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

    legacy::moldT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return m_record[m_pos];
    }

    void set_next() { set_pos(m_pos + 1); }
    void set_prev() { set_pos(m_pos - 1); }
    void set_first() { set_pos(0); }
    void set_last() { set_pos(size() - 1); }

    void clear() {
        m_record = {m_record[m_pos]};
        m_pos = 0;
    }

private:
    void set_pos(int pos) { m_pos = std::clamp(pos, 0, size() - 1); }
};

static void assign_val(legacy::moldT& mold, legacy::extrT::valT& val) {
    if (val.lock) {
        mold.lock = *val.lock;
        mold.rule = val.rule;
    } else {
        if (!mold.compatible(val.rule)) {
            mold.lock = {};
        }
        mold.rule = val.rule;
    }
}

void frame_main(const code_icons& icons, screenT& screen) {
    messenger::display();

#ifndef NDEBUG
    ImGui::ShowDemoWindow();
#endif

    static recorderT recorder;
    legacy::moldT current = recorder.current();
    bool update = false;

    static bool show_load = true;
    static bool show_static = false;

    if (show_load) {
        // TODO: which is responsible for setting Size(Constraints)? frame_main or load_rule?
        ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        if (auto window = imgui_Window("Load rule", &show_load, ImGuiWindowFlags_NoCollapse)) {
            if (auto out = load_rule()) {
                assign_val(current, *out);
                update = true;
            }
        }
    }
    if (show_static) {
        if (auto window = imgui_Window("Static constraints", &show_static,
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
            if (auto out = static_constraints()) {
                current = *out;
                update = true;
            }
        }
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (auto window = imgui_Window("Main", nullptr, flags)) {
        if (imgui_KeyPressed(ImGuiKey_H, false)) {
            helper::enable_help = !helper::enable_help;
        }

        ImGui::Checkbox("\"Help\"", &helper::enable_help);
        helper::show_help("Or press 'H' to toggle this mode.");
        ImGui::SameLine();
        ImGui::Checkbox("\"Load\"", &show_load);
        ImGui::SameLine();
        ImGui::Checkbox("\"Static\"", &show_static);

        // TODO: whether to show FPS/Framecount? whether to show time-since-startup?
        // ImGui::Text("    (%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        // TODO: simplify? unlike the one in fileT::display, this is mainly for undo/redo...
        sequence::seq(
            "<|", "prev", "next", "|>", //
            [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
            [&] { recorder.set_last(); });
        ImGui::SameLine();
        ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1); // TODO: +1 is clumsy
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            recorder.clear();
        }

        // TODO: whether to do set_*(next etc) / update current eagerly during the frame?
        // (whether to postpone after gui logics?)
        {
            // TODO: reconsider how to show help for the rule string...
            // TODO: about lock...
            if (helper::show_help("Current rule. You can hover on the text and right-click to copy to the clipboard.",
                                  false)) {
                ImGui::SameLine(0, 0);
            }
            // TODO: add a shortcut for quick rule-saving...
            // (As the rule may be gotten from enter-bound buttons)
            static bool with_lock = false;
            if (with_lock) {
                imgui_StrCopyable(legacy::to_MAP_str(current), imgui_Str);
                with_lock = ImGui::IsItemHovered();
            } else {
                imgui_StrCopyable(legacy::to_MAP_str(current.rule), imgui_Str);
                ImGui::SameLine(0, ImGui::CalcTextSize(" ").x - 1); // TODO: about -1...
                std::string lock_str = "[";
                legacy::_misc::to_MAP(lock_str, current.lock); // TODO: avoid raw calls like this...
                lock_str += "]";
                imgui_StrDisabled(lock_str);
                with_lock = ImGui::IsItemHovered();
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Rul", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = edit_rule(current, icons)) {
                    current = *out;
                    update = true;
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Til", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                if (auto out = apply_rule(current.rule, screen)) {
                    current.lock = *out;
                    update = true;
                }
            }
            ImGui::EndTable();
        }
    }

    if (update) {
        recorder.update(current);
    }
}
