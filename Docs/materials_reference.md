# Materials Reference

> **Placeholder.** This document will hold the human-readable reference tables for Worldizer's acoustic material library. It is not the source of truth.

The **canonical material data lives in `Resources/Materials/materials.json`** — that file is what the ray tracer and editor load at runtime. This document exists to explain and tabulate that data for contributors and the curious.

When the material library is populated (Slice 9), this reference will contain, for each material:

- **Name and id** (e.g. `concrete_smooth`, `drywall`, `carpet_heavy`)
- **Absorption coefficient per octave band** — 8 bands (see `Worldizer::kIRBands` in `Source/Shared/Constants.h`); `0.0` = perfectly reflective, `1.0` = perfectly absorptive
- **Scattering coefficient** — `0.0` = fully specular, `1.0` = fully diffuse
- **Source / notes** — where the measured data came from, or that it is an informed estimate

See the `Material` struct in `Source/Model/Material.h` for the in-memory representation, and `architecture.md` §2 for how materials feed the ray tracer.
