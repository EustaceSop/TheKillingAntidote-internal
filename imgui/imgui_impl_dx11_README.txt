Missing files — DOWNLOAD THESE TWO before building:

The imgui core in this folder is version 1.75. Use the matching-era DX11 backend:

  https://raw.githubusercontent.com/ocornut/imgui/v1.75/examples/imgui_impl_dx11.h
  https://raw.githubusercontent.com/ocornut/imgui/v1.75/examples/imgui_impl_dx11.cpp

Place them right next to this file (i.e. under imgui/).

Notes
-----
* Location was `examples/` in v1.75; ImGui moved to `backends/` in 1.80+.
* DO NOT mix a newer backend with the 1.75 core — struct layouts and function
  signatures changed. Either upgrade both together, or keep both at 1.75.
* If you'd rather upgrade to a modern ImGui (recommended), replace every file
  in this imgui/ folder with the latest, then use imgui_impl_dx11 from the same
  release tag. You may also need to fix `ListBoxHeader`/`ListBoxFooter` in
  tabs/profile.cpp — those were renamed to `BeginListBox`/`EndListBox`.
