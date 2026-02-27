/// @file modal_builder.hpp
/// @brief Fluent modal dialog builder with ok/cancel and danger mode.
///
/// Usage:
/// @code
///   // Full builder (call every frame):
///   imgui_util::modal_builder("Delete Item")
///       .message("Are you sure you want to delete this item?")
///       .ok_button("Delete", [&]{ delete_item(); })
///       .cancel_button("Cancel", [&]{ cancel(); })
///       .danger()
///       .render(&show_modal);
///
///   // Convenience:
///   imgui_util::confirm_dialog("Confirm", "Really delete?", &open,
///       [&]{ do_delete(); }, [&]{ cancelled(); });
/// @endcode
#pragma once

#include <functional>
#include <imgui.h>
#include <optional>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/theme/color_math.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    /**
     * @brief Fluent builder for modal popup dialogs.
     *
     * Call render() exactly once per frame. render() handles OpenPopup / BeginPopupModal /
     * CloseCurrentPopup internally based on *open.
     */
    class modal_builder {
    public:
        explicit modal_builder(const char *title) noexcept : title_(title) {}

        /// @brief Set the message text displayed at the top of the dialog.
        [[nodiscard]] modal_builder &message(const char *text) noexcept {
            message_ = text;
            return *this;
        }

        /// @brief Set a custom body callback rendered below the message.
        [[nodiscard]] modal_builder &body(std::move_only_function<void()> fn) noexcept {
            body_ = std::move(fn);
            return *this;
        }

        /// @brief Set a fixed window size instead of auto-resize.
        [[nodiscard]] modal_builder &size(const float w, const float h) noexcept {
            width_  = w;
            height_ = h;
            return *this;
        }

        /**
         * @brief Set the OK/confirm button.
         * @param label  Button text.
         * @param on_ok  Callback invoked when the user confirms.
         */
        template<std::invocable F>
        [[nodiscard]] modal_builder &ok_button(const char *label, F &&on_ok) noexcept {
            ok_label_ = label;
            on_ok_    = std::forward<F>(on_ok);
            return *this;
        }

        /**
         * @brief Set the cancel button.
         * @param label      Button text.
         * @param on_cancel  Callback invoked when the user cancels.
         */
        template<std::invocable F>
        [[nodiscard]] modal_builder &cancel_button(const char *label, F &&on_cancel) noexcept {
            cancel_label_ = label;
            on_cancel_    = std::forward<F>(on_cancel);
            show_cancel_  = true;
            return *this;
        }

        /// @brief Enable danger mode (red-tinted OK button).
        [[nodiscard]] modal_builder &danger(const bool d = true) noexcept {
            danger_ = d;
            return *this;
        }

        /// @brief Render the modal dialog. Call once per frame.
        void render(bool *open) noexcept {
            if (open != nullptr && *open) {
                ImGui::OpenPopup(title_);
            }

            if (width_ > 0.0f) {
                ImGui::SetNextWindowSize({width_, height_}, ImGuiCond_Appearing);
            }

            const ImGuiWindowFlags win_flags =
                width_ > 0.0f ? ImGuiWindowFlags_None : ImGuiWindowFlags_AlwaysAutoResize;

            if (const popup_modal modal{title_, open, win_flags}) {
                if (message_ != nullptr) {
                    const text_wrap_pos wrap(0.0f);
                    ImGui::TextUnformatted(message_);
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                if (body_) {
                    body_();
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                {
                    if (danger_) {
                        std::optional<style_colors> danger_style;
                        danger_style.emplace(std::initializer_list<style_color_entry>{
                            {ImGuiCol_Button, colors::error_dark},
                            {ImGuiCol_ButtonHovered, colors::error},
                            {ImGuiCol_ButtonActive, color::offset(colors::error, 0.1f)}});
                    }
                    if (ImGui::Button(ok_label_, {120, 0})) {
                        dismiss(open, on_ok_);
                    }
                }

                if (show_cancel_) {
                    ImGui::SameLine();
                    if (ImGui::Button(cancel_label_, {120, 0})) {
                        dismiss(open, on_cancel_);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                    dismiss(open, on_ok_);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    dismiss(open, on_cancel_);
                }
            }
        }

    private:
        static void dismiss(bool *open, std::move_only_function<void()> &callback) noexcept {
            if (callback) callback();
            if (open != nullptr) *open = false;
            ImGui::CloseCurrentPopup();
        }

        const char                     *title_        = "";
        const char                     *message_      = nullptr;
        const char                     *ok_label_     = "OK";
        const char                     *cancel_label_ = "Cancel";
        bool                            danger_       = false;
        bool                            show_cancel_  = false;
        float                           width_        = 0.0f;
        float                           height_       = 0.0f;
        std::move_only_function<void()> on_ok_;
        std::move_only_function<void()> on_cancel_;
        std::move_only_function<void()> body_;
    };

    /**
     * @brief Convenience wrapper: render a simple OK/Cancel confirmation dialog.
     * @param title    Dialog title.
     * @param message  Message text.
     * @param open     Visibility flag (set to false on dismiss).
     * @param on_yes   Callback invoked on OK.
     * @param on_no    Callback invoked on Cancel.
     */
    template<std::invocable OnYes, std::invocable OnNo>
    void confirm_dialog(const char *title, const char *message, bool *open, OnYes &&on_yes, OnNo &&on_no) noexcept {
        modal_builder(title)
            .message(message)
            .ok_button("OK", std::forward<OnYes>(on_yes))
            .cancel_button("Cancel", std::forward<OnNo>(on_no))
            .render(open);
    }

} // namespace imgui_util
