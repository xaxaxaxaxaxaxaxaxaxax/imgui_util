// property_editor.hpp - label + input property rows for settings panels
//
// Usage:
//   imgui_util::property_text("Name", "##name", name_str);
//   imgui_util::property_double("Speed", "##spd", speed, "%.2f", 0.0, 100.0);
//   imgui_util::property_checkbox("Enabled", "##en", enabled);
//   imgui_util::property_path("Model", "##mdl", path, default_dir, state, ".obj");
//   imgui_util::property_color("Color", "##col", color);
//   imgui_util::property_slider("Alpha", "##a", alpha, 0.0f, 1.0f);
//
// All property_* functions render a label on the left and an input on the right.
// Returns true when the value was edited this frame.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <limits>
#include <log.h>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/controls.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Configurable label width for property editor rows
    inline constexpr float property_label_width = 180.0f;

    // Common label/layout pattern for property editor rows
    inline void property_label(const char *label, const float label_width) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        ImGui::SetNextItemWidth(-1.0f);
    }

    // Helper: InputText writing directly into a std::string via CallbackResize.
    // Returns true if the value was edited.
    [[nodiscard]]
    inline bool input_text_buffered(const char *id, std::string &value, const ImGuiInputTextFlags flags = 0,
                                    const ImVec2 &size = {0, 0}) {
        const auto cb_flags = flags | ImGuiInputTextFlags_CallbackResize; // NOLINT(hicpp-signed-bitwise)
        if (size.x != 0 || size.y != 0) {
            return ImGui::InputTextMultiline(id, value.data(), value.capacity() + 1, size, cb_flags,
                                             input_text_callback_std_string, &value);
        }
        return ImGui::InputText(id, value.data(), value.capacity() + 1, cb_flags, input_text_callback_std_string,
                                &value);
    }

    // Label + InputText on a single row, full remaining width (fixed buffer copy-in/copy-out)
    [[nodiscard]]
    inline bool property_text(const char *label, const char *id, std::string &value,
                              const float label_width = property_label_width) {
        property_label(label, label_width);
        return input_text_buffered(id, value);
    }

    // Caller-owned state for property_path directory listing cache.
    // Keep one per property_path call site; popup caches the directory scan.
    struct path_editor_state {
        std::string                                        key;     // popup ID that owns this cache
        std::vector<std::pair<std::string, std::string>>   entries; // {full_path, display_name}
    };

    // Label + InputText + "..." browse button with file/dir popup
    [[nodiscard]]
    inline bool property_path(const char *label, const char *id, std::string &value,
                              const std::filesystem::path &default_dir, path_editor_state &state,
                              const std::string_view ext = "", const float label_width = property_label_width) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);

        bool            changed      = false;
        constexpr float browse_btn_w = 30.0f;
        ImGui::SetNextItemWidth(-browse_btn_w - ImGui::GetStyle().ItemSpacing.x);

        if (input_text_buffered(id, value)) {
            changed = true;
        }
        ImGui::SameLine();

        const fmt_buf<64> popup_id("PathPopup{}", id);

        if (const fmt_buf<64> btn_id("...##{}", id); ImGui::Button(btn_id.c_str(), ImVec2(browse_btn_w, 0))) {
            ImGui::OpenPopup(popup_id.c_str());
            state.key = popup_id.c_str();
            state.entries.clear(); // force rescan when popup opens
        }

        namespace fs = std::filesystem;
        if (ImGui::BeginPopup(popup_id.c_str())) {
            if (!default_dir.empty() && fs::is_directory(default_dir)) {
                // Scan directory once when popup opens; display cached list
                if (state.entries.empty()) {
                    try {
                        for (const auto &entry: fs::directory_iterator(default_dir)) {
                            bool match = false;
                            if (ext.empty()) {
                                match = entry.is_directory();
                            } else {
                                match = entry.is_regular_file() && entry.path().extension() == ext;
                            }
                            if (match) {
                                state.entries.push_back({entry.path().string(),
                                                         entry.path().filename().string()});
                            }
                        }
                    } catch (const fs::filesystem_error &e) {
                        Log::error("Property", "directory scan failed: ", e.what());
                        ImGui::TextUnformatted("(error reading directory)");
                    }
                }
                for (const auto &[path, name]: state.entries) {
                    if (ImGui::Selectable(name.c_str())) {
                        value   = path;
                        changed = true;
                    }
                }
            } else {
                ImGui::TextUnformatted("(directory not found)");
            }
            ImGui::EndPopup();
        }
        return changed;
    }

    // Label + InputDouble with optional min/max clamping
    [[nodiscard]]
    inline bool property_double(const char *label, const char *id, double &value, const char *fmt = "%.6g",
                                const double min         = std::numeric_limits<double>::lowest(),
                                const double max         = std::numeric_limits<double>::max(),
                                const float  label_width = property_label_width) {
        property_label(label, label_width);
        if (ImGui::InputDouble(id, &value, 0.0, 0.0, fmt)) {
            value = std::clamp(value, min, max);
            return true;
        }
        return false;
    }

    // Label + InputFloat with optional min/max clamping
    [[nodiscard]]
    inline bool property_float(const char *label, const char *id, float &value, const char *fmt = "%.3f",
                               const float min         = std::numeric_limits<float>::lowest(),
                               const float max         = std::numeric_limits<float>::max(),
                               const float label_width = property_label_width) {
        property_label(label, label_width);
        if (ImGui::InputFloat(id, &value, 0.0f, 0.0f, fmt)) {
            value = std::clamp(value, min, max);
            return true;
        }
        return false;
    }

    // Label + InputInt (int32_t) with optional min/max clamping
    [[nodiscard]]
    inline bool property_int(const char *label, const char *id, int32_t &value,
                             const int32_t min         = std::numeric_limits<int32_t>::lowest(),
                             const int32_t max         = std::numeric_limits<int32_t>::max(),
                             const float   label_width = property_label_width) {
        property_label(label, label_width);
        if (ImGui::InputInt(id, &value)) {
            value = std::clamp(value, min, max);
            return true;
        }
        return false;
    }

    // Label + InputScalar for int64_t with optional min/max clamping
    [[nodiscard]]
    inline bool property_int64(const char *label, const char *id, int64_t &value,
                               const int64_t min         = std::numeric_limits<int64_t>::lowest(),
                               const int64_t max         = std::numeric_limits<int64_t>::max(),
                               const float   label_width = property_label_width) {
        property_label(label, label_width);
        if (ImGui::InputScalar(id, ImGuiDataType_S64, &value)) {
            value = std::clamp(value, min, max);
            return true;
        }
        return false;
    }

    // Label + Checkbox
    [[nodiscard]]
    inline bool property_checkbox(const char *label, const char *id, bool &value,
                                  const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::Checkbox(id, &value);
    }

    // Label + ColorEdit4 (RGBA)
    [[nodiscard]]
    inline bool property_color(const char *label, const char *id, ImVec4 &value,
                               const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::ColorEdit4(id, &value.x);
    }

    // Label + ColorEdit3 (RGB)
    [[nodiscard]]
    inline bool property_color3(const char *label, const char *id, float col[3],
                                const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::ColorEdit3(id, col);
    }

    // Label + Combo from a range of strings. Returns true if selection changed.
    // Uses detail::c_string_range and detail::as_c_str from controls.hpp.
    template<typename R>
        requires detail::c_string_range<R> || std::same_as<R, std::span<const char *const>>
    [[nodiscard]]
    bool property_combo(const char *label, const char *id, int &idx, const R &items,
                        const float label_width = property_label_width) {
        property_label(label, label_width);
        bool        changed = false;
        const auto  sz      = static_cast<int>(std::ranges::size(items));
        const char *preview = (idx >= 0 && idx < sz) ? detail::as_c_str(items[idx]) : "<none>";
        if (const combo c{id, preview}) {
            for (int i = 0; i < sz; ++i) {
                if (ImGui::Selectable(detail::as_c_str(items[i]), i == idx)) {
                    idx     = i;
                    changed = true;
                }
            }
        }
        return changed;
    }

    // Label + SliderFloat
    [[nodiscard]]
    inline bool property_slider(const char *label, const char *id, float &value, const float min, const float max,
                                const char *fmt = "%.3f", const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::SliderFloat(id, &value, min, max, fmt);
    }

    // Label + SliderInt
    [[nodiscard]]
    inline bool property_slider_int(const char *label, const char *id, int &value, const int min, const int max,
                                    const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::SliderInt(id, &value, min, max);
    }

    // Label + DragFloat
    [[nodiscard]]
    inline bool property_drag(const char *label, const char *id, float &value, const float speed = 1.0f,
                              const float min = 0.0f, const float max = 0.0f, const char *fmt = "%.3f",
                              const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::DragFloat(id, &value, speed, min, max, fmt);
    }

    // Label + DragInt
    [[nodiscard]]
    inline bool property_drag_int(const char *label, const char *id, int &value, const float speed = 1.0f,
                                  const int min = 0, const int max = 0,
                                  const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::DragInt(id, &value, speed, min, max);
    }

    // Label + InputFloat2 (vec2)
    [[nodiscard]]
    inline bool property_vec2(const char *label, const char *id, float v[2], const char *fmt = "%.3f",
                              const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::InputFloat2(id, v, fmt);
    }

    // Label + InputFloat3 (vec3)
    [[nodiscard]]
    inline bool property_vec3(const char *label, const char *id, float v[3], const char *fmt = "%.3f",
                              const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::InputFloat3(id, v, fmt);
    }

    // Label + InputFloat4 (vec4)
    [[nodiscard]]
    inline bool property_vec4(const char *label, const char *id, float v[4], const char *fmt = "%.3f",
                              const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::InputFloat4(id, v, fmt);
    }

    // Label + SliderAngle (value in radians, display/limits in degrees)
    [[nodiscard]]
    inline bool property_angle(const char *label, const char *id, float &v_rad, const float min_deg = -360.0f,
                               const float max_deg = 360.0f, const float label_width = property_label_width) {
        property_label(label, label_width);
        return ImGui::SliderAngle(id, &v_rad, min_deg, max_deg);
    }

    // Label + InputTextMultiline for std::string
    [[nodiscard]]
    inline bool property_multiline(const char *label, const char *id, std::string &value, const float height = 80.0f,
                                   const float label_width = property_label_width) {
        property_label(label, label_width);
        return input_text_buffered(id, value, 0, ImVec2(-1, height));
    }

    // Caller-owned state for property_list joined-string cache.
    // The generation counter detects external mutations to the vector.
    struct list_editor_state {
        std::string   key;          // ImGui ID that owns this cache
        std::string   joined;       // newline-joined text for InputTextMultiline
        std::uint64_t generation{}; // compared against caller's generation to detect changes
    };

    // Multiline text editor for a vector<string> (one item per line).
    // Caches the joined string per ImGui ID; uses a generation counter to detect external changes.
    [[nodiscard]]
    inline bool property_list(const char *label, const char *id, std::vector<std::string> &items,
                              list_editor_state &state, const std::uint64_t generation = 0,
                              const float label_width = property_label_width) {
        property_label(label, label_width);

        // Rebuild joined string only when generation or ID changed
        if (state.key != id || state.generation != generation) {
            state.key        = id;
            state.generation = generation;
            state.joined.clear();
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i > 0) state.joined += '\n';
                state.joined += items[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (input_text_buffered(id, state.joined, 0, ImVec2(-1, 80))) {
            items.clear();
            std::string_view sv(state.joined);
            while (!sv.empty()) {
                const auto nl = sv.find('\n');
                if (auto line = sv.substr(0, nl); !line.empty()) {
                    items.emplace_back(line);
                }
                if (nl == std::string_view::npos) {
                    break;
                }
                sv = sv.substr(nl + 1);
            }
            return true;
        }
        return false;
    }

    // Enum property: maps enum values to display strings via a fixed-size array of pairs.
    template<typename E, std::size_t N>
        requires std::is_enum_v<E>
    [[nodiscard]]
    bool property_enum(const char *label, const char *id, E &value,
                       const std::array<std::pair<E, const char *>, N> &entries,
                       const float label_width = property_label_width) {
        property_label(label, label_width);
        int current = -1;
        for (std::size_t i = 0; i < N; ++i) {
            if (entries[i].first == value) {
                current = static_cast<int>(i);
                break;
            }
        }
        const char *preview = (current >= 0) ? entries[static_cast<std::size_t>(current)].second : "<unknown>";
        bool changed = false;
        if (const combo c{id, preview}) {
            for (std::size_t i = 0; i < N; ++i) {
                if (ImGui::Selectable(entries[i].second, static_cast<int>(i) == current)) {
                    value   = entries[i].first;
                    changed = true;
                }
            }
        }
        return changed;
    }

} // namespace imgui_util
