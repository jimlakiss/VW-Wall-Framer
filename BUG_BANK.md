# Bug Bank

## Component-offset junction overlap

- Seen in `probe-output/wall_framing_probe_20260613_084129.json` and
  `probe-output/stud_wall_frame_generation_20260613_084129.json`.
- Selected set had a 295 mm external wall whose `Timber Frame` component centre
  was offset `77.5 mm` from the wall object centreline, joining plain 140 mm
  internal frame walls with `0 mm` component offset.
- The visual wall objects meet at corners, but the generated timber-frame
  component centrelines do not resolve the same way. Junction decisions then
  apply 70 mm half-frame trims/extensions to the 140 mm walls while the
  multi-component external wall does not record matching corner decisions.
- This appears isolated to component-offset junction geometry, not openings,
  lintels, or duplicate generated members.
- Later fix should compare selected framing-component centreline/face geometry
  at junctions, especially where a multi-component wall joins a single-component
  140 mm frame wall.
- Possible Vectorworks-specific signal: capped wall joins. SDK exposes
  `GetWallCaps`, `GetWallCapsOffsets`, `GetWallEndCapFootPrint`, and
  `GetComponentAlwaysAutoJoinInCappedJoinMode`; `JoinWalls` also accepts a
  `capped` flag. Probe these before inventing more junction heuristics.
