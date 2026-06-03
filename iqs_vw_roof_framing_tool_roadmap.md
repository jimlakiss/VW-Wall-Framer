# iQs Vectorworks Roof Framing Tool - Concept Roadmap v0.1

**Target:** Vectorworks 2026+
**Primary implementation:** C++ SDK plug-in / PIOs
**Purpose:** Generate QS-focused roof framing from selected Vectorworks Roof or Roof Face objects by solving member centrelines, cloaking those lines with typed timber sections, and storing reliable quantity/cut metadata.

---

## 1. The idea in one sentence

Build an iQs roof-framing generator that reads selected roof geometry, derives roof planes and key intersection lines, then creates a graph of member centrelines for rafters, ridges, hips, valleys, purlins, battens and ceiling joists. A reusable member engine then cloaks each line with a typed section, reference position and end-cut definition.

This should be a drafting and estimating accelerator, not a structural engineering engine. The tool should generate repetitive members and expose clear manual choices at intersections.

---

## 2. Why roof framing is the next better target

Floor framing is useful, but complex floor plans quickly become a duplicate manual drafting tool: mixed joist sizes, intersecting beams, trimmed joist zones and engineer-specific intent make full automation expensive.

Roof framing has a clearer geometric host:

- roof planes define rafter direction and extents;
- roof-plane intersections naturally identify ridge, hip and valley candidate lines;
- battens and many purlins are repetitive and rule-based;
- ceiling joists can be generated from a nominated horizontal boundary without solving the full roof structure.

The correct v1 stance is manual-first: generate the repetitive work accurately, then let the user decide structural intent where the model cannot know it.

---

## 3. Product shape

The tool should be one **Roof Framing** command/dialog with right-side tabs or panes:

- **Source**
- **Rafters**
- **Ridges / Hips / Valleys**
- **Purlins**
- **Battens**
- **Ceiling Joists**
- **Output**

Each tab should be an independent generation pass. A user should be able to generate rafters only, battens only, ceiling joists only, or any combination.

---

## 4. Core workflow

1. User selects one or more Roof PIOs, Roof Face objects, or a nominated ceiling/plan polygon.
2. User runs `iQs > Generate Roof Framing`.
3. Dialog opens with the right-side framing tabs.
4. Tool probes the selected host geometry and shows the available roof planes or plan boundaries.
5. User chooses member settings per tab.
6. Tool creates or refreshes an `iQs_RoofFrame` object/group linked to the source host.
7. Generated members are placed on a roof timber frame layer/classes and receive iQs exporter metadata.
8. Member schedule is stored/exported for quantities and costing.

Live preview is optional. A reliable button-driven rebuild is enough for early versions.

---

## 5. Object model

Use the same conservative pattern as the wall framer.

### Recommended v1 model

Create one generated frame group/object per source roof host or per selected roof face:

- parent object: `iQs_RoofFrame`
- source record: `iQs_Host`
- generated member record: existing `iQs Extruder V0.1` where practical
- generated member classes under a roof-specific class prefix

Suggested classes:

- `Roof-Timber Frame-Frame group`
- `Roof-Timber Frame-Rafter`
- `Roof-Timber Frame-Ridge beam`
- `Roof-Timber Frame-Hip rafter`
- `Roof-Timber Frame-Valley rafter`
- `Roof-Timber Frame-Purlin`
- `Roof-Timber Frame-Batten`
- `Roof-Timber Frame-Ceiling joist`

Avoid one PIO per member in v1. There may be thousands of members across rafters and battens.

### 5.1 Member-axis model

The roof solver should output **member axes**, not solids.

Each roof member starts as a 3D line or path:

- start point;
- end point;
- optional intermediate path points;
- member type;
- section width and depth;
- local side/up orientation;
- reference position relative to the line;
- start/end cut definitions;
- gross stock length;
- finished length;
- class, material and rate metadata.

The lower-level iQs member engine then "cloaks" the axis:

1. Build a local coordinate frame from the member axis.
2. Place the typed rectangular section around, above, below or offset from that axis.
3. Generate a base rectangular extrude or extrude-along-path.
4. Apply cutting solids for plumb, bevel and compound end cuts when required.
5. Attach iQs records containing the authoritative dimensions and cut metadata.

This keeps the geometry durable and avoids depending on native beam/rafter PIOs. If the visual object later becomes a generic solid after boolean cuts, the attached iQs record remains the source of truth for estimating and scheduling.

---

## 6. Geometry strategy

### 6.1 Roof plane extraction

For each selected roof host, extract:

- host handle, name, type, class and layer;
- host UUID;
- roof style and roof thickness where available;
- roof face count;
- each roof face polygon;
- each roof face 3D polygon or transform;
- bearing line;
- upslope direction;
- rise/run;
- slope angle;
- roof openings if exposed;
- parent roof relationship for roof faces;
- edge list for full Roof PIOs.

### 6.2 Plane-local coordinates

Each roof plane should be solved in local coordinates:

- X axis = across the roof plane along the eave/bearing line;
- Y axis = upslope direction on the roof plane;
- Z axis = normal/elevation derived from the plane transform.

This keeps rafter and batten setout simple. After solving, transform rectangular members back into Vectorworks world coordinates.

### 6.3 Intersection lines

For each pair of roof planes:

1. Compare 2D/3D boundary edges.
2. Find shared or intersecting edge segments.
3. Classify candidate lines:
   - ridge: high line shared by opposing upslope planes;
   - valley: inward/downstream intersection;
   - hip: outward/upstream intersection;
   - eave/verge: exposed plane boundary.
4. Present ambiguous lines as selectable candidates rather than forcing a structural decision.

V1 can be conservative: start with selected Roof Face objects and simple full Roof PIOs, then add complex multi-plane roofs after probing.

### 6.4 Member cloaking and end cuts

The member engine should accept:

- `axis_start` and `axis_end` in world coordinates;
- `section_width_mm` and `section_depth_mm`;
- `reference_mode`, such as centroid, top centre, bottom centre, left/right face or custom offset;
- `local_up`, usually roof-plane normal or world Z depending on member type;
- `start_cut` and `end_cut` definitions;
- metadata for member type, host face, material and class.

Cut definitions should be expressed as solver data, not inferred from the final solid:

- square cut;
- plumb cut;
- bevel cut;
- compound cut;
- future seat/birdsmouth cut.

The geometric implementation can use oversized cutting solids and `SubtractSolid` to remove material from the base member. A plumb cut is a vertical cut plane placed at a member end. A compound cut is a cut plane rotated in both member pitch and plan. For hips and valleys, this is much cleaner if expressed in the member's local coordinate frame before transforming the cutter back to world coordinates.

The output object may be:

- a clean extrude when no cuts are required;
- a CSG solid/subtraction object when cuts are required;
- a generic solid if Vectorworks collapses or converts the CSG result.

All quantity and cut data should come from the iQs record, not from scraping the final solid.

---

## 7. Framing tabs

### 7.1 Source

Controls:

- source mode:
  - selected Roof PIOs
  - selected Roof Face objects
  - selected 2D/3D polygon boundary
  - selected ceiling joist boundary
- update mode:
  - create new
  - refresh linked
  - delete and rebuild
- host linking:
  - attach/update `iQs_Host`
  - preserve previous tab settings

### 7.2 Rafters

Controls:

- rafter size/profile
- spacing
- start offset
- rafter direction:
  - auto from upslope
  - manual picked direction
- bearing/eave rule
- trim to roof face
- trim to ridge/hip/valley candidate lines
- output class/material/rate code

V1 behaviour:

- generate rectangular rafter members on each supported roof plane;
- trim to the roof face polygon;
- stop at detected intersection lines where available;
- allow overshoot/manual cleanup where intersection classification is uncertain.

### 7.3 Ridges / Hips / Valleys

Controls:

- show detected candidate lines
- per-line member type:
  - none/manual
  - ridge beam
  - hip rafter
  - valley rafter
- member size/profile per type
- vertical/plane offset
- output class/material/rate code

V1 should not assume the engineer's intent. It should detect lines and let the user choose what to insert.

### 7.4 Purlins

Controls:

- purlin size/profile
- maximum nominated rafter span
- fixed spacing override
- offset below rafters
- plane selection
- output class/material/rate code

V1 behaviour:

- place purlins as a secondary pass under rafters;
- use max rafter span to determine rows;
- do not attempt full support or strut design.

### 7.5 Battens

Controls:

- batten size/profile
- spacing
- start offset from eave
- stop/trim at ridge or roof face boundary
- plane selection
- output class/material/rate code

Battens are a strong automation candidate because they are repetitive and roof-plane based.

### 7.6 Ceiling Joists

Controls:

- source boundary:
  - roof footprint
  - selected polygon
  - selected room/ceiling boundary
- joist size/profile
- spacing
- direction:
  - auto from selected edge
  - manual picked direction
- bearing/start edge
- trim boundary
- elevation/offset
- align with rafters:
  - off
  - align where possible
- output class/material/rate code

Ceiling joists should be treated as horizontal framing, not roof-plane framing. They may share the same modal, records and output layer, but the solver should use a plan boundary and direction rather than roof slope.

---

## 8. Data model

### 8.1 Host record

Record: `iQs_Host`

| Field | Type | Purpose |
|---|---:|---|
| `iqs_uuid` | Text | Stable host ID |
| `source_type` | Text | `VW_ROOF_PIO`, `VW_ROOF_FACE`, or `VW_POLYGON_BOUNDARY` |
| `last_framed_unix` | Integer | Last generation timestamp |

### 8.2 Roof frame fields

PIO/group: `iQs_RoofFrame`

| Field | Type | Purpose |
|---|---:|---|
| `P_iqs_uuid` | Text | Stable frame ID |
| `P_host_uuid` | Text | Linked source UUID |
| `P_host_type` | Text | Roof, roof face or polygon |
| `P_preset_code` | Text | Active framing preset |
| `P_member_count_total` | Integer | Computed total |
| `P_length_total_lm` | Real | Computed total |
| `P_volume_total_m3` | Real | Computed total |
| `P_last_generated_unix` | Integer | Version/debug |
| `P_solver_status` | Text | Complete, partial, unsupported |

### 8.3 Member schedule

Minimum JSON member entry:

```json
{
  "member_id": "RAF-001",
  "member_type": "RAFTER",
  "source_face_id": "FACE-001",
  "size_code": "140x45 MGP10",
  "width_mm": 45,
  "depth_mm": 140,
  "finished_length_mm": 4200,
  "gross_length_mm": 4285,
  "qty": 1,
  "start": [0, 0, 2400],
  "end": [0, 4200, 3600],
  "reference_mode": "centroid",
  "local_up": [0, 0, 1],
  "start_cut": {
    "type": "plumb",
    "bevel_deg": 0,
    "miter_deg": 0
  },
  "end_cut": {
    "type": "compound",
    "bevel_deg": 30,
    "miter_deg": 45
  },
  "material": "MGP10 Pine",
  "grade": "MGP10",
  "class": "Roof-Timber Frame-Rafter",
  "geometry_source": "iqs_member_axis_boolean_cut_solid"
}
```

---

## 9. SDK findings and risks

This appears buildable. No major SDK showstopper is obvious from the current headers, but roof probing needs to be proven before committing to the full solver.

### 9.0 Existing iQs roof extractor lessons

There is already a working roof extraction path in the iQs SDK exporter:

`/Users/jameslakiss/Documents/Develop/iQs/11 VW SDK/SDK/SDKVW(832364)/Source/Samples/EmptyModule/Source/ModuleMain.cpp`

Supporting status references:

- `/Users/jameslakiss/Documents/Develop/iQs/11 VW SDK/SDK/IQS_VW_INTEGRATION_STATUS_V1.md`
- `/Users/jameslakiss/Documents/Develop/iQs/11 VW SDK/SDK/PLUGIN_SUPPORT_CHECKLIST.md`

Reuse these lessons before writing any new roof-framer probe:

- Native Roof PIOs are object type `83`.
- Individual roof face members are typically object type `71`.
- Roof Face objects are object type `84`.
- The discovered useful hierarchy for a native Roof PIO is:
  - `t=83` roof
  - direct child `t=71` roof face objects
  - child `t=21` net plan-area polygon objects under each `t=71` face
- `gSDK->ObjArea` returns `0` for native Roof PIO `t=83`, so do not rely on it for roof area.
- `GetComponentNetArea(h, componentIndex)` on a `t=83` roof returns gross sloped roof area, with skylights/openings not deducted.
- For roof faces, `ObjArea(t=71)` gives gross sloped face area.
- Net face plan area can be found from the `t=21` child polygon under each `t=71` face.
- Net sloped face area can be derived as `net_plan_area / cos(slope)`, where slope comes from the roof edge/face data.
- Traversal should guard against leaving the selected roof's hierarchy. The existing exporter uses a parent-walk `isDescendantOf(child, roof)` helper while iterating members.
- Temporary converted geometry must be deleted. The exporter already uses temporary polygon/solid vertex extraction patterns and deletes generated handles afterward.

For framing, this means the first probe should not start from a blank SDK search. It should copy the known roof traversal pattern, then extend the payload from quantity areas into actual plane boundaries, face transforms and member insertion axes.

Note: the saved `/Users/jameslakiss/Documents/Develop/iQs/11 VW SDK/iqs_export_selection.json` checked during this review did not contain any `t=83` or `t=84` roof records, so it is not a useful live roof payload sample.

### 9.1 Useful SDK surfaces found

The Vectorworks 2026 SDK includes roof-specific C++ interfaces and VWFC wrappers:

- `VWFC::VWObjects::VWRoofObj`
  - `IsRoofObject`
  - `GetRoofPoly`
  - `GetFacesCount`
  - `GetFaceAt`
  - `GetIsFaceDormer`
  - `GetGableWall`
  - `GetBearingInset`
  - `GetRoofThick`
  - `GetMiterType`
- `VWFC::VWObjects::VWRoofFaceObj`
  - `IsRoofFaceObject`
  - `GetParentRoof`
  - `GetPolygon`
  - `GetPolygon3D`
  - `GetBearingStartPos`
  - `GetBearingEndPos`
  - `GetRotationTransform`
  - `GetThickness`
  - `GetIntersectionSegments`
  - `GetOpeningPolygons`
  - `IsLyingOnSide`
- lower-level `ISDK` calls:
  - `GetRoofAttributes`
  - `GetRoofEdge`
  - `GetRoofElementType`
  - `GetNumRoofElements`
  - `GetRoofStyle`
  - `GetDatumRoofComponent`
  - `CalcRoofTopArea`
  - `AddSolid`
  - `SubtractSolid`
  - `IntersectSolid`
  - `SectionSolid`
  - `CreateExtrude`
  - `CreateExtrudeAlongPath`
  - `Create3DPoly`
  - `CreateGroup`
  - `CreateCustomObjectPath`
  - `SetEntityMatrix`
  - `GetEntityMatrix`
  - `GetVerticesFromSolid`
  - `GetNurbsCurvesFromSolid`
  - `GetNurbsSurfacesFromSolid`
  - NURBS and 3D polygon creation/evaluation helpers

The script/Python catalogue also exposes useful roof probes:

- `GetRoofVertices`
- `GetRoofEdge`
- `GetRoofFaceAttrib`
- `GetRoofFaceCoords`
- `ConvertTo3DPolys`

These script calls are useful for proof probing if the C++ wrappers do not expose a detail directly.

The SDK also includes working examples around solid boolean operations in `VWSolidObj.cpp`, including `GS_SubtractSolid` used to clip extrudes. That supports the proposed plumb/compound cut approach: create a member solid, create oversized cutter solids, subtract them, then keep iQs records as the authoritative data.

### 9.2 Likely technical risks

Main risks:

- Full Roof PIOs may expose faces differently from standalone Roof Face objects.
- Complex roofs, dormers, skylights and holes may need filtering before v1. Existing area extraction showed roof openings are not deducted from native roof component areas, so opening handling must be explicit if we want rafters/battens trimmed around skylights.
- Plane intersection classification may be ambiguous and should remain user-confirmable.
- Generated members need robust 3D transforms so rafters/battens sit correctly on sloped planes.
- Boolean-cut member output may become CSG/generic solid geometry. This is acceptable only if iQs records store member axis, section, gross length, finished length and cut metadata.
- Compound cutter math must be tested in a member-local frame before being used across hips/valleys.
- A full roof with battens can create many members; geometry level-of-detail and grouping matter.
- Ceiling joists need a clear horizontal boundary; deriving that perfectly from every roof is not a v1 assumption.

### 9.3 No-go signals to check in the probe

Before implementing the generator, confirm:

- selected Roof PIOs return face handles via `VWRoofObj::GetFacesCount` / `GetFaceAt`;
- selected standalone Roof Face objects return usable 2D and 3D polygons;
- `GetRoofFaceCoords`/`GetRoofFaceAttrib` agree with C++ roof-face data;
- roof openings/skylights can be detected or safely ignored;
- generated test extrudes can be aligned to a sloped roof plane with correct length, width and depth metadata;
- a base rectangular member can be cut with `SubtractSolid` using one plumb cutter and one compound cutter;
- the resulting object can retain an attached iQs record after boolean operations;
- `GetVerticesFromSolid`, object cube/bounds, surface area and volume remain available for sanity checks on the generated solid;
- converted 3D polygons are only used as a fallback and do not require mutating the source roof.

If those checks pass, there is no obvious SDK blocker for v1.

---

## 10. Proof command roadmap

Start with a probe, not a generator.

### Milestone 1 - Probe selected roof hosts

Command:

`iQs > Probe Selected Roofs For Framing`

Output JSON:

- selected object handle/type/class/layer/name;
- whether it is a Roof PIO or Roof Face;
- for Roof PIO `t=83`, direct child roof-face handles and types;
- for each roof face child `t=71`, any child `t=21` net plan polygon;
- roof style name/index;
- roof thickness;
- roof face count;
- per-face polygon vertices;
- per-face 3D vertices;
- per-face bearing line;
- per-face upslope vector;
- per-face rise/run and slope;
- roof edge count and edge attributes;
- dormer/skylight/opening hints if available.

### Milestone 2 - Generate one roof face rafters

- select one simple Roof Face;
- generate rafter centrelines at fixed spacing;
- cloak each centreline with a typed rectangular section;
- trim to the roof face polygon;
- store member records and JSON schedule;
- verify dimensions against Vectorworks object measurements.

### Milestone 3 - Prove plumb and compound member cuts

- create one sloped member from a typed section and centreline;
- apply one plumb cut using an oversized cutter solid;
- apply one compound cut using a member-local cutter definition;
- verify the boolean result remains schedulable through iQs records;
- compare object bounds, vertices, surface area and volume as sanity checks only.

### Milestone 4 - Generate battens on one roof face

- generate battens perpendicular to rafters;
- trim to roof face;
- validate count and lengths.

### Milestone 5 - Multi-face roof candidate lines

- select a simple full Roof PIO;
- extract all faces;
- detect candidate ridge/hip/valley/eave lines;
- output debug lines/classes before inserting structural members.

### Milestone 6 - Insert ridge/hip/valley members

- present detected lines in the dialog;
- allow per-line type selection;
- generate selected ridge/hip/valley member axes;
- cloak each axis with section and end-cut metadata.

### Milestone 7 - Purlins

- place rows under rafters based on max nominated rafter span;
- keep support assumptions manual.

### Milestone 8 - Ceiling joists

- start from a selected polygon/roof footprint;
- generate horizontal ceiling joists by direction and spacing;
- later add optional alignment with rafters.

---

## 11. v1 non-goals

Keep these out of v1:

- structural compliance or AS1684 span validation;
- automatic engineering design of beams/struts/strutting beams;
- truss design;
- dormer/skylight trimming beyond safe ignore/report;
- curved or warped roof planes;
- perfect valley/hip classification on complex roofs;
- automatic roof-to-wall load path decisions;
- stock optimisation;
- live interactive preview.

---

## 12. Recommended build order

1. Port the existing exporter roof traversal into this project as a roof probe command.
2. Prove Roof PIO `t=83`, face child `t=71`, face object `t=84`, and net plan polygon `t=21` extraction in JSON.
3. Extend the probe from areas into usable face boundary vertices, 3D vertices, face axes and transforms.
4. Build the `CreateIqsFramingMember(axis, section, reference, cuts, metadata)` member-cloaking engine.
5. Prove sloped rectangular member generation on one roof face.
6. Prove boolean plumb and compound cuts using `SubtractSolid`.
7. Add rafter solver for one selected roof face.
8. Add batten solver for one selected roof face.
9. Add full Roof PIO face iteration.
10. Add candidate ridge/hip/valley detection and debug graphics.
11. Add modal tabs and persisted settings.
12. Add ridge/hip/valley insertion.
13. Add purlins.
14. Add ceiling joists from selected polygon/footprint.
15. Add quantity rollups and iQs exporter integration.

---

## 13. Bottom line

The roof framer is worth pursuing, provided v1 is disciplined:

- start from roof probing;
- prefer Roof Face support first;
- solve member axes first, then cloak those axes with typed sections and end cuts;
- generate rafters and battens before solving complex intersections;
- expose ridge/hip/valley decisions to the user;
- treat ceiling joists as a horizontal boundary solver inside the same roof-framing modal;
- keep all output editable, classed and quantity-ready.

The SDK has enough roof and geometry hooks to justify a prototype. The main unknown is quality of roof-face extraction from real project Roof PIOs, so the first milestone should be a roof probe JSON command.

The earlier iQs exporter already solved the first hard part: native Roof PIO member traversal and the face/net-plan hierarchy. The framer should treat that extractor as the starting point, then add the extra geometric data needed for member placement.
