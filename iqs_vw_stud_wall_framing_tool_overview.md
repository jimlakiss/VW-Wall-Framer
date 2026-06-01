# iQs Vectorworks Stud Wall Framing Tool — High Level Build Brief v0.1

**Target:** Vectorworks 2026+
**Primary implementation:** C++ SDK plug-in / PIOs
**Secondary fallback:** Python/VectorScript exporter only for proof-of-concept probing, not final tool UX
**Purpose:** Convert selected Vectorworks Wall PIOs, or wall components/materials/classes, into regeneratable stud wall framing objects with clear graphical representation and reliable quantity/cost extraction.

---

## 1. The idea in one sentence

Build an iQs wall-framing generator that reads a selected Vectorworks Wall PIO, derives the wall run, height, openings and selected stud-framing component, then generates plates, studs, noggings/blocking, jamb/trimmer studs, lintel/header framing and optional extra members as independent iQs framing objects or sub-objects, with stored quantities and cost-plan data.

This is not trying to make the native Vectorworks framing command better. It is a separate QS/detailing-first framing system that uses the Wall PIO as the host/reference geometry.

---

## 2. Why this is worth building

The market gap is real:

- Carpenters need fast frame layouts and material take-offs.
- QS workflows need extractable counts, lengths, grades, sizes, waste and cost build-ups.
- Native modelling tools tend to produce either rough graphics or awkward BIM objects, not clean estimating data.
- Existing examples in other ecosystems, such as PlusSpec, Medeek Wall and Profile Builder, prove the demand for geometry-generation tools that turn simple host geometry into buildable framing assemblies.

For iQs, this becomes a bridge between estimating and detailing: the user draws or imports walls, presses a button, and receives visible framing plus a schedule that can be costed.

---

## 3. Core workflow

### 3.1 Generate from selected Wall PIOs

1. User selects one or more Vectorworks Wall PIO objects.
2. User runs `iQs > Generate Stud Framing From Walls`.
3. Dialog appears with framing presets:
   - stud size
   - plate size
   - stud spacing
   - nogging/blocking rows or spacing
   - single/double top plate
   - single/double bottom plate
   - start/end stud rules
   - opening framing rules
   - selected wall component/material/class to treat as the stud wall layer
4. Tool creates or updates an `iQs_StudWallFrame` PIO linked to the source wall.
5. Framing appears graphically as actual members.
6. Quantities are stored on the generated PIO and exportable to JSON/CSV/worksheet.

### 3.2 Refresh after design changes

1. User changes wall length, height, style, doors/windows, or location.
2. User selects the wall or existing frame object.
3. User runs `iQs > Refresh Stud Framing`.
4. Tool recalculates member layout and rebuilds geometry.
5. Stable IDs are preserved where practical; removed/changed members are versioned or replaced.

Live preview is not required for v1. A reliable button-driven rebuild is enough.

---

## 4. Object model options

There are three realistic implementation models.

### Option A — One host PIO with generated member geometry inside

Create one `iQs_StudWallFrame` PIO per source wall. The PIO owns all member geometry internally.

**Pros**
- Clean drawing tree.
- Easy reset/rebuild.
- One object per wall to schedule.
- Best v1 option.

**Cons**
- Individual studs are not separately selectable unless we add member-level metadata or nested objects.
- Editing individual members manually is discouraged.

### Option B — One parent PIO plus separate child PIOs/members

Create parent `iQs_StudWallFrame`, then create individual member objects such as `iQs_FrameMember` for each stud/plate/nogging.

**Pros**
- Individual members can carry records and be selected/scheduled.
- Better for future fabrication drawings.

**Cons**
- More object-management risk.
- Harder to delete/rebuild cleanly without orphaned objects.
- More moving parts.

### Option C — Generate native Structural Member PIO objects

Use Vectorworks Structural Member PIOs for studs/plates/noggings.

**Pros**
- Native-looking objects.
- Potentially better visual integration.

**Cons**
- May be harder to control consistently.
- Quantities and metadata may depend on native PIO internals.
- Performance may suffer with many individual PIOs.

### Recommendation

Use **Option A for v1**: one iQs PIO per wall, with generated member solids/objects inside and a structured internal/export schema for each member.

Keep **Option C as a toggle or v2 experiment** only if the Structural Member PIO proves reliably scriptable and extractable.

---

## 5. Geometry strategy

### 5.1 Host wall extraction

For each selected Wall PIO, extract:

- wall handle
- wall UUID / iQs host ID
- wall type/class/layer
- wall centreline or component reference line
- start/end points
- wall length
- wall height
- wall thickness
- component list if available
- selected framing component/material/class
- openings: doors, windows, symbols/PIOs inserted in wall
- opening station along wall
- opening width/height/sill/head levels

### 5.2 Local coordinate frame

Each frame should be generated in a wall-local coordinate system:

- X axis = along wall length
- Y axis = wall thickness direction
- Z axis = vertical height

This makes member layout simple. After solving layout in local coordinates, transform member solids back to Vectorworks world coordinates.

### 5.3 Member geometry

Each timber member can be represented as:

- a simple rectangular prism/extrude, or
- an iQs proprietary extruder object, or
- later, a Structural Member PIO.

For v1, use rectangular generated solids inside the parent PIO:

- studs: vertical prisms
- bottom plates: horizontal prisms along wall length at base
- top plates: horizontal prisms along top
- noggings/blocking: horizontal short prisms between studs
- jamb studs: vertical prisms each side of openings
- trimmer/jack studs: vertical prisms supporting lintels/headers
- lintel/header: horizontal prism over opening
- sill/tray framing: optional horizontal member under windows

---

## 6. Framing rules / presets

Presets should be stored externally, not hard-coded.

Suggested path:

```text
~/Library/Application Support/Vectorworks/2026/iQs/Data/Framing/stud_wall_presets.csv
~/Library/Application Support/Vectorworks/2026/iQs/Data/Framing/member_sizes.csv
~/Library/Application Support/Vectorworks/2026/iQs/Data/Framing/standards.json
```

Mirror backup:

```text
~/Documents/iQs/VW Tools Backup/2026/iQs/Data/Framing/
```

This mirrors the reinforcement library strategy already adopted for iQs tools.

### 6.1 Preset fields

`stud_wall_presets.csv`

```csv
preset_code,preset_name,standard_ref,stud_width_mm,stud_depth_mm,stud_spacing_mm,plate_width_mm,plate_depth_mm,bottom_plate_count,top_plate_count,nogging_rows,nogging_spacing_mm,first_stud_offset_mm,end_stud_rule,opening_rule_code,default_material,default_grade,waste_percent
AU_TIMBER_90_MGP10_450,AU Timber 90 MGP10 @ 450,AS1684,45,90,450,45,90,1,2,1,,0,double_end,STD_OPENING_TIMBER,MGP10 Pine,90x45 MGP10,10
AU_TIMBER_90_MGP10_600,AU Timber 90 MGP10 @ 600,AS1684,45,90,600,45,90,1,2,1,,0,double_end,STD_OPENING_TIMBER,MGP10 Pine,90x45 MGP10,10
AU_STEEL_64_600,AU Steel Stud 64 @ 600,NASH,35,64,600,35,64,1,1,0,,0,single_end,STD_OPENING_STEEL,Steel Stud,64mm steel stud,7.5
```

### 6.2 Opening rule fields

`opening_rules.csv`

```csv
opening_rule_code,jamb_studs_each_side,jack_studs_each_side,header_member_count,sill_member_count,jack_above_spacing_mm,jack_below_spacing_mm,extra_stud_at_wall_ends
STD_OPENING_TIMBER,1,1,1,1,450,450,true
HEAVY_OPENING_TIMBER,2,2,2,1,450,450,true
STD_OPENING_STEEL,1,1,1,1,600,600,true
```

---

## 7. Dialog / popup requirements

The generator dialog should have four tabs.

### 7.1 Source tab

- Source mode:
  - selected Wall PIOs
  - selected wall component by material
  - selected wall component by class
  - selected wall style component index
- Host side/reference:
  - centreline
  - left face
  - right face
  - selected component centreline
- Update mode:
  - create new frames only
  - refresh existing linked frames
  - delete and rebuild

### 7.2 Preset tab

- framing preset popup
- stud size
- plate size
- stud spacing
- top plate count
- bottom plate count
- nogging rows / spacing
- start/end stud rule
- material / grade
- waste percentage

### 7.3 Openings tab

- detect doors
- detect windows
- detect all wall inserts
- jamb studs each side
- jack/trimmer studs each side
- header/lintel member count
- sill member under windows
- jack stud spacing above/below openings
- ignore openings below minimum width

### 7.4 Output tab

- output class prefix
- generate 2D elevation graphics
- generate 3D members
- generate labels/member IDs
- store member schedule on PIO
- create worksheet/report
- export JSON immediately

---

## 8. Data model / records

### 8.1 Host wall record

Attach this to any source wall used by the tool.

Record: `iQs_Host`

| Field | Type | Purpose |
|---|---:|---|
| `iqs_uuid` | Text | Stable host ID |
| `source_type` | Text | `VW_WALL_PIO` |
| `last_framed_unix` | Integer | Last generation timestamp |

### 8.2 Frame PIO fields

PIO: `iQs_StudWallFrame`

| Field | Type | Purpose |
|---|---:|---|
| `P_iqs_uuid` | Text | Stable frame ID |
| `P_host_wall_uuid` | Text | Linked wall UUID |
| `P_host_wall_handle_debug` | Text | Debug only |
| `P_preset_code` | Popup/Text | Framing preset |
| `P_source_component_mode` | Popup | Material/Class/Index/Whole Wall |
| `P_source_component_ref` | Text | Source material/class/component |
| `P_wall_length_mm` | Real | Computed |
| `P_wall_height_mm` | Real | Computed |
| `P_stud_spacing_mm` | Real | Active spacing |
| `P_member_count_total` | Integer | Computed |
| `P_length_total_lm` | Real | Computed |
| `P_volume_total_m3` | Real | Computed |
| `P_mass_total_kg` | Real | Optional if density known |
| `P_cost_total` | Real | Optional if rate linked |
| `P_last_generated_unix` | Integer | Version/debug |

### 8.3 Internal member schedule

Each generated member should exist in an internal schedule array, exported as JSON.

```json
{
  "member_id": "STUD-001",
  "member_type": "STUD",
  "size_code": "90x45 MGP10",
  "width_mm": 45,
  "depth_mm": 90,
  "length_mm": 2700,
  "qty": 1,
  "station_start_mm": 450,
  "station_end_mm": 450,
  "z_start_mm": 0,
  "z_end_mm": 2700,
  "opening_ref": null,
  "material": "MGP10 Pine",
  "grade": "MGP10",
  "class": "iQs-Framing-Studs"
}
```

---

## 9. Quantity outputs

Mandatory v1 quantities:

- stud count
- plate lineal metres
- nogging/blocking count and lineal metres
- header/lintel count and lineal metres
- jamb/trimmer stud count and lineal metres
- total timber/steel lineal metres by size/grade
- total member count
- total volume by size/grade
- waste-adjusted order length
- cost by rate code if mapped

Suggested rollup output:

| Group | Measurement |
|---|---:|
| 90x45 MGP10 studs | count + lm |
| 90x45 MGP10 plates | lm |
| 90x45 MGP10 noggings | count + lm |
| 140x45 lintels/headers | count + lm |
| Labour install wall framing | m2 or lm |
| Fixings/consumables | % or m2 allowance |

---

## 10. Export contract

Minimum JSON per frame object:

```json
{
  "type": "iQs_StudWallFrame",
  "iqs_uuid": "...",
  "host_wall_uuid": "...",
  "host_type": "VW_WALL_PIO",
  "preset_code": "AU_TIMBER_90_MGP10_450",
  "source_component_mode": "MATERIAL",
  "source_component_ref": "Timber Stud Framing",
  "wall_length_mm": 5400,
  "wall_height_mm": 2700,
  "stud_spacing_mm": 450,
  "top_plate_count": 2,
  "bottom_plate_count": 1,
  "openings": [
    {
      "opening_id": "OPN-001",
      "kind": "DOOR",
      "station_start_mm": 1200,
      "station_end_mm": 2020,
      "sill_mm": 0,
      "head_mm": 2100
    }
  ],
  "members": [
    {
      "member_id": "STUD-001",
      "member_type": "STUD",
      "size_code": "90x45 MGP10",
      "length_mm": 2700,
      "qty": 1,
      "station_mm": 450
    }
  ],
  "quantity_rollup": [
    {
      "size_code": "90x45 MGP10",
      "member_type": "STUD",
      "count": 12,
      "length_total_m": 32.4,
      "volume_m3": 0.13122,
      "waste_percent": 10,
      "order_length_m": 35.64
    }
  ]
}
```

---

## 11. Regeneration / refresh rules

The tool must never rely on manual deletion by the user.

### 11.1 Create

- If no existing frame is linked to the wall, create new `iQs_StudWallFrame`.
- Attach/generate `iQs_Host.iqs_uuid` on the wall.
- Store the host UUID on the frame.

### 11.2 Refresh

- Find frame objects with matching `host_wall_uuid`.
- Re-read wall geometry and openings.
- Re-solve layout.
- Delete/rebuild generated internal geometry.
- Preserve frame UUID and user preset choices.

### 11.3 Delete/rebuild

- Optional command for hard reset.
- Deletes linked frame objects and creates fresh ones.

---

## 12. Solver logic — v1 framing algorithm

### 12.1 Inputs

- wall length L
- wall height H
- stud spacing S
- stud width/depth
- plate count top/bottom
- nogging rules
- openings as station intervals

### 12.2 Base studs

1. Place first stud at station 0.
2. Place regular studs at spacing S.
3. Place final stud at station L.
4. Apply end-stud rule:
   - single end
   - double end
   - triple end if selected

### 12.3 Openings

For each opening:

1. Remove regular studs that clash with clear opening span.
2. Add jamb studs each side.
3. Add jack/trimmer studs each side.
4. Add header/lintel over opening.
5. Add sill member under window if applicable.
6. Add jack studs above header and/or below sill at spacing rule.

### 12.4 Plates

- Bottom plate: one or more full-length members.
- Top plate: one or more full-length members.
- Future version may split plates around openings or stock lengths.

### 12.5 Noggings/blocking

- Add rows at fixed heights or max spacing.
- Split noggings between adjacent studs.
- Skip through openings.
- Optional future behaviour: align row to sheet bracing/plasterboard rules.

---

## 13. SDK / Vectorworks implementation notes

### 13.1 Why C++ SDK

The final tool should be C++ SDK because:

- it needs reliable parametric reset behaviour;
- it needs a proper dialog/popup workflow;
- it needs access to wall/insert geometry and potentially component data;
- it may generate many members and therefore needs better performance than a Python-only solution.

The existing SDK exporter notes already confirm a working C++ plug-in pattern for VW 2026, including selection iteration, object type, object name, class, bounds and parent/layer traversal.

### 13.2 Existing iQs SDK patterns to reuse

Reuse the patterns already proven in the exporter:

- menu command plug-in using `IMenuEventSink`
- selection traversal using `gSDK->ForEachObjectN(allSelected, ...)`
- object type via `gSDK->GetObjectTypeN(h)`
- object name via `gSDK->GetObjectName(h, TXString&)`
- class via `gSDK->GetObjectClass(h)` and `gSDK->ClassIDToName(...)`
- bounds via `gSDK->GetObjectBounds(h, WorldRect&)`
- parent walk via `GS_ParentObject(...)` when needed

### 13.3 Wall extraction route

Start with safe extraction:

- confirm wall object type number in selected file
- get wall bounds
- get wall length using available length APIs or wall-specific object variables
- get height using wall-specific APIs/object variables/PIO record fields
- get wall inserts by walking child/contained objects or wall insert APIs
- get component data by wall style/component APIs where available

The current Python/Vectorscript API catalogue confirms relevant wall-related calls exist for adding/deleting wall symbols, wall peaks, wall components, component area/volume, wall-associated component bounds and wall insert location data. Use the C++ equivalents where available, and script-level calls only as probes.

### 13.4 Openings

Opening detection is the likely hard bit.

V1 practical approach:

- detect wall inserts attached to host wall;
- read their location along the wall;
- read width/height from the inserted symbol/PIO bounds or parameters;
- convert into station ranges;
- classify as door/window by object type, PIO name, class, record, or height/sill heuristic.

Do not try to solve every custom plug-in opening in v1.

### 13.5 Generated geometry

For v1, generate member solids inside the `iQs_StudWallFrame` PIO.

Implementation options:

- create rectangular prism/extrude from local coordinates;
- assign class/material per member type using descriptive child classes such as
  `Wall-Timber Frame-Bottom plate` and `Wall-Timber Frame-Top plate`, rather than
  abbreviations, so installation costs can differ by member type; implemented
  in the current prototype;
- assign colours by generated framing class;
- allow materials to be allocated to framing members;
- optionally add a window/door ledger beneath lintels over a configurable
  height, defaulting to `120 mm`. The
  ledger is generally stud-sized, installed length-long by height-short, with
  its underside governed by the window or door head height. Lintel support
  detailing for above-opening jack studs should use total lintel depth: run the
  jack from the underside of the top plate down to the lintel underside when
  the lintel is less than half the stud width, but terminate it at the lintel
  top when the total depth is at least half the stud width, including built-up
  lintels such as `2 x 32 mm = 64 mm`; implemented in the current prototype for
  the nominated single lintel profile. A settings override can continue upper
  jack studs to the lintel underside;
- store member metadata in a JSON blob/record field on the parent PIO;
- optionally create lightweight 2D elevation lines/rectangles for documentation.

### 13.6 Performance

A whole house could easily create thousands of members. Therefore:

- one PIO per wall is better than one PIO per member in v1;
- use LOD settings:
  - schedule only
  - 2D elevation only
  - 3D simplified
  - 3D full
- keep quantities independent of LOD.

---

## 14. Acceptance criteria for v1

### Functional

- User can select one or more Wall PIOs and generate framing.
- User can choose a framing preset from a popup/dialog.
- Tool creates one linked `iQs_StudWallFrame` per source wall.
- Frame object graphically shows plates, studs, noggings and opening members.
- User can change the wall and manually refresh the linked frame.
- Refresh preserves user preset choices and frame UUID.

### Quantity

- Tool stores a member schedule per frame.
- Tool produces rollups by member type, size and grade.
- Tool exports JSON suitable for iQs cost planning.
- Quantities do not depend on graphical LOD.

### Stability

- Tool must not mutate the source wall except to attach an iQs UUID record.
- Tool must not leave orphaned temporary geometry.
- Tool must not require symbols for every stud/member.
- Tool must be safe on multiple selected walls.

---

## 15. v1 non-goals

Keep these out of v1 unless they become unavoidable:

- live drag preview
- structural engineering compliance engine
- AS1684 span validation
- bracing wall design
- tie-down design
- stock length optimisation
- automatic sheet lining/bracing layout
- perfect handling of every custom door/window PIO
- curved wall framing
- raked wall top plates
- multi-storey frame stacking
- fabrication/nailing diagrams

---

## 16. Roadmap

### v1 — Stud wall from straight Wall PIOs

- straight wall support
- basic doors/windows
- timber presets
- visible members
- schedule/export
- manual refresh
- Set lintel size to 90x45 as default; that is 90mm wide and 45 high. This could be a standard feature as a 'header' with the lintel an optional extra...

### v1.1 — Better documentation

- 2D wall frame elevation sheet output
- member labels
- opening labels
- worksheet creation
- JSON export integration with iQs app

### v2 — Production carpentry features

- stock-length optimisation
- cutting lists
- wall panelisation
- bracing and sheet setout
- steel stud presets
- raked walls
- better curved/segmented wall handling

### v3 — Commercial framing package

- client-facing frame reports
- cost plan templates
- supplier integration
- order packs
- revision compare: old frame vs new frame
- cloud handoff to iQs estimating/rate library

---

## 17. Suggested first Codex milestone

Do not start by trying to solve every wall. Start with a proof command:

`iQs > Probe Selected Wall For Framing`

For the selected wall, output a JSON/TXT probe containing:

- handle/type/class/layer
- wall length
- wall height
- wall thickness
- start/end points if available
- wall style name
- component names/classes/materials/thicknesses if available
- inserted objects/doors/windows with bounds and station guesses

Once that probe is stable, build the solver and generator.

---

## 18. Recommended build order

1. Probe selected wall geometry.
2. Attach/get stable `iQs_Host.iqs_uuid` on walls.
3. Generate one simple frame from wall length/height: plates + regular studs only.
4. Add dialog presets.
5. Add openings detection.
6. Add opening framing rules.
7. Add noggings/blocking.
8. Add member schedule JSON blob.
9. Add refresh existing frames.
10. Add exporter integration.

---

## 19. Bottom line

This is very buildable if we keep v1 disciplined:

- Wall PIO is the host/reference.
- iQs owns the generated frame object.
- One frame PIO per wall.
- Presets live in external editable libraries.
- Quantities are generated from the solver, not scraped from dumb geometry.
- Manual refresh is acceptable and probably preferable for v1.

The hard parts are not studs and plates. The hard parts are host-wall extraction, opening stationing, and refresh/version control. Solve those cleanly and the rest becomes a very powerful QS/detailing product.
