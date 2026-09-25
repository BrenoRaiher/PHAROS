# Shared Apollo 8 visualization assets

This directory contains the portable runtime assets referenced by every Apollo
8 `*_with_visual_appearance.tgscn` file. Keep it at the Apollo validation root
so the `../../../00_Shared_Visualization_Assets/...` paths resolve unchanged.

## Geometry

The eleven STL files under `apollo8_csm_common_frame` come from the supplied
`Apollo8_CSM_Assembly_V3_CoupledHatch_SMBodyAligned` package. They are the
package's `CommonFrame_Baked` representation:

- coordinates are full-scale centimeters;
- every mesh keeps the same imported origin;
- every mesh is imported with identity orientation and unit scale;
- `+X` points toward the Command Module nose, `+Y` is spacecraft starboard,
  and `+Z` is the crew-feet direction;
- the SPS engine bell extends aft along `-X`, consistent with the validation
  scenario's SPS force direction of body `+X`.

The merged reference STL and duplicate `ComponentLocal` set are intentionally
not included because the visual TGSCNs use only the common-frame parts.

Original model attribution and ShareAlike information are retained under
`source_documentation/SOURCE_LICENSE.txt`. The accompanying assembly,
transform, geometry-sanity, coupled-hatch, and body-axis notes are included for
traceability. The original 3D model is credited to lowracer / Pinshape.

## Materials

The `textures` directory contains 1024 by 1024 base-color, normal, roughness,
and metallic PNG maps for six whole-component materials:

- service-module satin silver;
- command-module charcoal thermal covering;
- command-module ablative heat shield;
- polished SPS nozzle metal;
- dark external hardware;
- coated-aluminum high-gain antenna.

The restrained roughness and normal amplitudes are intentional so the CSM does
not become excessively coarse under direct solar illumination. The script in
`textures/tools` can rebuild all derived PBR maps from the checked-in base-color
maps. Prompt and processing provenance is recorded in
`TEXTURE_GENERATION_NOTES.md`.

## Physical isolation

Every TGSCN component using these assets is fixed, massless, and explicitly
configured with `srp.included_in_proxy = false`. The meshes are therefore
presentation-only and do not contribute mass, inertia, articulation, or optical
facets to the retained validation model.

