# Interfaces and tables

Two bridge headers let scripts work with other easyforge libraries. They are
header-only, so a bridge compiles only in a program that uses both libraries,
and neither library links the other.

| Header | Gives scripts |
|---|---|
| `<easyforge/bridges/ui_script.h>` | a `ui` interface: elements found by name, their properties, their events, and displays |
| `<easyforge/bridges/data_script.h>` | a `data` table: nodes, their properties, and undo |

## Driving an interface

```cpp
#include <easyforge/window.h>
#include <easyforge/ui.h>
#include <easyforge/script.h>
#include <easyforge/bridges/ui_script.h>

using namespace easyforge;

int main()
{
    Window window = Window::New({ .Title = "Menu", .Width = 960, .Height = 600 });

    ui::Displays pages({
        .Name = "Pages",
        .Start = "Menu",
        .Children = {
            ui::Display("Menu", { .Children = { ui::Button("Play", { .Name = "Play" }) } }),
            ui::Display("Game", { .Children = { ui::Label("Score: 0", { .Name = "Score" }) } }),
        },
    });
    window.Content = pages;

    ScriptEngine scripts = ScriptEngine::New({ .MemoryLimit = 16 * 1024 * 1024 });
    ui::DefineInterface(scripts, ui::Root::Of(window));
    if (Result<ScriptValue> ran = scripts.RunFile("menu.script"); !ran)
    {
        Log(LogLevel::Error, ran.Error());
    }

    window.OnFrame = [&](float deltaSeconds) { scripts.Update(deltaSeconds); };
    window.Run();
}
```

```script
-- menu.script
constant pages = ui.Find("Pages")

function OnPlayClicked() then
    pages.Show("Game", Fade(0.3))
    ui.Find("Score").Text = "Score: 3"
end

ui.Find("Play").OnClick = OnPlayClicked
```

`ui::DefineInterface(scripts, root)` gives scripts `ui`, whose `Find(name)`
gives the element with that `.Name`, or `nothing`. It also defines the
transitions for `Show`: `Fade(seconds)`, `Scale(seconds)`,
`Slide(seconds, from)` with `from` one of `"left"`, `"right"`, `"top"`, and
`"bottom"`, and `Cut()`. A third argument names `ui` something else.
`ui::ScriptObjectFor(element, root)` gives a script one element by itself.

[Example 06](https://github.com/cresmarmat-an/easyforge-examples/tree/main/06-scripted-interface)
runs a menu, a settings page, and a game screen this way.

### What scripts can use

Every element has:

| Member | |
|---|---|
| `Name`, `Kind` | its name and kind, such as `"Button"`; read only |
| `Visible`, `Enabled` | read and set |
| `Opacity` | read and set, from 0 to 1 |
| `Tooltip` | read and set; `nothing` removes it |
| `Find(name)` | an element inside this one |

And by kind:

| Kind | Members | Events and what they pass |
|---|---|---|
| Label | `Text`, `FontSize` | |
| Button | `Text`, `Click()` | `OnClick()` |
| Checkbox, Toggle | `Text`, `Checked` | `OnChange(checked)` |
| Slider | `Value`, `Minimum`, `Maximum`, `Step` | `OnChange(value)` |
| ProgressBar | `Value`, `Minimum`, `Maximum` | |
| TextField | `Text`, `Placeholder`, `SelectAll()` | `OnChange(text)`, `OnSubmit(text)` |
| TextArea | `Text`, `Placeholder` | `OnChange(text)` |
| Dropdown | `Options`, `Selected`, `SelectedText` | `OnChange(position)` |
| List | `Items`, `Selected`, `SelectedText` | `OnSelect(position)`, `OnActivate(position)` |
| Tabs | `Current` (a page's name) | `OnChange(page)` |
| Displays | `Current`, `Show(name, transition)`, `Back()`, `CanGoBack` | `OnChange(name)` |
| Dialog | `Title`, `Open()`, `Close()`, `IsOpen` | `OnClosed()` |
| Menu | `Text`, `Open()`, `Close()`, `IsOpen` | |

An event is set by assigning a function, and `nothing` removes it. The function
receives what the table says, so a slider's handler takes one parameter:

```script
ui.Find("Volume").OnChange = function(value) then
    ui.Find("VolumeText").Text = "Volume: {Round(value * 100)}%"
end
```

Positions in dropdowns and lists count from 1, as in script lists, and `nothing`
means none is chosen. `Options` and `Items` are lists of strings. `SelectedText`
and `CanGoBack` are read only.

A script function that fails while handling an event does not stop the
program: its error is written to the log, with the file and line.

## Changing a table

```cpp
#include <easyforge/data.h>
#include <easyforge/script.h>
#include <easyforge/bridges/data_script.h>

using namespace easyforge;

Table game = Table::New();
Node player = game.Add("Player");
player["Health"] = 100;

ScriptEngine scripts = ScriptEngine::New();
scripts.Define("game", ScriptObjectFor(game));
scripts.RunFile("rules.script");
```

```script
-- rules.script
constant player = game.Find("Player")
player.Health -= 10

constant sword = player.Add("Sword")
sword.Damage = 12
sword.Tint = #FF8800

for child in player.Children() then
    print(child.Name, child.Path)      -- Sword Player/Sword
end
```

`ScriptObjectFor(table)` gives scripts the table, and `ScriptObjectFor(node)`
one node. Changes scripts make are ordinary table changes: they are recorded,
undone, saved, shown by `ui::Bind`, and shared over the network like any
other.

The table has:

| Member | Does |
|---|---|
| `Find(path)` | the node at a path such as `"Player/Sword"`, or `nothing` |
| `Add(name)`, `Add(name, type)` | adds a top-level node, of a type when given |
| `TopNodes()`, `Nodes()` | the top-level nodes, or every node |
| `NodesOfType(type)`, `NodesWith(property)` | the nodes of a type, or that have a property |
| `Version` | the table's version, which grows with every change |
| `BeginEdit(name)`, `EndEdit()` | groups changes into one step to undo |
| `Undo()`, `Redo()` | undoes or redoes a step; `true` when there was one |

A node has:

| Member | Does |
|---|---|
| `Name`, `Type`, `Path`, `Parent` | read only |
| `Children()`, `Child(name)` | its children, or one by name |
| `Add(name)`, `Add(name, type)` | adds a child |
| `Remove()`, `Rename(name)` | removes or renames the node |
| `Has(property)` | `true` when the node has the property |
| `Get(property)`, `Set(property, value)` | reads and sets any property |

Any other name after a dot is a property: `player.Health` reads `Health` and
`player.Health = 90` sets it. A property whose name is one of the node's own
members, such as a property called `Name`, is read with `Get("Name")` and set
with `Set("Name", value)`.

Properties come to scripts as the language's values. Whole numbers and numbers
become numbers, vectors become tables with `X`, `Y`, `Z`, and `W`, and colors
tables with `Red`, `Green`, `Blue`, and `Alpha`. Going back, a number without a
fraction is stored as a whole number when the property is new or was whole
already, and a table with those fields becomes a vector or a color.

## Limitations

- The `ui` bridge reaches the properties above; layout, styles, and effects stay
  in C++.
- Scripts cannot make new elements yet; they find and change the ones the
  program built.
- A data property set to a list or a table without vector or color fields is
  stored as nothing.
