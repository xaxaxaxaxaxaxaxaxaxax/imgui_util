// toast.hpp - Toast notification system with stacking and fade-out
//
// Usage:
//   imgui_util::toast::show("Saved successfully");
//   imgui_util::toast::show("Connection lost", imgui_util::severity::error);
//   imgui_util::toast::show("Low memory", imgui_util::severity::warning, 5.0f);
//
//   // With action button:
//   imgui_util::toast::show("File deleted", imgui_util::severity::info, 5.0f,
//                           "Undo", [&]{ undo_delete(); });
//
//   // Call once per frame (typically at end of frame, after other UI):
//   imgui_util::toast::render();
//
// Toasts stack from the bottom-right corner and fade out in the last 0.5s.
#pragma once

#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/theme/dynamic_colors.hpp"
#include "imgui_util/widgets/severity.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util::toast {

    enum class position { bottom_right, top_right, bottom_left, top_left };

    namespace detail {

        struct entry {
            std::string                     text;
            severity                        sev;
            float                           start_time;
            float                           duration;
            std::string                     action_label;
            std::move_only_function<void()> action_callback;
            ImVec2                          cached_text_size;
            float                           cached_action_w;
        };

        struct toast_state {
            std::vector<entry> entries;
            position           anchor      = position::bottom_right;
            int                max_visible = 10;
        };

        [[nodiscard]]
        inline auto &state() noexcept {
            static toast_state s;
            return s;
        }

        [[nodiscard]]
        inline ImVec4 color_for(const severity sev) noexcept {
            switch (sev) {
                case severity::info:
                    return theme::info_color();
                case severity::warning:
                    return theme::warning_color();
                case severity::error:
                    return theme::error_color();
                case severity::success:
                    return theme::success_color();
            }
            return theme::info_color();
        }

    } // namespace detail

    inline void set_position(const position pos) noexcept {
        detail::state().anchor = pos;
    }

    inline void set_max_visible(const int max) noexcept {
        detail::state().max_visible = max;
    }

    inline void show(const std::string_view message, const severity sev = severity::info,
                     const float duration_sec = 3.0f, const std::string_view action_label = {},
                     std::move_only_function<void()> action_callback = {}) {
        constexpr float toast_padding = 12.0f;
        const ImVec2    text_size = ImGui::CalcTextSize(message.data(), message.data() + message.size(), false, 300.0f);
        const float     action_w =
            action_label.empty()
                    ? 0.0f
                    : ImGui::CalcTextSize(action_label.data(), action_label.data() + action_label.size()).x +
                      ImGui::GetStyle().FramePadding.x * 2.0f + toast_padding;
        detail::state().entries.push_back({
            .text             = std::string(message),
            .sev              = sev,
            .start_time       = static_cast<float>(ImGui::GetTime()),
            .duration         = duration_sec,
            .action_label     = std::string(action_label),
            .action_callback  = std::move(action_callback),
            .cached_text_size = text_size,
            .cached_action_w  = action_w,
        });
    }

    inline void render() {
        auto &[entries, anchor, max_visible] = detail::state();
        if (entries.empty()) return;

        const auto   now      = static_cast<float>(ImGui::GetTime());
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;

        constexpr float padding = 12.0f;

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoDocking;

        const bool anchor_bottom = anchor == position::bottom_right || anchor == position::bottom_left;
        const bool anchor_right  = anchor == position::bottom_right || anchor == position::top_right;

        // Walk entries, stacking from anchor corner
        float y_offset    = padding;
        int   visible_cnt = 0;

        for (size_t i = entries.size(); i-- > 0;) {
            constexpr float fade_time = 0.5f;
            constexpr float min_width = 200.0f;
            constexpr float spacing   = 6.0f;
            if (visible_cnt >= max_visible) break;

            auto &[text, sev, start_time, duration, action_label, action_callback, cached_text_size, cached_action_w] =
                entries[i];
            const float elapsed = now - start_time;
            if (elapsed >= duration) continue;

            // Fade alpha only in the last fade_time seconds
            const float  remaining = duration - elapsed;
            const float  alpha = remaining < fade_time ? linear_fade_alpha(fade_time - remaining, fade_time) : 1.0f;
            const ImVec4 col   = detail::color_for(sev);

            const style_var alpha_var(ImGuiStyleVar_Alpha, alpha);

            const float win_w = std::max(cached_text_size.x + cached_action_w + padding * 2.0f, min_width);
            const float win_h = cached_text_size.y + padding * 2.0f;

            const float  pos_x = anchor_right ? viewport.x - win_w - padding : padding;
            const float  pos_y = anchor_bottom ? viewport.y - y_offset - win_h : y_offset;
            const ImVec2 pos{pos_x, pos_y};

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize({win_w, win_h});

            const fmt_buf<32> win_id{"##toast_{}", i};

            if (const window w{win_id.c_str(), nullptr, flags}) {
                // Left accent bar
                auto *dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled({pos.x, pos.y}, {pos.x + 3.0f, pos.y + win_h}, ImGui::ColorConvertFloat4ToU32(col));

                // Text with wrapping
                const text_wrap_pos wrap(pos.x + win_w - padding);
                colored_text(text, colors::text_primary);

                // Action button
                if (!action_label.empty() && action_callback) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(action_label.c_str())) {
                        action_callback();
                        duration = 0.0f; // dismiss after action
                    }
                }

                // Click to dismiss
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                    duration = 0.0f;
                }
            }

            y_offset += win_h + spacing;
            ++visible_cnt;
        }

        // Remove expired entries
        std::erase_if(entries, [now](const detail::entry &e) { return now - e.start_time >= e.duration; });
    }

    // Clear all active toasts
    inline void clear() noexcept {
        detail::state().entries.clear();
    }

} // namespace imgui_util::toast
