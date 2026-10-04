#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <easyforge/graphics/Canvas.h>
#include <easyforge/graphics/Font.h>
#include <easyforge/graphics/Scene.h>
#include <easyforge/graphics/Texture.h>
#include <easyforge/ui/Bind.h>
#include <easyforge/ui/Element.h>

namespace easyforge::ui
{
    struct LabelSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        // In points. Zero is the theme's size.
        float FontSize = 0.0f;

        // Without a color, the theme's text color.
        std::optional<easyforge::Color> Color;

        ui::TextAlignment TextAlignment = ui::TextAlignment::Start;

        // Breaks lines between words to fit the width the label is given.
        bool Wrap = false;

        // Without a font, the theme's.
        easyforge::Font Font;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;
    };

    // Text. A new line starts at each "\n".
    //
    //     ui::Label("Hello", { .FontSize = 32 })
    //     ui::Label(ui::Bind(player["Health"], "Health: {}"))
    class Label : public Element
    {
    public:
        explicit Label(std::string text = {}, const LabelSettings& settings = {});

        // Text that follows a value in a data table by itself.
        explicit Label(const ui::Binding& binding, const LabelSettings& settings = {});

        Label(const Label& other);
        Label& operator=(const Label& other);

        Property<std::string> Text;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<ui::TextAlignment> TextAlignment;
        Property<bool> Wrap;
        Property<easyforge::Font> Font;

        // A value the text follows. Assigning Text stops following it.
        Property<ui::Binding> Binding;

        static constexpr std::string_view KindName = "Label";

    protected:
        explicit Label(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct ImageSettings
    {
        std::string Name;

        // Without a size, the image's own: one point for each pixel.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        ui::ImageFit Fit = ui::ImageFit::Contain;
        easyforge::Color Tint = easyforge::Color::White;

        // Keeps this many pixels at each edge from stretching. Only with Stretch.
        float Slice = 0.0f;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;
    };

    // A picture from a file or a texture.
    //
    //     ui::Image("icon.png", { .Width = 20, .Height = 20 })
    class Image : public Element
    {
    public:
        explicit Image(std::string_view path = {}, const ImageSettings& settings = {});
        explicit Image(const easyforge::Texture& texture, const ImageSettings& settings = {});

        Image(const Image& other);
        Image& operator=(const Image& other);

        // The file the picture came from, found the way assets finds files.
        // Assigning loads another.
        Property<std::string> Source;

        Property<easyforge::Texture> Texture;
        Property<ui::ImageFit> Fit;
        Property<easyforge::Color> Tint;
        Property<float> Slice;

        static constexpr std::string_view KindName = "Image";

    protected:
        explicit Image(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // How a button looks.
    enum class ButtonStyle
    {
        // The theme's control color.
        Normal,

        // The theme's accent color, for the one action a screen is about.
        Accent,

        // No box until the pointer is on it, for toolbars.
        Subtle,
    };

    struct ButtonSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // Without padding, 14 points at the sides and 6 above and below.
        std::optional<Insets> Padding;

        ButtonStyle Style = ButtonStyle::Normal;

        // In points. Zero is the theme's size.
        float FontSize = 0.0f;

        // The text's color. Without one, the theme's, for the style.
        std::optional<easyforge::Color> Color;

        // Where the text sits: in the middle, or at the start for buttons in a
        // list.
        ui::TextAlignment TextAlignment = ui::TextAlignment::Center;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called when the button is clicked, or pressed with Enter or Space while
        // it has the keyboard.
        std::function<void()> OnClick;

        // Elements to show in place of the text, side by side, such as an icon
        // and a label.
        std::vector<Element> Children;
    };

    // A button with text, or with children of its own.
    //
    //     ui::Button("Save", { .Style = ui::ButtonStyle::Accent, .OnClick = [] { Save(); } })
    class Button : public Element
    {
    public:
        explicit Button(std::string text = {}, const ButtonSettings& settings = {});

        Button(const Button& other);
        Button& operator=(const Button& other);

        // Calls OnClick, as a click would.
        void Click() const;

        Property<std::string> Text;
        Property<ButtonStyle> Style;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<ui::TextAlignment> TextAlignment;
        Property<std::function<void()>> OnClick;

        static constexpr std::string_view KindName = "Button";

    protected:
        explicit Button(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // Settings for Checkbox and Toggle.
    struct CheckSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        bool Checked = false;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called with the new state whenever it changes, by a click or by the
        // program.
        std::function<void(bool)> OnChange;
    };

    // A box that is checked or not, with text beside it. Clicking the box or the
    // text changes it.
    class Checkbox : public Element
    {
    public:
        explicit Checkbox(std::string text = {}, const CheckSettings& settings = {});

        Checkbox(const Checkbox& other);
        Checkbox& operator=(const Checkbox& other);

        Property<std::string> Text;
        Property<bool> Checked;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(bool)>> OnChange;

        static constexpr std::string_view KindName = "Checkbox";

    protected:
        explicit Checkbox(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // A switch that slides on or off, with text beside it. Checked is whether it
    // is on.
    class Toggle : public Element
    {
    public:
        explicit Toggle(std::string text = {}, const CheckSettings& settings = {});

        Toggle(const Toggle& other);
        Toggle& operator=(const Toggle& other);

        Property<std::string> Text;
        Property<bool> Checked;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(bool)>> OnChange;

        static constexpr std::string_view KindName = "Toggle";

    protected:
        explicit Toggle(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // Settings for Slider and ProgressBar.
    struct RangeSettings
    {
        std::string Name;

        // Without a width, 200 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        float Value = 0.0f;
        float Minimum = 0.0f;
        float Maximum = 1.0f;

        // A slider only stops at multiples of the step from Minimum. Zero stops
        // anywhere.
        float Step = 0.0f;

        // Without a color, the theme's accent.
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called with the new value whenever it changes.
        std::function<void(float)> OnChange;
    };

    // A value picked by dragging a handle along a track, or with the arrow keys.
    class Slider : public Element
    {
    public:
        explicit Slider(const RangeSettings& settings = {});

        Slider(const Slider& other);
        Slider& operator=(const Slider& other);

        Property<float> Value;
        Property<float> Minimum;
        Property<float> Maximum;
        Property<float> Step;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(float)>> OnChange;

        static constexpr std::string_view KindName = "Slider";

    protected:
        explicit Slider(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // How far along something is: a bar filled from Minimum to Value.
    class ProgressBar : public Element
    {
    public:
        explicit ProgressBar(const RangeSettings& settings = {});

        ProgressBar(const ProgressBar& other);
        ProgressBar& operator=(const ProgressBar& other);

        Animated<float> Value;
        Property<float> Minimum;
        Property<float> Maximum;
        Property<std::optional<easyforge::Color>> Color;

        static constexpr std::string_view KindName = "ProgressBar";

    protected:
        explicit ProgressBar(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct TextFieldSettings
    {
        std::string Name;

        // Without a width, 200 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // Without padding, 8 points at the sides and 6 above and below.
        std::optional<Insets> Padding;

        std::string Text;

        // Shown in the theme's muted color while the field is empty.
        std::string Placeholder;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        // Shows a dot for each character, and does not copy.
        bool Password = false;

        // The most characters the field takes. Zero is no limit.
        int MaximumLength = 0;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        std::optional<float> BorderWidth;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called after every change to the text.
        std::function<void(const std::string&)> OnChange;

        // Called when Enter is pressed in the field.
        std::function<void(const std::string&)> OnSubmit;
    };

    // One line of text to type into, with a caret, selection by mouse and
    // keyboard, copy and paste, undo, and input methods for Chinese, Japanese,
    // and Korean.
    class TextField : public Element
    {
    public:
        explicit TextField(const TextFieldSettings& settings = {});

        TextField(const TextField& other);
        TextField& operator=(const TextField& other);

        void SelectAll() const;

        Property<std::string> Text;
        Property<std::string> Placeholder;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<bool> Password;
        Property<int> MaximumLength;
        Property<std::function<void(const std::string&)>> OnChange;
        Property<std::function<void(const std::string&)>> OnSubmit;

        static constexpr std::string_view KindName = "TextField";

    protected:
        explicit TextField(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct TextAreaSettings
    {
        std::string Name;

        // Without a size, 300 by 120 points. With a height that fits, it grows
        // with its text.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // Without padding, 8 points at the sides and 6 above and below.
        std::optional<Insets> Padding;

        std::string Text;

        // Shown in the theme's muted color while the area is empty.
        std::string Placeholder;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        // Breaks lines between words to fit the width. Without it, long lines
        // scroll sideways.
        bool Wrap = true;

        // The most characters the area takes. Zero is no limit.
        int MaximumLength = 0;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        std::optional<float> BorderWidth;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called after every change to the text.
        std::function<void(const std::string&)> OnChange;
    };

    // Several lines of text to type into. Enter starts a new line, Up and Down
    // move between lines, and the wheel scrolls; the rest works as in a
    // TextField.
    class TextArea : public Element
    {
    public:
        explicit TextArea(const TextAreaSettings& settings = {});

        TextArea(const TextArea& other);
        TextArea& operator=(const TextArea& other);

        void SelectAll() const;

        Property<std::string> Text;
        Property<std::string> Placeholder;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<bool> Wrap;
        Property<int> MaximumLength;
        Property<std::function<void(const std::string&)>> OnChange;

        static constexpr std::string_view KindName = "TextArea";

    protected:
        explicit TextArea(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // What a drawing area calls to draw: a function taking the canvas, or the
    // canvas and the area's size in points.
    class DrawFunction
    {
    public:
        DrawFunction() = default;

        template <typename Function>
            requires std::invocable<Function&, Canvas&, Vector2>
        DrawFunction(Function function) : Call(std::move(function))
        {
        }

        template <typename Function>
            requires(std::invocable<Function&, Canvas&> && !std::invocable<Function&, Canvas&, Vector2>)
        DrawFunction(Function function)
            : Call([drawing = std::move(function)](Canvas& canvas, Vector2) mutable { drawing(canvas); })
        {
        }

        explicit operator bool() const { return static_cast<bool>(Call); }
        void operator()(Canvas& canvas, Vector2 size) const { Call(canvas, size); }

    private:
        std::function<void(Canvas&, Vector2)> Call;
    };

    struct DrawingAreaSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        // Called every frame the area is on screen. Points start at the area's
        // top left inside its padding, and drawing is cut to that box.
        DrawFunction OnDraw;
    };

    // An area to draw into with a Canvas, as with a renderer of your own.
    //
    //     ui::DrawingArea({
    //         .Width = ui::Fill,
    //         .Height = 200,
    //         .OnDraw = [](Canvas& canvas) {
    //             canvas.Line({ 0, 0 }, { 100, 50 }, { .Color = Color::White, .Width = 2 });
    //         },
    //     })
    class DrawingArea : public Element
    {
    public:
        explicit DrawingArea(const DrawingAreaSettings& settings = {});

        DrawingArea(const DrawingArea& other);
        DrawingArea& operator=(const DrawingArea& other);

        Property<DrawFunction> OnDraw;

        static constexpr std::string_view KindName = "DrawingArea";

    protected:
        explicit DrawingArea(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct SceneViewSettings
    {
        std::string Name;

        // Without a size, it fills.
        Size Width = ui::Fill;
        Size Height = ui::Fill;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        std::optional<float> CornerRadius;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        std::optional<easyforge::Cursor> Cursor;
    };

    // A 3D scene drawn through its camera. It does not use pointer events, so
    // clicks on it reach the program's own controls.
    class SceneView : public Element
    {
    public:
        explicit SceneView(const easyforge::Scene& scene = {}, const SceneViewSettings& settings = {});

        SceneView(const SceneView& other);
        SceneView& operator=(const SceneView& other);

        Property<easyforge::Scene> Scene;

        static constexpr std::string_view KindName = "SceneView";

    protected:
        explicit SceneView(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };
}
