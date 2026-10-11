# Threads

What each type allows when a program uses more than one thread. Each library's
overview says the same for its own types.

Three kinds of rule come up:

| Rule | Means |
|---|---|
| Any thread | Every function can be called from any thread, at the same time as others |
| One thread at a time | Any thread may use it, but never two at once; give each thread its own, or guard it with a lock of your own |
| Its owner's thread | Only the thread named, usually the one that made it or the one running the window |

Values, such as vectors, colors, images, and messages, follow the usual C++
rule: reading one from several threads at once is safe, and changing one while
another thread uses it is not.

## core

| Type | Rule |
|---|---|
| Vectors, matrices, quaternions, transforms, rectangles, boxes, colors, `Result` | Values |
| `Random`, `Clock`, `FrameTimer` | One thread at a time |
| `Log`, `SetLogHandler` | Any thread; messages reach the handler one at a time, on the thread that logged |
| `Jobs` | Any thread; `Run`, `ParallelFor`, and a job's `Wait` can be used from inside jobs too |
| `PathFromText`, `TextFromPath` | Any thread |
| `Host`, `View`, `HostListener` | Their window's thread, except `Host::RequestFrame`, from any thread |
| `Property` | As the object it belongs to |
| Tests | The test runner's thread |

## assets

| Type | Rule |
|---|---|
| `ImageData`, `SoundData`, `FontData`, `ModelData` | Values; `Load`, `Decode`, and `LoadInBackground` from any thread |
| `Pending` | One thread at a time |
| `Files` | Any thread, including reading while another thread mounts |
| Compression and checksums | Any thread |

## window

| Type | Rule |
|---|---|
| `Window` and its properties | The thread that made it, normally the main one; `RequestFrame` from any thread |
| `Monitor` | The window thread |

## input

| Type | Rule |
|---|---|
| `Controls` | The thread of the window it follows |
| `Binding` | A value |

## graphics

| Type | Rule |
|---|---|
| `Renderer`, `Canvas`, `Scene` | The thread that made the renderer |
| `Texture`, `Font`, `Model`, `Shader` | Made and loaded on any thread; `Texture::Update` on the renderer's thread |
| `Font::Measure`, `Font::LineHeight` | Any thread |

## data

| Type | Rule |
|---|---|
| `Table` and its `Node`s | One thread at a time |
| `DataValue` | A value |

## ui

| Type | Rule |
|---|---|
| Elements, `Root`, `Theme` | The thread of the window that shows them; a root without a window, the thread that draws it |

Work done on another thread reaches an interface by keeping its result
somewhere the window's thread reads, such as in `OnFrame`, and asking for a
frame with `window.RequestFrame()`.

## script

| Type | Rule |
|---|---|
| `ScriptEngine`, `ScriptValue` | One thread at a time, the same for an engine and its values; the window's thread for an engine made with a window |
| `ScriptObject` | Called on the thread running its engine |

## sound

| Type | Rule |
|---|---|
| `Mixer`, `Sound`, and what `Play` returns | Any thread |

## physics

| Type | Rule |
|---|---|
| `Physics2D`, and its bodies and joints | One thread at a time, the same for a world and everything in it |

## network

| Type | Rule |
|---|---|
| `Server`, `Client`, `Connection` | Any thread |
| Handlers | Run on the thread that calls `Update`, on the window's thread for an end made with a window, or on the network's own thread for a threaded end |
| `Message`, `Reply` | Values |
| `Smoothed`, `SmoothedAngle` | One thread at a time |
| Shared tables | One thread at a time, the table's; share them only with ends that are not threaded |
