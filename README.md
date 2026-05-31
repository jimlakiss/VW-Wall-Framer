# iQs Wall Framer

Standalone Vectorworks 2026 SDK plug-in for generating QS-focused stud-wall
framing objects.

The first milestone is the menu command:

`iQs Probe Selected Walls For Framing`

It attaches an `iQs_Host` record with a stable UUID to selected Vectorworks
walls and writes development probe JSON files to `probe-output/`.

The current generator milestone also creates one prototype `iQs_StudWallFrame`
group per selected straight wall. Each group contains:

- one bottom plate
- two top plates
- regular `45 mm` studs at `450 mm` centres, including end studs
- opening-aware jamb and trimmer studs for inserted doors and windows
- tagged extrude headers over inserted doors and windows
- tagged extrude sill members under inserted windows
- cripple studs above headers and below raised sills/openings
- opening-aware nogging rows fitted between vertical members at 1350 mm centres, staggered by 45 mm
- linear start-to-end top and bottom wall rakes, with sloped plates and locally sized studs
- gross long-point stock lengths for raked members before bevel cuts, reflected in both geometry and export records
- component-centreline plate joins for selected endpoint-connected wall chains
- lining-fixing corner studs on through walls, inset by the joined framing-component depth
- overlap resolution for vertical framing: end studs, opening studs, trimmers, kings, corner studs, then regular studs
- incrementing member names: `BP`, `TP`, `S`, `NOG`, `WS`, `LIN` and `DH`

Generated frame groups and child members are assigned to the
`iQs-Wall Framing` class. A later settings modal will allow the user to choose a
different framing class and wall component.

The prototype requires a wall component named exactly `Timber Frame`. Its depth
and centreline define the generated framing position. If that component is not
present, the wall is reported and skipped without replacing any prior generated
frame. The settings modal will later allow the component name to be selected.
The command writes a matching `stud_wall_frame_generation_*.json` member
schedule to `probe-output/`.

Connected-wall corner extents form the baseline for plates, studs and noggings.
Copied walls with duplicated `iQs_Host` UUIDs are assigned fresh host IDs before
generation so each selected wall receives its own linked framing group.

Each generated timber extrude also receives the `iQs Extruder V0.1` semantic
record used by the iQs exporter. The framer stores the physical extrusion
distance, profile dimension A and profile dimension B separately, then assigns
each measured value to `Length`, `Width` or `Height`. Suggested mappings are
applied automatically per member type and remain independent of wall direction.

Re-running the command removes all frame groups already linked to each selected
host wall, reports stale duplicates, and creates one canonical replacement.

## Next-phase wishlist

- replace the menu-only workflow with an editable wall-frame object that
  recomputes when settings change
- expose opening IDs from nominated Vectorworks door and window IDs
- support per-opening lintel profile overrides
- support multiple lintels over an opening
- support explicit lintel offsets for the less common cases where a lintel sits
  outside the wall framing envelope
- calculate nogging rows from the available stud height rather than the overall
  wall height, and remove occasional spare noggings near wall ends below the
  top plate

V0.1 accepts straight walls with linear start-to-end top and bottom heights.
Walls with intermediate peak breaks, including complex fitted profiles, are
reported as unsupported and left unchanged. For selected endpoint-connected
walls, plate joins use the selected framing-component centrelines: the earlier
wall segment runs through the joint and the following segment butts against it.
A later settings modal will expose endpoint overrides.

## Shared SDK

`CppPrototype/SDKLib` is a symlink to the shared Vectorworks 2026 SDK under:

`/Users/jameslakiss/Documents/Develop/iQs/11 VW SDK/SDK/SDKVW(832364)/SDKLib`

## Development build

```sh
cd CppPrototype
xcodebuild \
  -project iQsWallFramer.xcodeproj \
  -scheme iQsWallFramer \
  -configuration 'Debug 64' \
  -derivedDataPath DerivedData \
  build
```

The Xcode build installs a development symlink into the Vectorworks 2026
user plug-ins folder. Restart Vectorworks after each C++ build.

The SDK command and its VectorScript helper must both be added to the active
workspace. Add the native `iQs Probe Selected Walls For Framing` SDK command
first. Vectorworks may initially show SDK menu commands as `[unknown]` in the
Workspace Editor.

Then create a VectorScript menu plug-in in the Vectorworks Plug-in Manager
named:

`iQs Probe Selected Walls For Framing 1`

Use the source from:

`Helper Plugins/iQs_Probe_Selected_Walls_For_Framing.vs`

Add the named VectorScript menu plug-in to the workspace as the visible menu
item. It delegates to the installed SDK command through `DoMenuTextByName`.
