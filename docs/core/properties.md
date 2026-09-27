# Properties

A property is a setting of an object that you read and assign like a variable.
The object hears about every change and can react to it: a window changes its
title bar when its `Title` property is assigned.

```cpp
window.Title = "Notes - untitled.txt";      // the window updates its title bar
std::string title = window.Title;           // reads the current title

button.Width += 20;
if (button.Text == "Save")
{
    button.Enabled = false;
}
```

The objects that have properties arrive with the libraries after `core`. This
page explains how properties behave, so they are familiar when they appear, and
how to give your own objects properties.

## Reading and writing

| Written | What happens |
|---|---|
| `object.Setting = value` | Sends the new value to the object |
| `Type value = object.Setting` | Reads the current value |
| `object.Setting.Get()` | Reads the current value |
| `object.Setting == value`, `!=` | Compares the current value |
| `object.Setting += value`, `-=`, `*=`, `/=` | Reads, changes, and writes back, when the value type supports the operator |
| `object.Setting->Member` | Reads a member of the value, such as `window.Size->X` |
| `first.Setting = second.Setting` | Copies the value from one object to the other |

## A property is not a copy

`auto title = window.Title;` does not compile. A property belongs to its object
and cannot be copied away from it. Write the type instead
(`std::string title = window.Title;`) or call `Get()`.

For the same reason, members of a value can be read through `->` but not
assigned one at a time. `window.Size->X` reads the width, but to change it,
assign the whole value: `window.Size = { 800, window.Size->Y };`.

## Assigning through const

Objects in easyforge are handles, so a `const` handle still refers to an object
that can change. Properties follow that: they can be assigned through a `const`
handle. This is what lets a lambda that captured a handle by value change it:

```cpp
ui::Label status("Ready");
ui::Button save("Save", { .OnClick = [status] { status.Text = "Saved"; } });
```

## Properties on your own types

A property needs the object it belongs to and two functions: one that reads the
value and one that writes it. The functions receive the object as a `void*`, so
any type can own properties.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

struct Thermostat
{
    float Target = 20.0f;
};

class ThermostatHandle
{
public:
    explicit ThermostatHandle(Thermostat& thermostat)
        : Target(&thermostat,
              [](const void* owner) { return static_cast<const Thermostat*>(owner)->Target; },
              [](void* owner, const float& value) {
                  static_cast<Thermostat*>(owner)->Target = Clamp(value, 5.0f, 30.0f);
              })
    {
    }

    Property<float> Target;
};

Thermostat thermostat;
ThermostatHandle handle(thermostat);
handle.Target = 45.0f;             // stored as 30, the highest allowed
```

The functions must be plain functions or lambdas without captures, which keeps
a property as small as three pointers.

A handle class that can be copied has to point its properties at the copied
object. `Property::Rebind(owner)` does that; call it for each property in the
handle's copy constructor and copy assignment.

## Limitations

- Reading a property calls its read function every time, so read it once into a
  variable when you need the value repeatedly in a tight loop.
- Members of a value cannot be assigned one at a time, as explained above.
