# Properties and animation

```cpp
ui::Label status("Ready");
ui::Button save("Save", { .OnClick = [status] { status.Text = "Saved"; } });

save.Enabled = false;
status.Opacity.AnimateTo(0.0f, { .Duration = 0.4f, .Easing = ui::Easing::Out });
```

## Elements are handles

An element is a handle: copying one is cheap, and every copy refers to the same
element. Keep a copy of any element you want to change later, before or after
it is on screen; capturing one by value in a lambda is safe. An element lives
as long as a copy of it does, or as long as it is inside an interface.

Every setting in an element's settings struct is also a property of the element,
read and assigned like a variable:

```cpp
save.Text = "Save all";
std::string text = save.Text;
save.Width = ui::Fill;
save.Padding = { 20, 8 };
save.Background = Color::Hex("#2E5AAC");
save.OnClick = [] { /* ... */ };
```

Write the type you read into; `auto text = save.Text;` does not compile. See
[properties](../core/properties.md).

`ui::Element` is the type every element converts to: a list of children holds
any of them, and a function can return any of them.

```cpp
ui::Element Card(std::string title, std::string body)
{
    return ui::Panel({ .Padding = 16, .CornerRadius = 12, .Effects = { ui::Shadow {} },
        .Children = { ui::Label(title, { .FontSize = 20 }), ui::Label(body, { .Wrap = true }) } });
}
```

| Written | Gives |
|---|---|
| `element.Kind()` | Its kind, such as `"Button"` |
| `element.Is<ui::Button>()` | Whether it is a button |
| `element.As<ui::Button>()` | The element as a button, or no element when it is another kind |
| `element.Parent()`, `element.Children()` | The element it is in, and the elements in it |
| `element.Frame()` | Where it was last placed, in points from the top left of the window |
| `element.IsHovered()`, `IsPressed()`, `IsFocused()` | Its state |
| `element == other` | Whether both refer to the same element |

A default-constructed `ui::Element` is no element; it tests as false.

## Finding elements

```cpp
ui::Button("Save", { .Name = "Save" });
// ... later, anywhere:
ui::Button save = ui::Root::Of(window).Find<ui::Button>("Save");
ui::Label title = panel.Find<ui::Label>("Title");
```

`Find` returns the first element with the name, of the kind asked for, or no
element. `root.Find("Save")` finds any kind. Names do not have to be unique.

## Animation

Three properties of every element move gradually with `AnimateTo`:

| Property | Default | What it does |
|---|---|---|
| `Opacity` | 1 | Fades the element and everything in it |
| `Offset` | 0, 0 | Moves it, in points |
| `Scale` | 1 | Scales it around its center |

A progress bar's `Value` and a root's `Theme` animate too.

```cpp
panel.Offset.AnimateTo({ 0, -20 }, { .Duration = 0.3f, .Easing = ui::Easing::Spring });
panel.Scale.AnimateTo(1.1f, { .Duration = 0.2f, .Delay = 0.5f, .OnFinished = [] { /* ... */ } });
```

| AnimationSettings | Default | Meaning |
|---|---|---|
| `Duration` | 0.25 | Seconds |
| `Easing` | `Out` | `Linear`, `In` (starts slowly), `Out` (slows at the end), `InOut`, or `Spring` (goes past the end and back) |
| `Delay` | 0 | Seconds to wait before starting |
| `OnFinished` | none | Called when the animation reaches its end, not when something stops it |

An animation starts from the value the property has when it begins to move,
after any delay. Starting another animation of the same property replaces the
first; assigning the property stops it. Animations run while the element is in
an interface that is drawing.

`ui::Ease(easing, progress)` gives the eased value of a progress from 0 to 1,
for animating things of your own the same way.

## Drawn but not moved

`Opacity`, `Offset`, and `Scale` change how an element is drawn, not where it is
laid out: the elements around it stay where they are, and there is no layout
work while they animate. Clicks follow the element where it is drawn.

## Limitations

- Only `Opacity`, `Offset`, `Scale`, a progress bar's `Value`, and a root's
  `Theme` animate. Sizes and colors of single elements change at once.
