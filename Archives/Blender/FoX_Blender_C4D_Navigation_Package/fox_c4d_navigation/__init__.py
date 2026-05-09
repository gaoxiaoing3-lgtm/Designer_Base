bl_info = {
    "name": "FoX C4D Navigation",
    "author": "FoX / Codex",
    "version": (0, 24, 2),
    "blender": (5, 0, 0),
    "location": "3D Viewport",
    "description": "Cinema 4D style viewport navigation shortcuts for Blender.",
    "category": "3D View",
}

import bpy
import bmesh
from mathutils import Euler, Vector
from bpy_extras import view3d_utils


addon_keymaps = []
disabled_keymap_items = []


def _active_fox_material_name(context):
    scene = context.scene
    return getattr(scene, "fox_material_manager_active", "")


def _active_fox_material(context):
    return bpy.data.materials.get(_active_fox_material_name(context))
def _is_plain_key(item, event_type):
    return (
        item.type == event_type
        and item.value == "PRESS"
        and not item.any
        and not item.shift
        and not item.ctrl
        and not item.alt
        and item.key_modifier == "NONE"
    )


def _disable_item(item):
    if item.active:
        item.active = False
        disabled_keymap_items.append(item)


def _disable_conflicting_default_shortcuts():
    window_manager = bpy.context.window_manager
    keyconfigs = [
        window_manager.keyconfigs.active,
        window_manager.keyconfigs.default,
        window_manager.keyconfigs.user,
    ]

    for keyconfig in filter(None, keyconfigs):
        for keymap in keyconfig.keymaps:
            for item in keymap.keymap_items:
                if item.idname == "screen.frame_offset" and item.alt and item.type in {
                    "WHEELUPMOUSE",
                    "WHEELDOWNMOUSE",
                }:
                    _disable_item(item)
                elif item.idname == "transform.rotate" and _is_plain_key(item, "R"):
                    _disable_item(item)
                elif item.idname == "transform.resize" and _is_plain_key(item, "S"):
                    _disable_item(item)
                elif item.idname in {
                    "view3d.edit_mesh_extrude_move_normal",
                    "armature.extrude_move",
                    "grease_pencil.extrude_move",
                } and _is_plain_key(item, "E"):
                    _disable_item(item)

    return None


def _show_view3d_toolbars():
    screen = bpy.context.screen
    if not screen:
        return None

    for area in screen.areas:
        if area.type != "VIEW_3D":
            continue
        space_data = area.spaces.active
        space_data.show_region_toolbar = True
        space_data.show_region_tool_header = True
        space_data.show_gizmo = True
        space_data.show_gizmo_tool = True
        space_data.show_gizmo_context = True
    return None


def _show_timeline_fox_panel():
    screen = bpy.context.screen
    if not screen:
        return None

    for area in screen.areas:
        if area.type != "DOPESHEET_EDITOR":
            continue
        space_data = area.spaces.active
        if hasattr(space_data, "show_region_ui"):
            space_data.show_region_ui = True
    return None


def _ensure_fox_workspace():
    if bpy.data.workspaces.get("FoX C4D"):
        return None

    window = bpy.context.window
    if not window:
        return None

    current_workspace = window.workspace
    bpy.ops.workspace.duplicate()
    window.workspace.name = "FoX C4D"
    window.workspace = current_workspace
    return None


def _find_timeline_area(context):
    screen = context.screen
    if not screen:
        return None

    timeline_areas = [
        area
        for area in screen.areas
        if area.type == "DOPESHEET_EDITOR" and getattr(area.spaces.active, "mode", "") == "TIMELINE"
    ]
    if timeline_areas:
        return min(timeline_areas, key=lambda area: area.y)

    dopesheet_areas = [area for area in screen.areas if area.type == "DOPESHEET_EDITOR"]
    if dopesheet_areas:
        return min(dopesheet_areas, key=lambda area: area.y)
    return None


def _configure_transform_gizmo(space_data, mode):
    if not space_data or space_data.type != "VIEW_3D":
        return

    space_data.show_region_toolbar = True
    space_data.show_region_tool_header = True
    space_data.show_gizmo = True
    space_data.show_gizmo_context = True
    space_data.show_gizmo_tool = True
    space_data.show_gizmo_object_translate = mode == "MOVE"
    space_data.show_gizmo_object_rotate = mode == "ROTATE"
    space_data.show_gizmo_object_scale = mode == "SCALE"


def _add_view3d_keymap_item(keymap, operator, event_type, *, key_modifier=None, alt=False):
    item = keymap.keymap_items.new(
        operator,
        event_type,
        "PRESS",
        key_modifier=key_modifier or "NONE",
        alt=alt,
    )
    addon_keymaps.append((keymap, item))
    return item


def _add_tool_keymap_item(keymap, event_type, tool_name):
    item = _add_view3d_keymap_item(keymap, "fox_c4d_navigation.set_tool", event_type)
    item.properties.name = tool_name
    if tool_name == "builtin.move":
        item.properties.gizmo_mode = "MOVE"
    elif tool_name == "builtin.rotate":
        item.properties.gizmo_mode = "ROTATE"
    elif tool_name == "builtin.scale":
        item.properties.gizmo_mode = "SCALE"
    return item


def _add_zoom_keymap_item(keymap, event_type, delta):
    item = _add_view3d_keymap_item(keymap, "fox_c4d_navigation.zoom_viewport", event_type, alt=True)
    item.properties.delta = delta
    return item


def _find_view3d_override(context):
    window = context.window
    screen = context.screen
    if not window or not screen:
        return None

    for area in screen.areas:
        if area.type != "VIEW_3D":
            continue
        for region in area.regions:
            if region.type == "WINDOW":
                return {
                    "window": window,
                    "screen": screen,
                    "area": area,
                    "region": region,
                    "space_data": area.spaces.active,
                    "region_data": area.spaces.active.region_3d,
                }
    return None


def _run_in_view3d(context, operator_name, **properties):
    override = _find_view3d_override(context)
    if not override:
        return False

    with context.temp_override(**override):
        operator = bpy.ops
        for attr in operator_name.split("."):
            operator = getattr(operator, attr)
        operator(**properties)
    return True


def _format_vector(vector, suffix=""):
    return f"X {vector[0]:.3f}{suffix}   Y {vector[1]:.3f}{suffix}   Z {vector[2]:.3f}{suffix}"


def _selected_mesh_point_positions(context):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return None

    mesh = obj.data
    bm = bmesh.from_edit_mesh(mesh)
    selected_verts = {vert for vert in bm.verts if vert.select}
    selected_edges = [edge for edge in bm.edges if edge.select]
    selected_faces = [face for face in bm.faces if face.select]

    active = bm.select_history.active
    if isinstance(active, bmesh.types.BMVert):
        selected_verts.add(active)
    elif isinstance(active, bmesh.types.BMEdge):
        selected_edges.append(active)
    elif isinstance(active, bmesh.types.BMFace):
        selected_faces.append(active)

    for edge in selected_edges:
        selected_verts.update(edge.verts)
    for face in selected_faces:
        selected_verts.update(face.verts)

    selected = [vert.co.copy() for vert in selected_verts]
    if not selected:
        return None

    local = sum(selected, selected[0].copy() * 0.0) / len(selected)
    world = obj.matrix_world @ local
    return local, world, len(selected)


def _selected_mesh_element_counts(context):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return None

    bm = bmesh.from_edit_mesh(obj.data)
    return (
        sum(1 for vert in bm.verts if vert.select),
        sum(1 for edge in bm.edges if edge.select),
        sum(1 for face in bm.faces if face.select),
    )


def _selected_mesh_label(context, vert_count):
    counts = _selected_mesh_element_counts(context)
    if not counts:
        return f"Selected Element ({vert_count})"

    verts, edges, faces = counts
    if faces:
        return f"Selected Face ({faces})"
    if edges:
        return f"Selected Edge ({edges})"
    if verts == 1:
        return "Selected Point (1)"
    if verts > 1:
        return f"Selected Points ({verts})"
    return f"Selected Element ({vert_count})"


def _selection_has_transform_shape(context):
    counts = _selected_mesh_element_counts(context)
    if not counts:
        return False
    verts, edges, faces = counts
    return bool(faces or edges or verts > 1)


def _selected_mesh_vertices(context):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return None, []

    try:
        bm = bmesh.from_edit_mesh(obj.data)
    except (ReferenceError, SystemError, ValueError):
        return None, []
    selected = {vert for vert in bm.verts if vert.select}

    active = bm.select_history.active
    if isinstance(active, bmesh.types.BMVert):
        selected.add(active)
    elif isinstance(active, bmesh.types.BMEdge):
        selected.update(active.verts)
    elif isinstance(active, bmesh.types.BMFace):
        selected.update(active.verts)

    for edge in bm.edges:
        if edge.select:
            selected.update(edge.verts)
    for face in bm.faces:
        if face.select:
            selected.update(face.verts)

    return bm, list(selected)


def _selected_mesh_local_bounds(context):
    _bm, verts = _selected_mesh_vertices(context)
    if not verts:
        return None

    coords = [vert.co.copy() for vert in verts]
    minimum = Vector((min(co.x for co in coords), min(co.y for co in coords), min(co.z for co in coords)))
    maximum = Vector((max(co.x for co in coords), max(co.y for co in coords), max(co.z for co in coords)))
    center = (minimum + maximum) * 0.5
    size = maximum - minimum
    return center, size


def _selected_mesh_signature(context):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return ""

    bm = bmesh.from_edit_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    verts = {vert.index for vert in bm.verts if vert.select}
    for edge in bm.edges:
        if edge.select:
            verts.update(vert.index for vert in edge.verts)
    for face in bm.faces:
        if face.select:
            verts.update(vert.index for vert in face.verts)

    active = bm.select_history.active
    if isinstance(active, bmesh.types.BMVert):
        verts.add(active.index)
    elif isinstance(active, bmesh.types.BMEdge):
        verts.update(vert.index for vert in active.verts)
    elif isinstance(active, bmesh.types.BMFace):
        verts.update(vert.index for vert in active.verts)

    coords = []
    for index in sorted(verts):
        vert = bm.verts[index]
        coords.append(f"{index}:{vert.co.x:.6f},{vert.co.y:.6f},{vert.co.z:.6f}")

    return f"{obj.name}:{'|'.join(coords)}"


def _sync_selected_point_props(context, local, world):
    signature = _selected_mesh_signature(context)
    if not signature:
        return

    context.scene.fox_coordinate_update_lock = True
    context.scene.fox_selected_point_local = local
    context.scene.fox_selected_point_world = world
    context.scene.fox_selected_element_rotation = (0.0, 0.0, 0.0)
    context.scene.fox_selected_element_rotation_previous = (0.0, 0.0, 0.0)
    context.scene.fox_selected_element_scale = (1.0, 1.0, 1.0)
    bounds = _selected_mesh_local_bounds(context)
    if bounds:
        context.scene.fox_selected_element_size = bounds[1]
    context.scene.fox_selected_point_signature = signature
    context.scene.fox_coordinate_update_lock = False


def _move_selected_mesh_points_to_world(context, target_world):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return False

    mesh = obj.data
    bm = bmesh.from_edit_mesh(mesh)
    selected = {vert for vert in bm.verts if vert.select}
    active = bm.select_history.active
    if isinstance(active, bmesh.types.BMVert):
        selected.add(active)
    elif isinstance(active, bmesh.types.BMEdge):
        selected.update(active.verts)
    elif isinstance(active, bmesh.types.BMFace):
        selected.update(active.verts)
    for edge in bm.edges:
        if edge.select:
            selected.update(edge.verts)
    for face in bm.faces:
        if face.select:
            selected.update(face.verts)
    selected = list(selected)
    if not selected:
        return False

    current_local = sum((vert.co for vert in selected), selected[0].co.copy() * 0.0) / len(selected)
    current_world = obj.matrix_world @ current_local
    delta_world = target_world - current_world
    delta_local = obj.matrix_world.inverted().to_3x3() @ delta_world

    for vert in selected:
        vert.co += delta_local

    bmesh.update_edit_mesh(mesh)
    return True


def _move_selected_mesh_points_to_local(context, target_local):
    obj = context.object
    if not obj or obj.type != "MESH" or obj.mode != "EDIT":
        return False

    mesh = obj.data
    bm = bmesh.from_edit_mesh(mesh)
    selected = {vert for vert in bm.verts if vert.select}
    active = bm.select_history.active
    if isinstance(active, bmesh.types.BMVert):
        selected.add(active)
    elif isinstance(active, bmesh.types.BMEdge):
        selected.update(active.verts)
    elif isinstance(active, bmesh.types.BMFace):
        selected.update(active.verts)
    for edge in bm.edges:
        if edge.select:
            selected.update(edge.verts)
    for face in bm.faces:
        if face.select:
            selected.update(face.verts)
    selected = list(selected)
    if not selected:
        return False

    current_local = sum((vert.co for vert in selected), selected[0].co.copy() * 0.0) / len(selected)
    delta_local = target_local - current_local

    for vert in selected:
        vert.co += delta_local

    bmesh.update_edit_mesh(mesh)
    return True


def _selected_point_world_update(self, context):
    if getattr(context.scene, "fox_coordinate_update_lock", False):
        return
    _move_selected_mesh_points_to_world(context, context.scene.fox_selected_point_world)


def _selected_point_world_get(self):
    positions = _selected_mesh_point_positions(bpy.context)
    if not positions:
        return (0.0, 0.0, 0.0)
    _local, world, _count = positions
    return (world.x, world.y, world.z)


def _selected_point_world_set(self, value):
    if getattr(self, "fox_coordinate_update_lock", False):
        return
    _move_selected_mesh_points_to_world(bpy.context, Vector(value))


def _selected_point_local_update(self, context):
    if getattr(context.scene, "fox_coordinate_update_lock", False):
        return
    _move_selected_mesh_points_to_local(context, context.scene.fox_selected_point_local)


def _apply_selected_mesh_rotation_delta(context, delta_euler):
    _bm, verts = _selected_mesh_vertices(context)
    if not verts:
        return False

    center = sum((vert.co for vert in verts), verts[0].co.copy() * 0.0) / len(verts)
    matrix = delta_euler.to_matrix().to_4x4()
    for vert in verts:
        vert.co = center + (matrix @ (vert.co - center).to_4d()).to_3d()

    bmesh.update_edit_mesh(context.object.data)
    return True


def _apply_selected_mesh_scale(context, target_scale):
    _bm, verts = _selected_mesh_vertices(context)
    if not verts:
        return False

    current = Vector(context.scene.fox_selected_element_scale)
    factors = Vector((
        target_scale[0] / current[0] if abs(current[0]) > 0.000001 else 1.0,
        target_scale[1] / current[1] if abs(current[1]) > 0.000001 else 1.0,
        target_scale[2] / current[2] if abs(current[2]) > 0.000001 else 1.0,
    ))
    center = sum((vert.co for vert in verts), verts[0].co.copy() * 0.0) / len(verts)
    for vert in verts:
        offset = vert.co - center
        vert.co = center + Vector((offset.x * factors.x, offset.y * factors.y, offset.z * factors.z))

    bmesh.update_edit_mesh(context.object.data)
    return True


def _apply_selected_mesh_size(context, target_size):
    bounds = _selected_mesh_local_bounds(context)
    if not bounds:
        return False

    _center, current_size = bounds
    factors = Vector((
        target_size[0] / current_size[0] if abs(current_size[0]) > 0.000001 else 1.0,
        target_size[1] / current_size[1] if abs(current_size[1]) > 0.000001 else 1.0,
        target_size[2] / current_size[2] if abs(current_size[2]) > 0.000001 else 1.0,
    ))
    current_scale = Vector(context.scene.fox_selected_element_scale)
    context.scene.fox_selected_element_scale = (
        current_scale.x * factors.x,
        current_scale.y * factors.y,
        current_scale.z * factors.z,
    )
    return _apply_selected_mesh_scale(context, context.scene.fox_selected_element_scale)


def _selected_element_rotation_update(self, context):
    if getattr(context.scene, "fox_coordinate_update_lock", False):
        return

    previous = Euler(context.scene.fox_selected_element_rotation_previous)
    current = Euler(context.scene.fox_selected_element_rotation)
    delta = current.to_matrix() @ previous.to_matrix().inverted()
    if _apply_selected_mesh_rotation_delta(context, delta.to_euler()):
        context.scene.fox_selected_element_rotation_previous = current


def _selected_element_scale_update(self, context):
    if getattr(context.scene, "fox_coordinate_update_lock", False):
        return
    if _apply_selected_mesh_scale(context, context.scene.fox_selected_element_scale):
        bounds = _selected_mesh_local_bounds(context)
        if bounds:
            context.scene.fox_coordinate_update_lock = True
            context.scene.fox_selected_element_size = bounds[1]
            context.scene.fox_coordinate_update_lock = False


def _selected_element_size_update(self, context):
    if getattr(context.scene, "fox_coordinate_update_lock", False):
        return
    _apply_selected_mesh_size(context, context.scene.fox_selected_element_size)


def _draw_coordinate_panel(layout, context):
    layout.use_property_split = True
    layout.use_property_decorate = False

    obj = context.object
    if not obj:
        layout.label(text="No active object")
        return

    box = layout.box()
    box.label(text="Object")
    box.prop(obj, "location", text="Position")
    box.prop(obj, "rotation_euler", text="Rotation")
    box.prop(obj, "scale", text="Scale")
    box.prop(obj, "dimensions", text="Size")

    point_positions = _selected_mesh_point_positions(context)
    if point_positions:
        local, world, count = point_positions
        box = layout.box()
        box.use_property_split = True
        box.use_property_decorate = False
        box.label(text=_selected_mesh_label(context, count))
        _draw_xyz_editor(box, context.scene, "fox_selected_point_world_live", "World Position")
    elif obj and obj.type == "MESH" and obj.mode == "EDIT":
        box = layout.box()
        box.label(text="Selected Point")
        box.label(text="Select a vertex, edge, or face")


def _draw_xyz_editor(layout, data, prop_name, label=None):
    if label:
        layout.separator(factor=0.45)
        layout.label(text=label)
    column = layout.column(align=True)
    column.use_property_split = True
    column.use_property_decorate = False
    for axis, index in (("X", 0), ("Y", 1), ("Z", 2)):
        column.prop(data, prop_name, index=index, text=axis)


def _draw_point_editor_panel(layout, context):
    obj = context.object
    point_positions = _selected_mesh_point_positions(context)
    if not point_positions:
        box = layout.box()
        box.label(text="Selected Point Editor")
        if obj:
            box.label(text=f"Object: {obj.name}")
            box.label(text=f"Mode: {obj.mode}")
        counts = _selected_mesh_element_counts(context)
        if counts:
            box.label(text=f"Selected V/E/F: {counts[0]} / {counts[1]} / {counts[2]}")
        if obj and obj.type == "MESH" and obj.mode == "EDIT":
            box.label(text="Select a vertex, edge, or face")
        else:
            box.label(text="Enter Mesh Edit Mode")
        return

    local, world, count = point_positions
    _sync_selected_point_props(context, local, world)

    box = layout.box()
    box.label(text=f"Selected Point Editor ({count})")
    counts = _selected_mesh_element_counts(context)
    if counts:
        box.label(text=f"Selected V/E/F: {counts[0]} / {counts[1]} / {counts[2]}")
    split = box.split(factor=0.5)

    left = split.column(align=True)
    left.label(text="Object Space")
    _draw_xyz_editor(left, context.scene, "fox_selected_point_local")

    right = split.column(align=True)
    right.label(text="World Space")
    _draw_xyz_editor(right, context.scene, "fox_selected_point_world")


def _draw_material_manager(layout, context):
    obj = context.object
    materials = list(bpy.data.materials)
    active_name = _active_fox_material_name(context)

    header = layout.box()
    row = header.row(align=True)
    row.label(text=f"Material Balls: {len(materials)}", icon="MATERIAL")
    row.operator("fox_c4d_navigation.material_add", text="", icon="ADD")
    row.operator("fox_c4d_navigation.material_duplicate", text="", icon="DUPLICATE")
    row.operator("fox_c4d_navigation.material_delete_active", text="", icon="TRASH")

    preset_row = header.row(align=True)
    for preset in ("CLAY", "PLASTIC", "METAL"):
        op = preset_row.operator("fox_c4d_navigation.material_create_preset", text=preset.title())
        op.preset = preset
    preset_row = header.row(align=True)
    for preset in ("RUBBER", "GLASS", "EMISSION"):
        op = preset_row.operator("fox_c4d_navigation.material_create_preset", text=preset.title())
        op.preset = preset

    library = layout.box()
    library.label(text="Materials")
    if not materials:
        library.label(text="No materials in scene")
    grid = library.grid_flow(row_major=True, columns=3, even_columns=True, even_rows=True, align=True)
    for material in materials:
        is_active = material.name == active_name
        column = grid.column(align=True)
        preview = column.operator(
            "fox_c4d_navigation.material_ball_select",
            text="",
            icon_value=layout.icon(material),
            depress=is_active,
        )
        preview.material_name = material.name
        preview_row = column.row(align=True)
        preview_row.scale_y = 0.85
        name = material.name[:12]
        select = preview_row.operator(
            "fox_c4d_navigation.material_ball_select",
            text=name,
            icon="RADIOBUT_ON" if is_active else "RADIOBUT_OFF",
            depress=is_active,
        )
        select.material_name = material.name
        color = column.row(align=True)
        color.scale_y = 0.75
        color.prop(material, "diffuse_color", text="")
        actions = column.row(align=True)
        drop = actions.operator("fox_c4d_navigation.material_drop_assign", text="", icon="EYEDROPPER")
        drop.material_name = material.name
        copy = actions.operator("fox_c4d_navigation.material_duplicate", text="", icon="DUPLICATE")
        copy.material_name = material.name
        users = actions.operator("fox_c4d_navigation.material_select_users", text=str(_material_user_object_count(material)))
        users.material_name = material.name

    object_box = layout.box()
    object_box.label(text="Object Slots")
    if not obj or not hasattr(obj.data, "materials"):
        object_box.label(text="Select a material-capable object")
        return

    row = object_box.row(align=True)
    row.operator("fox_c4d_navigation.material_add", text="Add Slot")
    row.operator("fox_c4d_navigation.material_remove", text="Remove Slot")

    if not obj.material_slots:
        object_box.label(text="No material slots")
        return

    for index, slot in enumerate(obj.material_slots):
        material = slot.material
        name = material.name if material else "Empty"
        row = object_box.row(align=True)
        icon = "RADIOBUT_ON" if index == obj.active_material_index else "RADIOBUT_OFF"
        op = row.operator("fox_c4d_navigation.material_select", text="", icon=icon)
        op.index = index
        op = row.operator("fox_c4d_navigation.material_select", text=name, emboss=False)
        op.index = index

    material = _active_fox_material(context) or obj.active_material
    if material:
        edit_box = layout.box()
        edit_box.label(text="Selected Material Ball")
        edit_box.prop(material, "name", text="")
        edit_box.prop(material, "diffuse_color", text="Color")
        edit_box.prop(material, "use_nodes", text="Nodes")


def _set_principled_input(material, input_name, value):
    if not material.use_nodes or not material.node_tree:
        return

    for node in material.node_tree.nodes:
        if node.type != "BSDF_PRINCIPLED":
            continue
        if input_name in node.inputs:
            node.inputs[input_name].default_value = value
        return


def _material_user_object_count(material):
    return sum(
        1
        for obj in bpy.data.objects
        if hasattr(obj.data, "materials") and any(slot.material == material for slot in obj.material_slots)
    )


def _ensure_material_on_object(obj, material):
    if not obj or not hasattr(obj.data, "materials"):
        return -1

    for index, slot in enumerate(obj.material_slots):
        if slot.material == material:
            obj.active_material_index = index
            return index

    obj.data.materials.append(material)
    obj.active_material_index = len(obj.material_slots) - 1
    return obj.active_material_index


def _view3d_override_from_mouse(context, event):
    window = context.window
    screen = context.screen
    if not window or not screen:
        return None

    for area in screen.areas:
        if area.type != "VIEW_3D":
            continue
        if not (area.x <= event.mouse_x <= area.x + area.width and area.y <= event.mouse_y <= area.y + area.height):
            continue
        for region in area.regions:
            if region.type != "WINDOW":
                continue
            if not (
                region.x <= event.mouse_x <= region.x + region.width
                and region.y <= event.mouse_y <= region.y + region.height
            ):
                continue
            space_data = area.spaces.active
            return {
                "window": window,
                "screen": screen,
                "area": area,
                "region": region,
                "space_data": space_data,
                "region_data": space_data.region_3d,
            }
    return None


def _object_under_mouse(context, event):
    override = _view3d_override_from_mouse(context, event)
    if not override:
        return None

    region = override["region"]
    region_data = override["region_data"]
    coord = (event.mouse_x - region.x, event.mouse_y - region.y)
    origin = view3d_utils.region_2d_to_origin_3d(region, region_data, coord)
    direction = view3d_utils.region_2d_to_vector_3d(region, region_data, coord)
    depsgraph = context.evaluated_depsgraph_get()
    hit, _location, _normal, _index, obj, _matrix = context.scene.ray_cast(depsgraph, origin, direction)
    return obj if hit and obj and hasattr(obj.data, "materials") else None


class FOX_C4D_NAVIGATION_OT_set_tool(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.set_tool"
    bl_label = "Set C4D Style Transform Tool"
    bl_options = {"REGISTER"}

    name: bpy.props.StringProperty()
    gizmo_mode: bpy.props.EnumProperty(
        items=(
            ("MOVE", "Move", ""),
            ("ROTATE", "Rotate", ""),
            ("SCALE", "Scale", ""),
        ),
        default="MOVE",
    )

    def execute(self, context):
        override = _find_view3d_override(context)
        if override:
            with context.temp_override(**override):
                bpy.ops.wm.tool_set_by_id(name=self.name)
                _configure_transform_gizmo(override["space_data"], self.gizmo_mode)
        else:
            bpy.ops.wm.tool_set_by_id(name=self.name)
            _configure_transform_gizmo(context.space_data, self.gizmo_mode)
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_frame_selected(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.frame_selected"
    bl_label = "Frame Selected"
    bl_options = {"REGISTER"}

    def execute(self, context):
        if _run_in_view3d(context, "view3d.view_selected"):
            return {"FINISHED"}
        return {"CANCELLED"}


class FOX_C4D_NAVIGATION_OT_frame_all(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.frame_all"
    bl_label = "Frame All"
    bl_options = {"REGISTER"}

    def execute(self, context):
        if _run_in_view3d(context, "view3d.view_all"):
            return {"FINISHED"}
        return {"CANCELLED"}
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_c4d_bottom_layout(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.c4d_bottom_layout"
    bl_label = "Create C4D Bottom Layout"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        area = _find_timeline_area(context)
        if not area:
            return {"CANCELLED"}

        before = set(context.screen.areas)
        with context.temp_override(area=area):
            bpy.ops.screen.area_split(direction="VERTICAL", factor=0.78)
        new_areas = [candidate for candidate in context.screen.areas if candidate not in before]
        right_area = new_areas[0] if new_areas else None

        for candidate in (area, right_area):
            if not candidate:
                continue
            candidate.type = "DOPESHEET_EDITOR"
            candidate.spaces.active.mode = "TIMELINE"
            if hasattr(candidate.spaces.active, "show_region_ui"):
                candidate.spaces.active.show_region_ui = True

        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_create_workspace(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.create_workspace"
    bl_label = "Create FoX C4D Workspace"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        existing = bpy.data.workspaces.get("FoX C4D")
        if existing:
            context.window.workspace = existing
        else:
            bpy.ops.workspace.duplicate()
            context.window.workspace.name = "FoX C4D"

        for area in context.screen.areas:
            if area.type == "DOPESHEET_EDITOR":
                area.spaces.active.mode = "TIMELINE"
                if hasattr(area.spaces.active, "show_region_ui"):
                    area.spaces.active.show_region_ui = True
            elif area.type == "VIEW_3D":
                area.spaces.active.show_region_toolbar = True
                area.spaces.active.show_region_tool_header = True
                area.spaces.active.show_region_ui = True
                area.spaces.active.show_gizmo = True
                area.spaces.active.show_gizmo_context = True
                area.spaces.active.show_gizmo_tool = True

        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_move_selected_point_to_world(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.move_selected_point_to_world"
    bl_label = "Move Selected Point To World Position"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        target = context.scene.fox_selected_point_world
        if _move_selected_mesh_points_to_world(context, target):
            return {"FINISHED"}
        return {"CANCELLED"}


class FOX_C4D_NAVIGATION_OT_zoom_viewport(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.zoom_viewport"
    bl_label = "C4D Style Viewport Zoom"
    bl_options = {"REGISTER"}

    delta: bpy.props.IntProperty(default=1)

    def execute(self, context):
        override = _find_view3d_override(context)
        if not override:
            return {"CANCELLED"}

        if _run_in_view3d(context, "view3d.zoom", delta=self.delta):
            return {"FINISHED"}
        return {"CANCELLED"}


class FOX_C4D_NAVIGATION_OT_zoom_drag(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.zoom_drag"
    bl_label = "C4D Style Zoom Drag"
    bl_options = {"REGISTER", "GRAB_CURSOR", "BLOCKING"}

    _init_x = 0
    _init_y = 0
    _init_distance = 0.0
    _window_ymax = 0
    _ui_scale = 1.0

    def invoke(self, context, event):
        override = _find_view3d_override(context)
        if not override:
            return {"CANCELLED"}

        self._init_x = event.mouse_x
        self._init_y = event.mouse_y
        self._init_distance = override["region_data"].view_distance
        self._window_ymax = override["region"].height
        self._ui_scale = max(0.001, context.preferences.system.ui_scale)
        context.window_manager.modal_handler_add(self)
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type in {"RIGHTMOUSE", "ESC"} and event.value == "RELEASE":
            return {"FINISHED"}

        if event.type == "MOUSEMOVE":
            dx = event.mouse_x - self._init_x
            dy = event.mouse_y - self._init_y
            motion = dy if abs(dy) >= abs(dx) else dx
            current_y = self._init_y + motion

            len_old = (5.0 * self._ui_scale) + ((self._window_ymax - self._init_y) / self._ui_scale)
            len_new = (5.0 * self._ui_scale) + ((self._window_ymax - current_y) / self._ui_scale)
            factor = 2.0 * (len_new / max(len_old, 1.0) - 1.0) + 1.0

            override = _find_view3d_override(context)
            if override:
                region_data = override["region_data"]
                if region_data.view_perspective == "CAMERA":
                    region_data.view_camera_zoom += motion * 0.01
                else:
                    region_data.view_distance = max(0.001, self._init_distance * factor)
            return {"RUNNING_MODAL"}

        return {"RUNNING_MODAL"}


class FOX_C4D_NAVIGATION_OT_material_add(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_add"
    bl_label = "New FoX Material Ball"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        obj = context.object
        material = bpy.data.materials.new(name="FoX Material Ball")
        material.diffuse_color = (0.8, 0.8, 0.8, 1.0)
        material.use_nodes = True
        _set_principled_input(material, "Base Color", material.diffuse_color)
        _set_principled_input(material, "Roughness", 0.45)
        if obj and hasattr(obj.data, "materials"):
            _ensure_material_on_object(obj, material)
        context.scene.fox_material_manager_active = material.name
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_remove(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_remove"
    bl_label = "Remove FoX Material Slot"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        obj = context.object
        if not obj or not hasattr(obj.data, "materials") or not obj.material_slots:
            return {"CANCELLED"}

        index = min(obj.active_material_index, len(obj.material_slots) - 1)
        obj.data.materials.pop(index=index)
        obj.active_material_index = max(0, min(index, len(obj.material_slots) - 1))
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_select(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_select"
    bl_label = "Select FoX Material Slot"
    bl_options = {"REGISTER"}

    index: bpy.props.IntProperty(default=0)

    def execute(self, context):
        obj = context.object
        if not obj or self.index >= len(obj.material_slots):
            return {"CANCELLED"}

        obj.active_material_index = self.index
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_ball_select(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_ball_select"
    bl_label = "Use FoX Material Ball"
    bl_options = {"REGISTER", "UNDO"}

    material_name: bpy.props.StringProperty()

    def execute(self, context):
        material = bpy.data.materials.get(self.material_name)
        if not material:
            return {"CANCELLED"}

        context.scene.fox_material_manager_active = material.name
        objects = [obj for obj in context.selected_objects if hasattr(obj.data, "materials")]
        if not objects and context.object and hasattr(context.object.data, "materials"):
            objects = [context.object]

        for obj in objects:
            index = _ensure_material_on_object(obj, material)
            if obj.mode == "EDIT" and obj == context.object and obj.type == "MESH":
                bm = bmesh.from_edit_mesh(obj.data)
                selected_faces = [face for face in bm.faces if face.select]
                if selected_faces:
                    for face in selected_faces:
                        face.material_index = index
                    bmesh.update_edit_mesh(obj.data)
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_create_preset(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_create_preset"
    bl_label = "Create FoX Material Preset"
    bl_options = {"REGISTER", "UNDO"}

    preset: bpy.props.EnumProperty(
        items=(
            ("CLAY", "Clay", ""),
            ("PLASTIC", "Plastic", ""),
            ("METAL", "Metal", ""),
            ("RUBBER", "Rubber", ""),
            ("GLASS", "Glass", ""),
            ("EMISSION", "Emission", ""),
        ),
        default="CLAY",
    )

    def execute(self, context):
        presets = {
            "CLAY": ("FoX Clay", (0.72, 0.68, 0.62, 1.0), 0.0, 0.82),
            "PLASTIC": ("FoX Red Plastic", (0.86, 0.08, 0.04, 1.0), 0.0, 0.36),
            "METAL": ("FoX Brushed Metal", (0.70, 0.72, 0.74, 1.0), 1.0, 0.24),
            "RUBBER": ("FoX Black Rubber", (0.01, 0.01, 0.012, 1.0), 0.0, 0.68),
            "GLASS": ("FoX Glass", (0.72, 0.90, 1.0, 0.35), 0.0, 0.02),
            "EMISSION": ("FoX Emission", (1.0, 0.72, 0.18, 1.0), 0.0, 0.2),
        }
        name, color, metallic, roughness = presets[self.preset]
        material = bpy.data.materials.new(name=name)
        material.diffuse_color = color
        material.use_nodes = True
        _set_principled_input(material, "Base Color", color)
        _set_principled_input(material, "Metallic", metallic)
        _set_principled_input(material, "Roughness", roughness)
        _set_principled_input(material, "Alpha", color[3])

        if self.preset == "GLASS":
            material.blend_method = "BLEND"
            _set_principled_input(material, "Transmission Weight", 0.65)
        elif self.preset == "EMISSION":
            _set_principled_input(material, "Emission Color", color)
            _set_principled_input(material, "Emission Strength", 1.8)

        obj = context.object
        if obj and hasattr(obj.data, "materials"):
            _ensure_material_on_object(obj, material)
        context.scene.fox_material_manager_active = material.name
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_duplicate(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_duplicate"
    bl_label = "Duplicate FoX Material"
    bl_options = {"REGISTER", "UNDO"}

    material_name: bpy.props.StringProperty()

    def execute(self, context):
        material = bpy.data.materials.get(self.material_name) or _active_fox_material(context)
        if not material and context.object:
            material = context.object.active_material
        if not material:
            return {"CANCELLED"}

        duplicate = material.copy()
        duplicate.name = f"{material.name} Copy"
        obj = context.object
        if obj and hasattr(obj.data, "materials"):
            _ensure_material_on_object(obj, duplicate)
        context.scene.fox_material_manager_active = duplicate.name
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_assign(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_assign"
    bl_label = "Assign FoX Material"
    bl_options = {"REGISTER", "UNDO"}

    material_name: bpy.props.StringProperty()

    def execute(self, context):
        material = bpy.data.materials.get(self.material_name)
        if not material:
            return {"CANCELLED"}

        objects = [obj for obj in context.selected_objects if hasattr(obj.data, "materials")]
        if not objects and context.object and hasattr(context.object.data, "materials"):
            objects = [context.object]

        for obj in objects:
            index = _ensure_material_on_object(obj, material)
            if obj.mode == "EDIT" and obj == context.object and obj.type == "MESH":
                bm = bmesh.from_edit_mesh(obj.data)
                for face in bm.faces:
                    if face.select:
                        face.material_index = index
                bmesh.update_edit_mesh(obj.data)

        return {"FINISHED"} if objects else {"CANCELLED"}


class FOX_C4D_NAVIGATION_OT_material_drop_assign(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_drop_assign"
    bl_label = "Drop FoX Material On Object"
    bl_options = {"REGISTER", "UNDO", "BLOCKING"}

    material_name: bpy.props.StringProperty()

    def invoke(self, context, event):
        material = bpy.data.materials.get(self.material_name)
        if not material:
            return {"CANCELLED"}

        context.scene.fox_material_manager_active = material.name
        context.window.cursor_set("EYEDROPPER")
        context.window_manager.modal_handler_add(self)
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type in {"ESC", "RIGHTMOUSE"}:
            context.window.cursor_set("DEFAULT")
            return {"CANCELLED"}

        if event.type == "LEFTMOUSE" and event.value == "PRESS":
            context.window.cursor_set("DEFAULT")
            material = bpy.data.materials.get(self.material_name)
            obj = _object_under_mouse(context, event)
            if not material or not obj:
                return {"CANCELLED"}

            _ensure_material_on_object(obj, material)
            obj.select_set(True)
            context.view_layer.objects.active = obj
            return {"FINISHED"}

        return {"RUNNING_MODAL"}


class FOX_C4D_NAVIGATION_OT_material_select_users(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_select_users"
    bl_label = "Select Objects Using FoX Material"
    bl_options = {"REGISTER", "UNDO"}

    material_name: bpy.props.StringProperty()

    def execute(self, context):
        material = bpy.data.materials.get(self.material_name)
        if not material:
            return {"CANCELLED"}

        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")

        bpy.ops.object.select_all(action="DESELECT")
        selected = []
        for obj in bpy.data.objects:
            if hasattr(obj.data, "materials") and any(slot.material == material for slot in obj.material_slots):
                obj.select_set(True)
                selected.append(obj)

        if selected:
            context.view_layer.objects.active = selected[0]
            return {"FINISHED"}
        return {"CANCELLED"}


class FOX_C4D_NAVIGATION_OT_material_remove_data(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_remove_data"
    bl_label = "Remove FoX Material"
    bl_options = {"REGISTER", "UNDO"}

    material_name: bpy.props.StringProperty()

    def execute(self, context):
        material_name = self.material_name or _active_fox_material_name(context)
        material = bpy.data.materials.get(material_name)
        if not material:
            return {"CANCELLED"}

        bpy.data.materials.remove(material, do_unlink=True)
        context.scene.fox_material_manager_active = ""
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_OT_material_delete_active(bpy.types.Operator):
    bl_idname = "fox_c4d_navigation.material_delete_active"
    bl_label = "Delete Active FoX Material Ball"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        material = _active_fox_material(context)
        if not material:
            return {"CANCELLED"}

        bpy.data.materials.remove(material, do_unlink=True)
        context.scene.fox_material_manager_active = ""
        return {"FINISHED"}


class FOX_C4D_NAVIGATION_PT_help(bpy.types.Panel):
    bl_label = "FoX C4D Navigation"
    bl_idname = "FOX_C4D_NAVIGATION_PT_help"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "FoX"
    bl_order = 100
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        layout.operator("fox_c4d_navigation.create_workspace", text="Create FoX C4D Workspace")
        layout.separator()
        col = layout.column(align=True)
        col.label(text="1 + LMB Drag: Pan")
        col.label(text="2 + LMB Drag: Zoom")
        col.label(text="3 + LMB Drag: Rotate")
        col.separator()
        col.label(text="Alt + LMB Drag: Rotate")
        col.label(text="Alt + MMB Drag: Pan")
        col.label(text="Alt + RMB Drag: Zoom")
        col.separator()
        col.label(text="W: Move Tool")
        col.label(text="E: Rotate Tool")
        col.label(text="R/T: Scale Tool")
        col.label(text="S: Frame Selected")
        col.label(text="H: Frame All")


class FOX_C4D_NAVIGATION_PT_coordinates(bpy.types.Panel):
    bl_label = "FoX Coordinates"
    bl_idname = "FOX_C4D_NAVIGATION_PT_coordinates"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "FoX"
    bl_order = -100

    def draw(self, context):
        _draw_coordinate_panel(self.layout, context)


class FOX_C4D_NAVIGATION_PT_materials(bpy.types.Panel):
    bl_label = "FoX Material Manager"
    bl_idname = "FOX_C4D_NAVIGATION_PT_materials"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "FoX"

    def draw(self, context):
        _draw_material_manager(self.layout, context)


class FOX_C4D_NAVIGATION_PT_timeline_coordinates(bpy.types.Panel):
    bl_label = "FoX Coordinates"
    bl_idname = "FOX_C4D_NAVIGATION_PT_timeline_coordinates"
    bl_space_type = "DOPESHEET_EDITOR"
    bl_region_type = "UI"
    bl_category = "FoX"

    def draw(self, context):
        _draw_coordinate_panel(self.layout, context)


class FOX_C4D_NAVIGATION_PT_properties_coordinates(bpy.types.Panel):
    bl_label = "FoX Coordinates"
    bl_idname = "FOX_C4D_NAVIGATION_PT_properties_coordinates"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        _draw_coordinate_panel(self.layout, context)


class FOX_C4D_NAVIGATION_PT_properties_point_editor(bpy.types.Panel):
    bl_label = "FoX Point Editor"
    bl_idname = "FOX_C4D_NAVIGATION_PT_properties_point_editor"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        _draw_point_editor_panel(self.layout, context)


class FOX_C4D_NAVIGATION_PT_timeline_layout(bpy.types.Panel):
    bl_label = "FoX C4D Layout"
    bl_idname = "FOX_C4D_NAVIGATION_PT_timeline_layout"
    bl_space_type = "DOPESHEET_EDITOR"
    bl_region_type = "UI"
    bl_category = "FoX"
    bl_order = -10

    def draw(self, context):
        layout = self.layout
        layout.operator("fox_c4d_navigation.create_workspace", text="Create FoX C4D Workspace")
        layout.operator("fox_c4d_navigation.c4d_bottom_layout", text="C4D Bottom Layout")
        layout.label(text="Left: Material Manager")
        layout.label(text="Right: Coordinates / Point Editor")


class FOX_C4D_NAVIGATION_PT_timeline_point_editor(bpy.types.Panel):
    bl_label = "FoX Point Editor"
    bl_idname = "FOX_C4D_NAVIGATION_PT_timeline_point_editor"
    bl_space_type = "DOPESHEET_EDITOR"
    bl_region_type = "UI"
    bl_category = "FoX"

    def draw(self, context):
        _draw_point_editor_panel(self.layout, context)


class FOX_C4D_NAVIGATION_PT_timeline_materials(bpy.types.Panel):
    bl_label = "FoX Material Manager"
    bl_idname = "FOX_C4D_NAVIGATION_PT_timeline_materials"
    bl_space_type = "DOPESHEET_EDITOR"
    bl_region_type = "UI"
    bl_category = "FoX"

    def draw(self, context):
        _draw_material_manager(self.layout, context)


def register():
    bpy.types.Scene.fox_material_manager_active = bpy.props.StringProperty(
        name="Active FoX Material Ball",
        default="",
    )
    bpy.types.Scene.fox_coordinate_update_lock = bpy.props.BoolProperty(default=False)
    bpy.types.Scene.fox_selected_point_signature = bpy.props.StringProperty(default="")
    bpy.types.Scene.fox_selected_point_world = bpy.props.FloatVectorProperty(
        name="Selected Point World Position",
        subtype="XYZ",
        default=(0.0, 0.0, 0.0),
        precision=4,
        update=_selected_point_world_update,
    )
    bpy.types.Scene.fox_selected_point_world_live = bpy.props.FloatVectorProperty(
        name="Selected Point World Position",
        subtype="XYZ",
        precision=4,
        get=_selected_point_world_get,
        set=_selected_point_world_set,
    )
    bpy.types.Scene.fox_selected_point_local = bpy.props.FloatVectorProperty(
        name="Selected Point Object Position",
        subtype="XYZ",
        default=(0.0, 0.0, 0.0),
        precision=4,
        update=_selected_point_local_update,
    )
    bpy.types.Scene.fox_selected_element_rotation = bpy.props.FloatVectorProperty(
        name="Selected Element Rotation",
        subtype="EULER",
        default=(0.0, 0.0, 0.0),
        precision=4,
        update=_selected_element_rotation_update,
    )
    bpy.types.Scene.fox_selected_element_rotation_previous = bpy.props.FloatVectorProperty(
        name="Previous Selected Element Rotation",
        subtype="EULER",
        default=(0.0, 0.0, 0.0),
        precision=4,
    )
    bpy.types.Scene.fox_selected_element_scale = bpy.props.FloatVectorProperty(
        name="Selected Element Scale",
        subtype="XYZ",
        default=(1.0, 1.0, 1.0),
        precision=4,
        update=_selected_element_scale_update,
    )
    bpy.types.Scene.fox_selected_element_size = bpy.props.FloatVectorProperty(
        name="Selected Element Size",
        subtype="XYZ",
        default=(0.0, 0.0, 0.0),
        precision=4,
        update=_selected_element_size_update,
    )

    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_set_tool)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_frame_selected)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_frame_all)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_c4d_bottom_layout)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_create_workspace)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_move_selected_point_to_world)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_zoom_viewport)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_zoom_drag)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_add)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_remove)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_select)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_ball_select)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_create_preset)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_duplicate)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_assign)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_drop_assign)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_select_users)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_remove_data)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_OT_material_delete_active)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_help)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_coordinates)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_materials)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_properties_coordinates)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_properties_point_editor)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_timeline_layout)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_timeline_coordinates)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_timeline_point_editor)
    bpy.utils.register_class(FOX_C4D_NAVIGATION_PT_timeline_materials)
    bpy.app.timers.register(_disable_conflicting_default_shortcuts, first_interval=0.5)
    bpy.app.timers.register(_show_view3d_toolbars, first_interval=0.7)
    bpy.app.timers.register(_show_timeline_fox_panel, first_interval=0.9)

    window_manager = bpy.context.window_manager
    keyconfig = window_manager.keyconfigs.addon
    if not keyconfig:
        return

    keymap = keyconfig.keymaps.new(name="3D View", space_type="VIEW_3D")
    window_keymap = keyconfig.keymaps.new(name="Window", space_type="EMPTY")
    frames_keymap = keyconfig.keymaps.new(name="Frames", space_type="EMPTY")
    dopesheet_keymap = keyconfig.keymaps.new(name="Dopesheet", space_type="DOPESHEET_EDITOR")
    mode_keymaps = [
        keymap,
        keyconfig.keymaps.new(name="Object Mode", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Mesh", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Curve", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Armature", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Metaball", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Lattice", space_type="EMPTY"),
        keyconfig.keymaps.new(name="Pose", space_type="EMPTY"),
    ]

    # Cinema 4D's direct viewport navigation muscle memory.
    _add_view3d_keymap_item(keymap, "view3d.move", "LEFTMOUSE", key_modifier="ONE")
    _add_view3d_keymap_item(keymap, "view3d.zoom", "LEFTMOUSE", key_modifier="TWO")
    _add_view3d_keymap_item(keymap, "view3d.rotate", "LEFTMOUSE", key_modifier="THREE")

    # Familiar DCC navigation, useful when coming from C4D/Maya style setups.
    _add_view3d_keymap_item(keymap, "view3d.rotate", "LEFTMOUSE", alt=True)
    _add_view3d_keymap_item(keymap, "view3d.move", "MIDDLEMOUSE", alt=True)
    _add_view3d_keymap_item(keymap, "fox_c4d_navigation.zoom_drag", "RIGHTMOUSE", alt=True)
    for km in (keymap, window_keymap, frames_keymap):
        _add_zoom_keymap_item(km, "WHEELINMOUSE", 1)
        _add_zoom_keymap_item(km, "WHEELOUTMOUSE", -1)
        _add_zoom_keymap_item(km, "WHEELUPMOUSE", 1)
        _add_zoom_keymap_item(km, "WHEELDOWNMOUSE", -1)

    # Cinema 4D style transform tool selection.
    for km in mode_keymaps:
        _add_tool_keymap_item(km, "W", "builtin.move")
        _add_tool_keymap_item(km, "E", "builtin.rotate")
        _add_tool_keymap_item(km, "R", "builtin.scale")
        _add_tool_keymap_item(km, "T", "builtin.scale")
        _add_view3d_keymap_item(km, "fox_c4d_navigation.frame_selected", "S")
        _add_view3d_keymap_item(km, "fox_c4d_navigation.frame_all", "H")

    _add_view3d_keymap_item(dopesheet_keymap, "fox_c4d_navigation.material_delete_active", "DEL")


def unregister():
    for attr in (
        "fox_selected_element_size",
        "fox_selected_element_scale",
        "fox_selected_element_rotation_previous",
        "fox_selected_element_rotation",
    ):
        if hasattr(bpy.types.Scene, attr):
            delattr(bpy.types.Scene, attr)
    if hasattr(bpy.types.Scene, "fox_selected_point_signature"):
        del bpy.types.Scene.fox_selected_point_signature
    if hasattr(bpy.types.Scene, "fox_selected_point_local"):
        del bpy.types.Scene.fox_selected_point_local
    if hasattr(bpy.types.Scene, "fox_selected_point_world"):
        del bpy.types.Scene.fox_selected_point_world
    if hasattr(bpy.types.Scene, "fox_selected_point_world_live"):
        del bpy.types.Scene.fox_selected_point_world_live
    if hasattr(bpy.types.Scene, "fox_coordinate_update_lock"):
        del bpy.types.Scene.fox_coordinate_update_lock
    if hasattr(bpy.types.Scene, "fox_material_manager_active"):
        del bpy.types.Scene.fox_material_manager_active

    for item in disabled_keymap_items:
        try:
            item.active = True
        except ReferenceError:
            pass
    disabled_keymap_items.clear()

    for keymap, item in reversed(addon_keymaps):
        keymap.keymap_items.remove(item)
    addon_keymaps.clear()

    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_help)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_timeline_materials)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_timeline_point_editor)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_timeline_coordinates)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_timeline_layout)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_properties_point_editor)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_properties_coordinates)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_materials)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_PT_coordinates)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_delete_active)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_remove_data)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_select_users)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_drop_assign)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_assign)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_duplicate)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_create_preset)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_ball_select)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_select)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_remove)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_material_add)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_zoom_drag)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_zoom_viewport)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_move_selected_point_to_world)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_create_workspace)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_c4d_bottom_layout)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_frame_all)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_frame_selected)
    bpy.utils.unregister_class(FOX_C4D_NAVIGATION_OT_set_tool)
