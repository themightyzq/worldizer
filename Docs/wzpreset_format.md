# The `.wzpreset` Format

A Worldizer preset packages a scene's geometry, its pre-baked impulse response, a
thumbnail, and descriptive metadata. This document is the reference for authoring
and parsing presets. (Implemented by `Source/IO/WzPresetIO.{h,cpp}`.)

## Bundle layout

A `.wzpreset` is a **directory**:

```
small_concrete_room.wzpreset/
├── geometry.json    # scene structure (brushes, source/mic defaults)
├── rendered.wav     # pre-baked impulse response
├── thumbnail.png    # 128x128 top-down visualization (placeholder for now)
└── metadata.json    # name, category, description, author, character defaults
```

The directory name without the `.wzpreset` extension is the preset's **id**
(e.g. `small_concrete_room`). At runtime the plugin reads `rendered.wav` +
`metadata.json` to use a preset, and `geometry.json` only when editing (Slice 7).

Zip packaging of bundles is a v1.0 distribution concern; the runtime works with
directories. (Shipped presets are additionally packed into a single `.wzpkg` blob
for binary embedding — see "Embedded format" below.)

## `geometry.json`

```json
{
  "$schema": "worldizer-geometry-v1",
  "version": 1,
  "bounds": { "min": [-3.0, -2.0, 0.0], "max": [3.0, 2.0, 3.0] },
  "brushes": [
    {
      "id": "floor",
      "kind": "additive",
      "type": "box",
      "min": [-3.0, -2.0, -0.1],
      "max": [ 3.0,  2.0,  0.0],
      "material": "concrete"
    }
  ],
  "source_default": { "position": [0.0, 0.0, 1.5] },
  "mic_default":    { "position": [2.0, 0.0, 1.5], "radius": 0.1 }
}
```

| Field | Meaning |
|-------|---------|
| `version` | Schema version (currently `1`). Bumped only on breaking changes. |
| `bounds` | AABB of all geometry (`min`/`max`, metres). Informational — the ray tracer derives its own bounds from the brushes. |
| `brushes[]` | One object per brush. |
| `brushes[].id` | Unique-within-preset string. |
| `brushes[].kind` | `"additive"` (solid, reflects) or `"subtractive"` (carves space — parsed but not yet ray-traced). |
| `brushes[].type` | `"box"` only in MVP. |
| `brushes[].min` / `max` | Box corners, `[x, y, z]` in metres. |
| `brushes[].material` | Material name (see Materials below). |
| `source_default.position` | `[x, y, z]` in metres. Orientation/directivity are later additions. |
| `mic_default.position` | `[x, y, z]` in metres. |
| `mic_default.radius` | Sphere-of-acceptance radius (metres). |

Coordinates are metres, +Z up. Rooms are built from thin slab brushes (one per
surface); rays travel the interior and reflect off the room-facing faces.

### Materials

`material` references a name resolved by `MaterialResolver` (Slice 9 will replace
this with a measured JSON library). Known names:

`concrete`, `drywall`, `wood_floor`, `carpet`, `curtain`, `glass`, `foliage`,
`gravel`, `open_air`.

An unknown name resolves to `drywall` with a logged warning — a preset never fails
to load over a material name.

## `metadata.json`

```json
{
  "$schema": "worldizer-metadata-v1",
  "version": 1,
  "name": "Small Concrete Room",
  "category": "Test",
  "description": "A 6x4x3 m room ...",
  "author": "ZQSFX",
  "tags": ["test", "small", "indoor", "concrete"],
  "ambient_bed": null,
  "default_source_character": null,
  "default_mic_character": null,
  "rendered_at": "2026-05-26T12:00:00Z",
  "render_settings": { "num_rays": 100000, "max_bounces": 32, "sample_rate": 48000, "random_seed": 42 }
}
```

| Field | Meaning |
|-------|---------|
| `name` | Human-readable; shown in the UI. |
| `category` | One of `Indoor`, `Outdoor`, `Vehicles/Devices`, `Cinematic`, `Experimental`, `Test`. |
| `description` | Long-form text (optional but encouraged). |
| `tags[]` | Free-form, for future search/filter. |
| `ambient_bed` | Room-tone filename, or `null` (Slice 6 wires this up). |
| `default_source_character` / `default_mic_character` | Character filenames, or `null` (Slice 5). |
| `rendered_at` | ISO 8601 bake timestamp (diagnostic). |
| `render_settings` | Parameters used to bake `rendered.wav` (diagnostic; makes renders reproducible). |

## `rendered.wav`

The baked IR: **mono, 48 kHz, 32-bit float**, length ≤ 6 seconds (the shipped test
presets are 4 s). Loaded into the convolver when the preset is selected. The
convolver applies the mono IR to each channel (`Trim::yes`, `Normalise::yes`) and
resamples internally if the host runs at a different rate.

## `thumbnail.png`

128×128 PNG, top-down visualization. **Currently a placeholder** — a dark square
with the preset name in amber. Real geometry-rendered thumbnails arrive with
`RoomView2D` in Slice 4.

## Forward compatibility

- Parsers **ignore unknown fields** — adding fields is non-breaking.
- The schema `version` is bumped **only** on breaking changes; a higher version is
  read best-effort with a logged warning rather than failing.
- Missing optional fields fall back to sensible defaults.

## Embedded format (`.wzpkg`)

Shipped presets are embedded in the plugin binary. Because every bundle contains a
`geometry.json` (etc.), embedding the raw files would collide in the generated
binary-data symbol table. Instead each bundle is packed into a single `.wzpkg`
blob — one unique symbol per preset — with a trivial length-prefixed layout:

```
"WZP1"                      (4-byte magic)
uint32  entryCount          (little-endian)
repeat entryCount times:
    uint32 nameLength
    bytes  name (UTF-8: "geometry.json", "metadata.json", "rendered.wav", "thumbnail.png")
    uint32 dataLength
    bytes  data
```

`WzPresetIO::packBundle()` writes these; `readFromBinaryData()` reads them.

## Authoring a preset by hand

You don't need the (future) in-plugin editor:

1. Create a directory `my_room.wzpreset/`.
2. Write `geometry.json` describing the brushes, source, and mic (per the schema above).
3. Bake an IR from that geometry — currently via the engine; a standalone "render a
   geometry.json" path will be exposed as the editor matures. For now, the simplest
   route is to base a preset on one of the shipped `Test` presets and edit it.
4. Write `metadata.json` with at least a `name` and `category`.
5. Optionally add a `thumbnail.png` (or let the tool generate a placeholder).
6. Drop the directory into your user presets folder (see README) and restart the plugin.

Unknown materials, missing thumbnails, and missing optional metadata are all
tolerated — the loader degrades gracefully and logs warnings.
