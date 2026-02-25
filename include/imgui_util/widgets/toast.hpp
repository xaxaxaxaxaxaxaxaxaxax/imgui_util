// toast.hpp - Toast notification system with stacking and fade-out
//
// Usage:
//   imgui_util::toast::show("Saved successfully");
//   imgui_util::toast::show("Connection lost", imgui_util::toast::level::error);
//   imgui_util::toast::show("Low memory", imgui_util::toast::level::warning, 5.0f);
//
//   // Call once per frame (typically at end of frame, after other UI):
//   imgui_util::toast::render();
//
// Toasts stack from the bottom-right corner and fade out in the last 0.5s.
#pragma once

#include <array>
#include <imgui.h>
#include <string>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util::toast {

    enum class level { info, success, warning, error };

    enum class position { bottom_right, top_right, bottom_left, top_left };

    namespace detail {

        struct entry {
            std::string text;
            level       lvl;
            float       start_time;
            float       duration;
        };

        inline std::vector<entry> &entries() {
            static std::vector<entry> e;
            return e;
        }

        inline position &anchor() {
            static position p = position::bottom_right;
            return p;
        }

        inline int &max_visible() {
            static int m = 10;
            return m;
        }

        [[nodiscard]]
        constexpr ImVec4 color_for(level lvl) noexcept {
            constexpr std::array<ImVec4, 4> colors_map = {{
                imgui_util::colors::accent,  // info
                imgui_util::colors::success, // success
                imgui_util::colors::warning, // warning
                imgui_util::colors::error,   // error
            }};
            return colors_map[static_cast<std::size_t>(lvl)];
        }

    } // namespace detail

    inline void set_position(position pos) { detail::anchor() = pos; }

    inline void set_max_visible(int max) { detail::max_visible() = max; }

    inline void show(std::string message, level lvl = level::info, float duration_sec = 3.0f) {
        detail::entries().push_back({
            std::move(message),
            lvl,
            static_cast<float>(ImGui::GetTime()),
            duration_sec,
        });
    }

    inline void render() {
        auto &entries = detail::entries();
        if (entries.empty()) return;

        const float  now      = static_cast<float>(ImGui::GetTime());
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;

        constexpr float padding    = 12.0f;
        constexpr float spacing    = 6.0f;
        constexpr float min_width  = 200.0f;
        constexpr float fade_time  = 0.5f;

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoDocking;

        const auto  anchor    = detail::anchor();
        const bool  anchor_bottom = (anchor == position::bottom_right || anchor == position::bottom_left);
        const bool  anchor_right  = (anchor == position::bottom_right || anchor == position::top_right);
        const int   max_vis   = detail::max_visible();

        // Walk entries, stacking from anchor corner
        float y_offset    = padding;
        int   visible_cnt = 0;

        for (size_t i = entries.size(); i-- > 0;) {
            if (visible_cnt >= max_vis) break;

            auto       &e       = entries[i];
            const float elapsed = now - e.start_time;
            if (elapsed >= e.duration) continue;

            // Fade alpha only in the last fade_time seconds
            const float remaining = e.duration - elapsed;
            const float alpha     = (remaining < fade_time) ? linear_fade_alpha(fade_time - remaining, fade_time) : 1.0f;
            const ImVec4 col      = detail::color_for(e.lvl);

            const style_var alpha_var(ImGuiStyleVar_Alpha, alpha);

            // Compute text size to auto-size the window
            const ImVec2 text_size = ImGui::CalcTextSize(e.text.c_str(), nullptr, false, 300.0f);
            const float  win_w     = (std::max)(text_size.x + padding * 2.0f, min_width);
            const float  win_h     = text_size.y + padding * 2.0f;

            const float pos_x = anchor_right ? (viewport.x - win_w - padding) : padding;
            const float pos_y = anchor_bottom ? (viewport.y - y_offset - win_h) : y_offset;
            const ImVec2 pos{pos_x, pos_y};

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize({win_w, win_h});

            const fmt_buf<32> win_id{"##toast_{}", i};

            if (const window w{win_id.c_str(), nullptr, flags}) {
                // Left accent bar
                auto *dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(
                    {pos.x, pos.y},
                    {pos.x + 3.0f, pos.y + win_h},
                    ImGui::ColorConvertFloat4ToU32(col));

                // Text with wrapping
                const text_wrap_pos wrap(pos.x + win_w - padding);
                colored_text(e.text, colors::text_primary);

                // Click to dismiss
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                    e.duration = 0.0f;
                }
            }

            y_offset += win_h + spacing;
            ++visible_cnt;
        }

        // Remove expired entries
        std::erase_if(entries, [now](const detail::entry &e) {
            return (now - e.start_time) >= e.duration;
        });
    }

    // Clear all active toasts
    inline void clear() {
        detail::entries().clear();
    }

} // namespace imgui_util::toast
