/// @file drag_drop.hpp
/// @brief Type-safe drag-drop payload helpers.
///
/// Uses existing RAII drag_drop_source/drag_drop_target wrappers from core/raii.hpp.
///
/// Usage:
/// @code
///   // In a drag source:
///   imgui_util::drag_drop::source("ITEM", my_index, "Dragging item");
///
///   // In a drag target:
///   if (const imgui_util::drag_drop_target tgt{}) {
///       if (auto val = imgui_util::drag_drop::accept_payload<int>("ITEM")) {
///           handle_drop(*val);
///       }
///   }
///
///   // Peek without accepting (for hover preview):
///   if (auto val = imgui_util::drag_drop::peek_payload<int>("ITEM")) { ... }
/// @endcode
#pragma once

#include <cstring>
#include <imgui.h>
#include <optional>
#include <string_view>
#include <type_traits>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::drag_drop {

    /// @brief Maximum size in bytes for a drag-drop payload value.
    constexpr std::size_t max_payload_bytes = 1024;

    /// @brief Constraint for types usable as drag-drop payloads.
    template<typename T>
    concept payload = std::is_trivially_copyable_v<T> && sizeof(T) <= max_payload_bytes;

    /**
     * @brief Set the current drag-drop payload.
     * @tparam T     Trivially copyable payload type.
     * @param type   Payload type identifier string.
     * @param value  Value to store in the payload.
     * @param cond   ImGui condition for setting the payload.
     */
    template<typename T>
        requires payload<T>
    void set_payload(const char *type, const T &value, const ImGuiCond cond = 0) noexcept {
        ImGui::SetDragDropPayload(type, &value, sizeof(T), cond);
    }

    /**
     * @brief Accept a drag-drop payload of the given type.
     * @tparam T     Expected payload type.
     * @param type   Payload type identifier string to match.
     * @param flags  ImGui drag-drop flags.
     * @return The payload value, or std::nullopt if no matching payload was accepted.
     */
    template<typename T>
        requires payload<T>
    [[nodiscard]] auto accept_payload(const char *type, const ImGuiDragDropFlags flags = 0) noexcept
        -> std::optional<T> {
        if (const auto *const pl = ImGui::AcceptDragDropPayload(type, flags)) {
            T value;
            std::memcpy(&value, pl->Data, sizeof(T));
            return value;
        }
        return std::nullopt;
    }

    /**
     * @brief Peek at the current drag-drop payload without accepting it.
     *
     * Useful for showing a hover preview while a drag is in progress.
     *
     * @tparam T    Expected payload type.
     * @param type  Payload type identifier string to match.
     * @return The payload value, or std::nullopt if no matching payload is active.
     */
    template<typename T>
        requires payload<T>
    [[nodiscard]] auto peek_payload(const char *type) noexcept -> std::optional<T> {
        if (const auto *const pl = ImGui::GetDragDropPayload()) {
            if (pl->IsDataType(type)) {
                T value;
                std::memcpy(&value, pl->Data, sizeof(T));
                return value;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Begin a drag-drop source with a payload and text preview.
     * @tparam T            Trivially copyable payload type.
     * @param type          Payload type identifier string.
     * @param value         Value to store in the payload.
     * @param preview_text  Text shown in the drag tooltip.
     * @param flags         ImGui drag-drop flags.
     */
    template<typename T>
        requires payload<T>
    void source(const char *type, const T &value, const std::string_view preview_text,
                const ImGuiDragDropFlags flags = 0) noexcept {
        if (const drag_drop_source src{flags}) {
            set_payload(type, value);
            ImGui::TextUnformatted(preview_text.data(), preview_text.data() + preview_text.size());
        }
    }

    /**
     * @brief Begin a drag-drop source with a payload and custom tooltip callback.
     * @tparam T            Trivially copyable payload type.
     * @tparam TooltipFn    Callable rendering custom drag tooltip content.
     * @param type          Payload type identifier string.
     * @param value         Value to store in the payload.
     * @param tooltip_fn    Callback invoked to render the drag tooltip.
     * @param flags         ImGui drag-drop flags.
     */
    template<typename T, std::invocable TooltipFn>
        requires payload<T>
    void source(const char *type, const T &value, TooltipFn &&tooltip_fn,
                const ImGuiDragDropFlags flags = 0) noexcept {
        if (const drag_drop_source src{flags}) {
            set_payload(type, value);
            std::forward<TooltipFn>(tooltip_fn)();
        }
    }

} // namespace imgui_util::drag_drop
