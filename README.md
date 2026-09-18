# Crown

A 2D game engine in C++, built from scratch on OpenGL. Windows only.

![The Crown editor](docs/editor.gif)

An editor with a scene hierarchy, a properties panel and a viewport you can click
into; sprites from any image you point it at; Box2D physics; and scenes that save
to plain text. Games are written in C++ against a small client API — the engine
knows nothing about your game.

## Building

Requires Visual Studio 2026 with the C++ desktop workload.

```
git clone --recursive https://github.com/KarnaTheGameDev/Crown.git
```

Already cloned without `--recursive`:

```
git submodule update --init --recursive
```

Then generate the projects and open the solution:

```
GenerateProject.bat
```

Set **Sandbox** as the startup project the first time; premake does not mark one.

Assets are found by walking up from the executable, so it does not matter whether
you launch from Visual Studio, from `bin/`, or by double-clicking.

## Writing a game

Everything you write lives in `Sandbox/src/SandboxApp.cpp`. You subclass
`Application` and override what you need. `#include <Crown.h>` brings in the whole
API, plus glm.

```cpp
#include <Crown.h>

class MyGame : public Crown::Application
{
public:
    void OnPlay() override                       // Play pressed: build the level
    {
        Crown::Entity& player = CreateEntity("Player");
        player.Position = { 0.0f, 0.0f, 0.0f };
        player.Scale = { 0.2f, 0.2f };
        player.Texture = "assets/textures/player.png";
        player.Body = Crown::Entity::BodyType::Dynamic;
        player.FixedRotation = true;             // stop it toppling over
        m_PlayerID = player.ID;                  // keep the id, never a pointer
    }

    void OnUpdate(float dt) override             // once per frame while playing
    {
        // The simulation owns the position of anything with a body, so ask for
        // a speed instead of assigning to Position - the next step undoes that.
        float walk = 0.0f;
        if (Crown::Input::IsKeyPressed(Crown::Key::Left))  walk -= 1.0f;
        if (Crown::Input::IsKeyPressed(Crown::Key::Right)) walk += 1.0f;

        glm::vec2 velocity = GetVelocity(m_PlayerID);
        SetVelocity(m_PlayerID, { walk * 2.0f, velocity.y });   // leave the fall alone

        bool standing = velocity.y > -0.05f && velocity.y < 0.05f;
        if (standing && Crown::Input::IsKeyPressed(Crown::Key::Space))
            SetVelocity(m_PlayerID, { walk * 2.0f, 3.5f });     // jump, if standing on something
    }

    void OnCollision(uint32_t a, uint32_t b, bool began) override
    {
        if (began) CROWN_INFO("{0} touched {1}", a, b);
    }

private:
    uint32_t m_PlayerID = 0;
};

Crown::Application* Crown::CreateApplication() { return new MyGame(); }
```

### The client API

| | |
|---|---|
| `OnUpdate(float dt)` | Every frame **while playing**. Not called in Edit. |
| `OnPlay()` / `OnStop()` | Play pressed / stopped. Build your level in `OnPlay`. |
| `OnCollision(a, b, began)` | Two entities started or stopped touching. |
| `OnImGuiRender()` | Draw your own panels — score, debug, tools. |
| `CreateEntity(name)` | Returns a reference. Read `.ID` from it immediately. |
| `FindEntity(id)` | `nullptr` once the entity is gone. |
| `FindOverlapping(e, out)` | Every entity overlapping this one, by id. No simulation needed. |
| `SetVelocity(id, v)` / `GetVelocity(id)` | Drive a body. Metres per second. |
| `ApplyImpulse(id, v)` | A shove. Mass-dependent, unlike `SetVelocity`. |
| `Teleport(id, pos, deg)` | Move something outright: respawn, screen wrap, a door. |
| `SetGravity(v)` | Zero for top-down or space. Applies mid-play. |
| `DestroyEntity(id)` | Removes it. **Invalidates every `Entity*` you hold.** |
| `GetEntities()` | The whole scene, if you want to iterate it. |
| `Crown::Input::IsKeyPressed(Crown::Key::Space)` | Keyboard. Returns false while an editor field has focus. |
| `CROWN_INFO("x is {0}", x)` | Logging. Also `CROWN_WARN`, `CROWN_ERROR`. |

Two rules that matter:

**Hold ids, not pointers.** `CreateEntity` and `DestroyEntity` both move the
underlying vector, so any `Entity*` you kept across one of those calls is
dangling. Look entities up by id each time you need them.

**Put game state on the entity.** `Entity::UserData` is a `std::any` the engine
never reads, so a struct of your own travels with the entity and dies with it:

```cpp
struct Bullet { glm::vec2 Velocity; float Life; };

bullet.UserData = Bullet{ direction * 3.0f, 1.5f };

if (Bullet* b = std::any_cast<Bullet>(&e.UserData))
    e.Position += glm::vec3(b->Velocity * dt, 0.0f);
```

It is not saved to the scene file — gameplay state is transient, and a saved
scene is an arrangement of things rather than the middle of a game.

## Adding art

Drop a PNG anywhere under `assets/`, select an entity, and use **Browse…** in the
Properties panel. Paths are stored relative to the project so a scene still opens
on someone else's machine.

For a sprite sheet, set **Sheet columns/rows** and drag **Cell** — it writes the
UV rectangle for you. `Sprite rect` is that rectangle directly: `x y` is the
origin and `z w` the size, both 0 to 1. **Whole image** resets it.

An entity with no texture draws as a plain white quad multiplied by its **Tint**,
so it is visible before you have any art. A path that fails to load does the same
and says why in the log, once — a broken asset never takes the program down.

**Reload from disk** re-reads the image files, so you can edit a PNG in another
program and see it update without restarting.

## Physics

Set **Body** on an entity to give it a rigid body:

| | |
|---|---|
| **None** | Not simulated. The default. |
| **Static** | Never moves. Floors, walls. |
| **Dynamic** | Falls, collides, gets pushed. |
| **Kinematic** | Moves only when you move it, but pushes dynamic bodies. |

`Collider size` multiplies `Scale`, so the collider follows the sprite unless you
say otherwise. `Sensor` reports overlaps without pushing anything — pickups,
triggers, goals. `Fixed rotation` stops a body toppling, which is what most
characters want. Gravity is under the **Scene** menu, and changing it there
reaches a running scene immediately rather than waiting for the next Play.

Physics only runs while playing, so nothing drifts while you are arranging a
scene. Bodies are built when Play is pressed and destroyed on Stop. Entities you
spawn mid-play need `AddPhysicsBody(entity)` once you have filled their fields in.

### Driving a body

While playing, the step writes each body's transform onto its entity every
frame, so assigning to `Position` fights the solver and loses. Go through the
simulation instead:

```cpp
SetVelocity(id, { 2.0f, GetVelocity(id).y });   // walk, keep falling
ApplyImpulse(id, { 3.0f, 1.0f });               // a shove; heavier bodies move less
Teleport(id, spawn);                            // respawn, wrap, a door
SetGravity({ 0.0f, 0.0f });                     // top-down or space
```

`SetVelocity` is exact whatever the body weighs, which is what a character
wants; `ApplyImpulse` is mass-dependent, which is what an explosion wants.
`Teleport` also works on an entity with no body, since then nothing else would
move it.

Two scenes to open and press Play:
`assets/scenes/physics-test.crown` is a floor and three boxes falling.
`assets/scenes/platformer.crown` is a level — **Left/Right** walk, **Space**
jumps, the crates can be shoved, and walking off the end respawns you. Its
controller is about thirty lines in `Sandbox`, which is all any of this takes.

## The editor

**Play / Pause / Stop** in the toolbar. Play copies the scene and runs the game on
the copy; Stop throws the copy away and restores exactly what you had. Nothing a
game does can damage what you authored.

Click a sprite in the viewport to select it, drag to move it. **Add** and
**Delete** are in the Hierarchy. The window title shows the open scene and a `*`
when there are unsaved changes.

| | |
|---|---|
| `Ctrl+N` / `Ctrl+O` | New scene / Open |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As |
| `W` `A` `S` `D` | Move the editor camera |
| `Q` `E` | Rotate the editor camera |

## Dependencies

| | | |
|---|---|---|
| [GLFW](https://github.com/TheCherno/glfw) | window and input | submodule |
| [glm](https://github.com/g-truc/glm) | vector and matrix maths | submodule |
| [Dear ImGui](https://github.com/ocornut/imgui) | editor UI (docking branch) | submodule |
| [Box2D](https://github.com/erincatto/box2d) | 2D physics (v3.1.1) | submodule |
| [spdlog](https://github.com/gabime/spdlog) | logging | submodule |
| [glad](https://github.com/Dav1dde/glad) | OpenGL 4.6 core loader | vendored source |
| [stb_image](https://github.com/nothings/stb) | image loading | vendored header |

premake5 is committed under `vendor/bin/premake/`, so no separate download.

### Regenerating glad

glad is generated output, not a submodule, so there is no upstream to trace it
back to. To reproduce `Crown/vendor/Glad`:

```
python -m glad --api gl:core=4.6 --extensions="" c
```

Extensions are excluded deliberately: including all 623 takes the generated
source from 345 KB to 1.5 MB for entry points nothing calls.

## Scene format

Line-oriented text, no serialisation library:

```
crown-scene 1
entity
id 1
name Floor
pos 0 -0.75 0
rot 0
scale 2.2 0.12
sprite 0 0 1 1
tint 0.35 0.4 0.5 1
body 1
collider 1 1
material 1 0.5 0
bodyflags 0 0
```

`body` is 0 None, 1 Static, 2 Dynamic, 3 Kinematic. `material` is density,
friction, bounciness. `bodyflags` is fixed rotation then sensor. Physics lines
are only written when the entity has a body, and `texture` only when one is set.

Unknown keys are skipped, so a file written by a later build still loads what
this one understands. A higher version number in the header is rejected outright.
Loading parses into a temporary and only replaces the scene on success; saving
writes alongside the target and renames over it.

## What is deliberately not built

These are decisions, not gaps. Each one is cheap to add later and would be a
guess today:

- **No scripting runtime.** Games are C++ against the client API.
- **No Renderer / RendererAPI abstraction.** One backend, no batching, no sort
  step. Three layers of indirection over `glDrawElements` would buy nothing.
- **No VertexArray / BufferLayout classes.** One mesh, one vertex layout.
- **No ECS.** `Entity` is a plain struct in a `std::vector`, with `UserData` for
  whatever the game needs. Box2D holds the simulation state.
- **No layer stack.** There are no layers.
- **No batching.** One draw call per entity, still not a measured problem. If it
  becomes one, instancing is about ten lines.
- **No asset GUIDs or import pipeline.** Paths are enough.
- **No raycasts, joints or collision filtering.** The one place a ray would earn
  its keep is a ground check, and comparing vertical speed is close enough until
  it isn't. Box2D supports all three, and each is a few lines added to
  `PhysicsWorld` the day a game actually needs one.

## Layout

```
Crown/src/Crown/          engine
  Renderer/               Shader, Texture2D, TextureLibrary, Framebuffer, OrthographicCamera
  Physics/                PhysicsWorld
  Scene/                  Entity, SceneSerializer
  Events/                 event types and dispatcher
Crown/src/Platform/       Windows implementations, including the file dialogs
Sandbox/                  the sample game - copy this to start your own
assets/                   textures and scenes
```

`Sandbox` is Asteroids, written entirely against the API above. It is the worked
example: read it, then replace it.
