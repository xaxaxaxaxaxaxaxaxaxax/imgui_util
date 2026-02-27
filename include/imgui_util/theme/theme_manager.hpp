/// @file theme_manager.hpp
/// @brief Theme persistence, preset registry, and built-in editor window.
///
/// Usage:
/// @code
///   theme_manager mgr;
///   mgr.set_theme(theme_config::from_preset(my_preset));
///   mgr.save_to_file("theme.json");
///   mgr.load_from_file("theme.json");
///   mgr.render_theme_editor(&show_editor);      // call each frame to show the editor
/// @endcode

#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "imgui_util/theme/theme.hpp"

namespace imgui_util::theme {

    /// @brief Generate a valid C++ theme_preset{...} initializer string from a theme_config.
    [[nodiscard]] std::string generate_preset_code(const theme_config &cfg);

    /**
     * @brief Validate a theme_config, returning a list of human-readable error strings.
     *
     * Checks for: alpha values out of [0,1], fully transparent window background,
     * nonsensical rounding values, etc.
     */
    [[nodiscard]] std::vector<std::string> validate(const theme_config &cfg);

    /// @brief Theme persistence, preset registry, and built-in editor window.
    class theme_manager {
    public:
        static constexpr int file_version = 1; ///< Bumped on breaking serialization changes.

        theme_manager();

        /// @brief Apply and store a theme as the current theme.
        void set_theme(theme_config theme);
        [[nodiscard]] theme_config       &get_current_theme() { return current_theme_; }
        [[nodiscard]] const theme_config &get_current_theme() const { return current_theme_; }

        /// @brief Return all built-in presets.
        [[nodiscard]] static std::span<const theme_preset> get_presets();
        /**
         * @brief Build a theme_config from a named built-in preset.
         * @param name Preset name (must match an entry from get_presets()).
         */
        [[nodiscard]] static theme_config get_preset(std::string_view name);
        /**
         * @brief Look up a built-in preset by name.
         * @param name Preset name.
         * @return Pointer to the preset, or nullptr if not found.
         */
        [[nodiscard]] static const theme_preset *find_preset(std::string_view name);

        /**
         * @brief Serialize the current theme to a JSON file.
         * @param path Destination file path.
         * @return True on success.
         */
        [[nodiscard]] bool save_to_file(const std::filesystem::path &path);
        /**
         * @brief Load a theme from a JSON file and apply it.
         * @param path Source file path.
         * @return True on success.
         */
        [[nodiscard]] bool load_from_file(const std::filesystem::path &path);

        /// @brief Render the built-in theme editor window. Call each frame when open.
        void render_theme_editor(bool *open);

    private:
        theme_config    current_theme_;
        theme_config    editing_theme_; // scratch copy while editor is open
        bool            live_preview_ = true;
        ImGuiTextFilter color_filter_;

        void        render_color_category(const char *name, std::span<const int> indices);
        void        render_node_colors();
        static void render_sizes_tab();
        static void render_fonts_tab();
        static void render_rendering_tab();
    };

} // namespace imgui_util::theme
