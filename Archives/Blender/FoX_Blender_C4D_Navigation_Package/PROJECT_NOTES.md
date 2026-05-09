# FoX Blender C4D Navigation Project Notes

Date: 2026-05-09

## Current Status

This package freezes the Blender-side experiment before moving the main workflow toward Unreal Engine.

Addon name:

- `fox_c4d_navigation`

Blender target:

- Blender 5.0

Current addon version:

- `0.24.2`

Installed source path on this machine:

- `C:\Users\Oplus\AppData\Roaming\Blender Foundation\Blender\5.0\scripts\addons\fox_c4d_navigation\__init__.py`

Packaged copy:

- `FoX_Blender_C4D_Navigation_Package\fox_c4d_navigation\__init__.py`

## Goal

The goal was to make Blender feel closer to Cinema 4D for common viewport, transform, coordinate, and material workflows.

The project later shifted direction because Unreal Engine already covers much of the desired final-scene workflow, especially material assets, scene assembly, lighting, camera work, and rendering.

## Implemented Features

### C4D-Style Navigation

- Added C4D-like viewport navigation mappings.
- Added Alt + mouse navigation support.
- Added Alt + right mouse drag zoom with left/right movement synchronized to up/down movement.
- Restored native-feeling zoom scale after several sensitivity iterations.

### C4D-Style Transform Tool Keys

- `W` selects Move tool.
- `E` selects Rotate tool.
- `R` selects Scale tool.
- `S` frames selected object instead of Blender scale.
- Transform gizmos are shown instead of immediately applying transform operations.

### FoX Sidebar Panels

The active usable area is the 3D View sidebar:

- Open with `N`
- Go to the `FoX` tab

Panel ordering:

- `FoX Coordinates`
- `FoX Material Manager`
- `FoX C4D Navigation`

`FoX C4D Navigation` is default closed and contains helper controls such as creating a FoX workspace.

### FoX Coordinates

The coordinate panel was simplified based on actual use:

- Removed the separate non-useful World block.
- Kept Object transform controls:
  - Position
  - Rotation
  - Scale
  - Size
- In Mesh Edit Mode, selected point/edge/face displays only:
  - `World Position`

`World Position` is implemented as a live Blender property:

- Getter reads the current selected mesh element world position directly.
- Setter moves the selected mesh element to the typed world coordinate.

This solved the issue where the field appeared but started at `0, 0, 0`.

### FoX Material Manager

Implemented an early material manager prototype:

- Shows material ball count.
- Adds a new material ball.
- Duplicates selected material.
- Deletes selected material.
- Displays materials in a 3-column grid.
- Clicking a material selects it and assigns it to the current selected object.
- In Mesh Edit Mode, clicking a material assigns it to selected faces.
- Eyedropper-style assign mode can assign a material to an object under the mouse.

This was useful as an experiment, but it is no longer the preferred direction because Unreal already has a stronger material asset workflow.

## Known Limitations

- Blender UI draw/update behavior made coordinate fields tricky; the final `World Position` live property works better than storing a synced scene value.
- Timeline-based panels were explored but should not be relied on. The preferred UI is now only the 3D View `N` sidebar.
- Material manager is still a prototype and not as natural as Unreal or C4D material workflows.
- This addon should be considered an experiment, not the final FoX workflow platform.

## Usage

Install folder:

```text
C:\Users\Oplus\AppData\Roaming\Blender Foundation\Blender\5.0\scripts\addons\fox_c4d_navigation
```

To install from this package:

1. Copy `fox_c4d_navigation` into Blender's addons folder.
2. Open Blender.
3. Enable the addon from Preferences > Add-ons.
4. In the 3D View, press `N`, then open the `FoX` tab.

## Direction Decision

The Blender route proved that C4D-like UI and interaction changes are possible, but it also showed the cost of fighting Blender's existing workflow.

The better next step is Unreal Engine:

- Keep C4D as an optional asset creation tool.
- Use Unreal as the main final-scene workspace.
- Build a `FoX Tools` Unreal Editor plugin that collects existing Unreal functions into a custom, comfortable workspace.

Unreal plugin direction:

- Custom `FoX Tools` panel.
- Saved FoX editor layout.
- Fast transform and selection tools.
- Material asset shortcuts.
- Common light/camera/empty creation buttons.
- Later: viewport/navigation/shortcut tuning.

