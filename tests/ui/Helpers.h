#pragma once

// What the ui tests share: a root drawn into an image on the software renderer,
// with the test font, so text measures the same on every computer.

#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>
#include <easyforge/ui.h>

namespace easyforge::testing
{
    inline ui::Theme TestTheme()
    {
        ui::Theme theme = ui::Theme::Light();
        theme.Font = Font::Load(EASYFORGE_TEST_FONT);
        theme.FontSize = 20.0f;
        return theme;
    }

    // A root and an image to draw it into.
    struct Screen
    {
        Screen(int width, int height, ui::Element content, float scale = 1.0f)
            : Renderer(easyforge::Renderer::New(
                  { .Adapter = GraphicsAdapter::Software, .Width = width, .Height = height, .Scale = scale })),
              Root(ui::Root::New({ .Content = content, .Theme = TestTheme() }))
        {
        }

        // Draws a frame `seconds` after the last.
        void Frame(float seconds = 1.0f / 60.0f)
        {
            ui::Theme theme = Root.Theme;
            Canvas canvas = Renderer.BeginFrame(theme.Background);
            Root.Draw(canvas, seconds);
            Renderer.EndFrame();
        }

        // Draws frames for a while, as a window running would.
        void Run(float seconds)
        {
            for (float passed = 0.0f; passed < seconds - 0.0001f; passed += 1.0f / 60.0f)
            {
                Frame(1.0f / 60.0f);
            }
        }

        ImageData Picture()
        {
            Frame(0.0f);
            return Renderer.Capture();
        }

        bool Send(EventType type, Vector2 position = {}, KeyModifiers modifiers = {})
        {
            Event event;
            event.Type = type;
            event.Position = position;
            event.Modifiers = modifiers;
            return Root.HandleEvent(event);
        }

        bool Press(Vector2 position, int clickCount = 1)
        {
            Event event;
            event.Type = EventType::MouseButtonPressed;
            event.Position = position;
            event.ClickCount = clickCount;
            return Root.HandleEvent(event);
        }

        bool Release(Vector2 position) { return Send(EventType::MouseButtonReleased, position); }
        bool Move(Vector2 position) { return Send(EventType::MouseMoved, position); }

        bool Click(Vector2 position)
        {
            Move(position);
            bool used = Press(position);
            Release(position);
            return used;
        }

        // Two clicks in a row, as the system reports a double-click.
        void DoubleClick(Vector2 position)
        {
            Click(position);
            Press(position, 2);
            Release(position);
        }

        bool Key(easyforge::Key key, KeyModifiers modifiers = {})
        {
            Event event;
            event.Type = EventType::KeyPressed;
            event.Key = key;
            event.Modifiers = modifiers;
            bool used = Root.HandleEvent(event);
            event.Type = EventType::KeyReleased;
            Root.HandleEvent(event);
            return used;
        }

        bool Type(std::string_view text)
        {
            Event event;
            event.Type = EventType::TextEntered;
            event.Text = std::string(text);
            return Root.HandleEvent(event);
        }

        bool Wheel(Vector2 position, Vector2 amount)
        {
            Event event;
            event.Type = EventType::MouseWheel;
            event.Position = position;
            event.Wheel = amount;
            return Root.HandleEvent(event);
        }

        easyforge::Renderer Renderer;
        ui::Root Root;
    };

    inline std::string PixelAt(const ImageData& image, int x, int y)
    {
        return image.ColorAt(x, y).ToHex();
    }

    inline bool NearColor(Color actual, Color expected, float tolerance = 0.02f)
    {
        return NearlyEqual(actual, expected, tolerance);
    }
}
