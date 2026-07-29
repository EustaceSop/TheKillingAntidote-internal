# TheKillingAntidote — internal DLL (ImGui + DX11)

Full rewrite of the base template for **The Killing Antidote** (UE4 4.27.2 build `+++UE4+Release-4.27`).
All offsets, class layouts and UFunction names come from a Dumper-7 SDK dump of the shipping build.
Menu built on the imgui-dx9-sample-menu, converted to DirectX 11.

## Layout

```
offsets.h / sdk_types.h / sdk.{h,cpp} / game.{h,cpp} / features.{h,cpp}   ← game engine + cheat features
hooks.{h,cpp}                                                              ← MinHook + D3D11 Present hook + ImGui glue
render.{cpp,hpp} / ctx.hpp                                                  ← menu shell (from sample)
uicomponents/  checkbox / colorpicker / combo / tab                        ← reusable widgets
tabs/          player / movement / world / aimbot / visuals / misc         ← individual menu tabs
imgui/         core ImGui + Win32 backend  (+ DX11 backend to download)
dllmain.cpp / spoof.h
```

## Third-party you must add

**1. MinHook** — https://github.com/TsudaKageyu/minhook

Either link `libMinHook.x64.lib` and add its `include/` to Include Directories,
or drop the whole `src/` (+ `include/MinHook.h`) directly into the VS project.

**2. ImGui DX11 backend** — matching the 1.75 core that's already in `imgui/`:

```
https://raw.githubusercontent.com/ocornut/imgui/v1.75/examples/imgui_impl_dx11.h
https://raw.githubusercontent.com/ocornut/imgui/v1.75/examples/imgui_impl_dx11.cpp
```

Drop both into `imgui/` next to `imgui_impl_win32.*`. See `imgui/imgui_impl_dx11_README.txt`.

## Build (Visual Studio 2019/2022)

1. New project → **Dynamic-Link Library (.dll)**, **x64**, **Release**, C++17 or later.
2. Add every `.cpp` under the project root, `tabs/`, `uicomponents/`, `imgui/` (except `imgui_demo.cpp` if you want smaller output).
3. **Project → Properties → C/C++ → General → Additional Include Directories**:
   `.`, `imgui`, `tabs`, `uicomponents`, `<path-to-minhook-include>`
4. **Linker → Input → Additional Dependencies**: `libMinHook.x64.lib` (skip if you compiled MinHook sources).
5. **C/C++ → Code Generation → Runtime Library**: `/MT` (Multi-threaded) — no CRT DLL dependency.
6. **C/C++ → Language → C++ Language Standard**: `ISO C++17` or later.
7. Build.

`d3d11.lib` and `dxgi.lib` are pulled via `#pragma comment(lib,…)` in `hooks.cpp` — no manual linking needed.

## Inject

Any user-mode injector (Xenos, GH Injector, `LoadLibrary` from a helper). The DLL waits up to ~60s for GObjects/world to be ready.

## Menu

* **INSERT** — toggle menu (fades in/out)
* Tabs: **Player · Movement · World · Aim · Visuals · Misc**
* Every checkbox in the menu is mirrored into `features::Config` (atomic), which the background loop applies each tick.
* Hotkeys (F1-F12) still work when the menu is closed — see `dllmain.cpp`.
* Middle-click any `[key]` button in the menu to rebind; right-click for mode (always / hold / toggle).
* NUM 0 = clean unload.

## Extending

- **Add a feature**: add a field to `ctx_t` (ctx.hpp) + a matching atomic in `features::Config` (features.h). Draw the widget in a tab, mirror it in `SyncCtxToFeatures()` (hooks.cpp), and apply it in `features::Loop` (features.cpp).
- **UFunction call**: `game::` wrappers use `sdk::CachedFn("Function Pkg.Class.Fn")`. Copy the pattern from `game.cpp`.
- **ESP boxes**: read `features::GetEntities()` inside `RenderFrame` (hooks.cpp) and draw with `ImGui::GetForegroundDrawList()`. You still need a `Project()` helper — grab camera POV from `APlayerCameraManager` (`off::pc::PlayerCameraManager`) then apply a standard perspective matrix.

## Warnings

- **Thread safety**: `ProcessEvent` isn't fully safe from a non-game thread. The feature loop runs at 30Hz. If you see rare crashes, move PE-heavy work into the D3D11 Present hook — the game thread is stalled inside `Present` so it's safe there.
- **Steam leaderboard**: `AMcoreSteamActor::UploadScore` will accept manipulated values, but the server tags them and you'll get flagged.

## Reference

Full field/offset/UFunction inventory: `分析.md` in this folder.
