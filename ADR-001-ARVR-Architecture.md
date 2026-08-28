# ADR-001: Crown Engine — AR/VR Target Architecture

**Status:** Proposed  
**Date:** 2026-05-29  
**Deciders:** Engine team  

---

## Context

Crown is a C++ game engine currently targeting Windows (x64) via a shared-lib + sandbox architecture. The goal is to pivot the engine's primary focus to **AR/VR application development** — producing high-quality, optimized experiences for headsets and mixed-reality devices. This ADR captures the foundational decisions that shape every subsequent subsystem: renderer, input, audio, and runtime.

AR/VR has unique hard constraints that differ from traditional game engines:

- **Latency is perceptual.** Frame-to-photon latency above ~20 ms causes nausea. The entire pipeline must be designed around this.
- **Stereo rendering doubles GPU load** unless handled with single-pass or instanced techniques.
- **Reprojection / ATW (Asynchronous TimeWarp)** can rescue missed frames but only if the engine exposes depth and motion vectors correctly.
- **Mixed Reality passthrough** requires camera feed compositing at the driver level, with precise pose timestamps.

---

## Decision

Crown will be architected as an **OpenXR-first, Vulkan-primary** engine with a layered abstraction designed for low-latency stereo rendering.

---

## Options Considered

### Option A: OpenXR + Vulkan (Recommended)

| Dimension         | Assessment |
|-------------------|------------|
| Complexity        | High       |
| XR Coverage       | All major headsets (Quest, Pico, Varjo, HoloLens, SteamVR) |
| Performance       | Highest — explicit GPU control, multiview, async compute |
| Future-proofing   | High — Khronos standard, broad industry adoption |
| Team familiarity  | Medium — steeper learning curve than OpenGL |

**Pros:**
- OpenXR is the cross-vendor XR standard; one codebase targets Quest, SteamVR, WMR, HoloLens
- Vulkan's explicit model enables multiview rendering (single draw call for both eyes), async compute for timewarp, and precise GPU timing
- Vulkan supports `VK_KHR_multiview` — the single most important optimization for stereo rendering
- OpenXR's `XrSwapchain` integrates directly with Vulkan surfaces — no extra copy path
- Supports foveated rendering via `XR_FB_foveation` / `XR_VARJO_foveated_rendering` extensions

**Cons:**
- Vulkan requires explicit synchronization, memory management, and render pass setup — significant upfront investment
- OpenXR is less documented than platform SDKs; debugging is harder
- No built-in fallback for older hardware that lacks Vulkan support

---

### Option B: Platform SDKs + OpenGL/DirectX

| Dimension         | Assessment |
|-------------------|------------|
| Complexity        | Medium     |
| XR Coverage       | Per-platform (Meta SDK for Quest, OpenVR for SteamVR, etc.) |
| Performance       | Medium — implicit driver overhead, no multiview |
| Future-proofing   | Low — fragmented; each SDK has its own update cadence |
| Team familiarity  | High — more tutorials, simpler API surface |

**Pros:**
- Faster to get something running on a specific headset
- More sample code and community resources for each platform SDK
- DirectX 12 is a viable Vulkan alternative on Windows-only targets

**Cons:**
- Requires maintaining a separate backend per platform
- No standard multiview support across SDKs
- Meta, Valve, and Microsoft all have diverging roadmaps
- Locks Crown to platform SDK update cycles

---

### Option C: WebXR (JavaScript/WASM)

Ruled out. Insufficient performance headroom for a native engine targeting quality and optimization.

---

## Trade-off Analysis

The core tension is **speed of iteration vs. long-term performance and portability**.

Option B gets you to a working demo faster on one headset. But as soon as you add a second target platform, maintenance burden doubles. Option A costs more upfront but the investment pays off with every new OpenXR-compliant device (and there will be many — Apple Vision Pro, future Meta headsets, enterprise AR).

On performance: `VK_KHR_multiview` alone can cut stereo draw call cost by 40–50% compared to rendering each eye separately. For a quality-focused engine, this is not optional.

---

## Architecture Consequences

### What becomes easier
- Adding new XR devices as they release (just update the OpenXR runtime)
- Implementing advanced features: foveated rendering, hand tracking (`XR_EXT_hand_tracking`), eye tracking, passthrough AR
- GPU profiling and optimization via RenderDoc (excellent Vulkan support)
- Frame pacing and reprojection — Vulkan gives precise control over present timing

### What becomes harder
- Initial bringup — need a working Vulkan renderer before anything XR is visible
- Debugging — Vulkan validation layers are verbose; OpenXR error messages are sparse
- Cross-compilation to non-Vulkan targets (e.g., older mobile) requires a fallback renderer

### What we'll need to revisit
- Audio spatialization (OpenXR has no audio API — needs a separate solution, e.g., Steam Audio or Meta Spatial Audio SDK)
- Physics (VR interactions require sub-frame physics updates for hand presence — standard fixed-step loops aren't sufficient)
- Multiplatform build (Android/Quest requires NDK + Gradle alongside the current Windows/Premake setup)

---

## Recommended Subsystem Roadmap

```
Phase 1 — Foundation (current)
├── Event system          ✅ Done
├── Window abstraction    ✅ Done (GLFW — will be replaced by XrSession for headset)
└── Logging               ✅ Done

Phase 2 — Renderer
├── Vulkan backend (device, swapchain, render pass, pipelines)
├── Abstraction layer (RHI) over Vulkan for future backends
└── Basic PBR material system

Phase 3 — XR Runtime
├── OpenXR session init (XrInstance, XrSession, XrSpace)
├── Stereo swapchain (XrSwapchain bound to Vulkan images)
├── Per-eye view projection + multiview rendering
└── Pose prediction + late-latching

Phase 4 — Interaction
├── OpenXR input system (XrActionSet, XrAction)
├── Hand tracking extension
└── Controller haptics

Phase 5 — AR Features
├── Passthrough compositing (XR_FB_passthrough / XR_HTC_passthrough)
├── Spatial anchors (XR_MSFT_spatial_anchor / XR_FB_spatial_entity)
└── Plane detection
```

---

## Action Items

1. [ ] Add Vulkan SDK to vendor dependencies and update premake5.lua
2. [ ] Replace GLFW window with a dual-mode window: desktop (GLFW) for dev, XrSession for headset
3. [ ] Implement a minimal Vulkan renderer (triangle on screen)
4. [ ] Integrate OpenXR loader as a submodule (`KhronosGroup/OpenXR-SDK`)
5. [ ] Create `XRSession` class mirroring the `Window` abstraction pattern already in Crown
6. [ ] Set up Android/Quest build pipeline (NDK, Gradle, OpenXR Android loader)
7. [ ] Evaluate audio spatialization library (Steam Audio recommended for cross-platform)
8. [ ] Document target headsets and minimum spec (Quest 3 + PC VR SteamVR as baseline)
