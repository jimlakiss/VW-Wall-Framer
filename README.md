# iQs Wall Framer

Standalone Vectorworks 2026 SDK plug-in for generating QS-focused stud-wall
framing objects.

The first milestone is the menu command:

`iQs Probe Selected Walls For Framing`

It attaches an `iQs_Host` record with a stable UUID to selected Vectorworks
walls and writes development probe JSON files to `probe-output/`.

The current generator milestone also creates one prototype `iQs_StudWallFrame`
group per selected straight wall. Each group contains:

- configurable bottom-plate layers, one by default
- two top plates
- bottom-plate layers split around door thresholds and low window sills that intersect them
- regular `45 mm` studs at `450 mm` centres, including end studs
- opening-aware jamb and trimmer studs for inserted doors and windows
- tagged extrude lintels over inserted doors and windows
- optional per-opening stud-profile ledgers above and beneath lintels over a
  configurable height, defaulting to `120 mm`
- scrollable per-opening lintel ID, count and profile overrides, keyed
  internally and labelled with the visible Vectorworks window or door ID
- verbatim user-nominated lintel IDs, with run-wide automatic numbering when no
  lintel ID is nominated
- document-native `iQs_Opening` records on inserted doors and windows, preserving
  lintel IDs, counts and dimensions across regeneration and Vectorworks restarts
- descriptive member classes with a default colour palette inherited by the
  generated framing solids
- tagged extrude sill members under inserted windows
- jack studs above lintels and below raised sills/openings, with lintel-height-aware
  upper termination and a persisted per-opening override to continue them to
  the lintel underside
- opening-aware nogging rows fitted between vertical members from the clear stud
  zone at 1350 mm centres, staggered by 45 mm
- short noggings retained inside intentional corner-stud clusters to represent
  solid blocking
- linear start-to-end top and bottom wall rakes, with sloped plates and locally sized studs
- gross long-point stock lengths for raked members before bevel cuts, reflected in both geometry and export records
- component-centreline plate joins for selected endpoint-connected wall chains
- lining-fixing corner studs on through walls, inset by the joined framing-component depth
- overlap resolution for vertical framing: end studs, opening studs, trimmers,
  jamb studs, corner studs, then regular studs
- configurable incrementing member names: `BP`, `TP`, `S`, `ES`, `CS`, `JAM`,
  `TR`, `JS`, `NOG`, `WS`, `LIN`, `DH` and `LED`

Generated frame groups are assigned to the `Wall-Timber Frame-Frame group`
class. Child members are assigned to descriptive sibling classes, such as
`Wall-Timber Frame-Bottom plate` and `Wall-Timber Frame-Top plate`, so each member
type can carry a distinct installation cost. The settings modal allows the user
to choose a different framing class and wall component.

The prototype requires a wall component named exactly `Timber Frame`. Its depth
and centreline define the generated framing position. If that component is not
present, the wall is reported and skipped without replacing any prior generated
frame. The settings modal allows the component name to be selected.
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
- support explicit lintel offsets for the less common cases where a lintel sits
  outside the wall framing envelope
- allow materials to be allocated to framing members
- add Advanced-tab links for editing generated classes and allocating materials
  by framing member type

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
