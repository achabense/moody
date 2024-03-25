#include "common.hpp"

// TODO: improve recorder logic if possible...
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

// TODO: make some tooltips interactive?
void frame_main(const code_icons& icons, screenT& screen) {
    messenger::display();

    static recorderT recorder;
    legacy::moldT current = recorder.current();
    bool update = false;

    static bool show_file = false;
    static bool show_clipboard = false;
    static bool show_doc = true;
    static bool show_static = false;

    auto load_rule = [&](bool& flag, const char* title, std::optional<legacy::extrT::valT> (*load_fn)()) {
        ImGui::Checkbox(title, &flag);
        if (flag) {
            ImGui::SetNextWindowSize({600, 400}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            if (auto window = imgui_Window(title, &flag)) {
                if (auto out = load_fn()) {
                    assign_val(current, *out);
                    update = true;
                }
            }
        }
    };

    if (show_static) {
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
        if (auto window = imgui_Window("Static constraints", &show_static, ImGuiWindowFlags_AlwaysAutoResize)) {
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
        load_rule(show_file, "Load file", load_file);
        ImGui::SameLine();
        load_rule(show_clipboard, "Clipboard", load_clipboard);
        ImGui::SameLine();
        load_rule(show_doc, "Documents", load_doc);

        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        ImGui::Checkbox("Static", &show_static); // TODO: move elsewhere?
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        const ImGuiID id_prev =
            ImGui::GetID("prev"); // For `sequence::bind_to` (when the rule is gotten by randomization.)
        sequence::seq(
            "<|", "prev", "next", "|>", //
            [&] { recorder.set_first(); }, [&] { recorder.set_prev(); }, [&] { recorder.set_next(); },
            [&] { recorder.set_last(); });
        ImGui::SameLine();
        ImGui::Text("Total:%d At:%d", recorder.size(), recorder.pos() + 1 /* [1, size()] */);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { // TODO: the effect is not obvious.
            recorder.clear();
        }
#ifndef NDEBUG
        ImGui::SameLine();
        imgui_Str("  (Debug mode)");
        ImGui::SameLine();
        static bool show_demo = true;
        ImGui::Checkbox("Demo window", &show_demo);
        if (show_demo) {
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
            ImGui::ShowDemoWindow(&show_demo);
        }
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        ImGui::Text("(%.1f FPS) Frame:%d", ImGui::GetIO().Framerate, ImGui::GetFrameCount());
#endif // !NDEBUG

        {
            static bool with_lock = false;
            if (with_lock) {
                imgui_StrCopyable(legacy::to_MAP_str(current), imgui_Str);
                with_lock = ImGui::IsItemHovered();
            } else {
                imgui_StrCopyable(legacy::to_MAP_str(current.rule), imgui_Str);
                ImGui::SameLine(0, ImGui::CalcTextSize(" ").x - 1 /* For correct alignment */);
                std::string lock_str = "[";
                legacy::_misc::to_MAP(lock_str, current.lock);
                lock_str += "]";
                imgui_StrDisabled(lock_str);
                with_lock = ImGui::IsItemHovered();
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Edit", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
                bool randomized = false;
                if (auto out = edit_rule(current, icons, randomized)) {
                    current = *out;
                    update = true;
                }
                if (randomized) {
                    sequence::bind_to(id_prev);
                }
            }
            ImGui::TableNextColumn();
            if (auto child = imgui_ChildWindow("Apply", {}, 0, ImGuiWindowFlags_NoScrollbar)) {
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
