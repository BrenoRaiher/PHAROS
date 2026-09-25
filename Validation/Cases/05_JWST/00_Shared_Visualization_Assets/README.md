# Shared JWST visualization assets

This directory contains portable display-only assets for the optional JWST
TGSCN variants. It is not part of the physical validation configuration.

## Geometry

The sixteen supplied STScI printable components preserve a common assembly
coordinate system. Their vertices were uniformly scaled, centered, and rotated
into the validation spacecraft body frame. Because the source primary-mirror
and secondary-mirror files weld optical faces to non-optical structure, those
two files were each partitioned by face position and normal. The result is
eighteen meter-unit material-region STLs, allowing gold only on the two optical
faces, graphite-black on the primary-mirror backplane and instruments, and
polished aluminum on the secondary-mirror supports and mounts.

The body-frame convention is `+X = source +Z`, `+Y = source -Y`, and `+Z =
source +X`. This is the former assembly mapping followed by a -90-degree
right-handed rotation about body `+Y`: the primary-mirror side is on physical
`+X`, aligned with the modeled thrust vector, while the spacecraft/nozzle side
is on `-X`.

The uniform scale is `0.0554508910915` m per
source coordinate unit. It calibrates the complete model's long axis to the
published 21.197 m Webb sunshield dimension. No TGSCN-side recentering, scale,
or orientation is required.

The source package describes part 10 as a pin that should be printed four
times. Only the single supplied baked placement is included here; no additional
placements were invented. This tiny print-assembly feature does not affect
simulation physics.

## Materials and opacity

Each material has base-color, normal, roughness, and metallic maps. The optical
faces use smooth gold; the primary-mirror backplane and instrument shield use
charcoal graphite; the secondary-mirror arms and mounts use polished aluminum;
and the deployer booms, covers, and spacecraft bus use highly specular
rose-silver metal. The solar array and aft momentum flap use the dark-blue
photovoltaic material. The two five-layer sunshield meshes alone use
`base_color_tint` alpha 0.92, retaining subtle translucency without allowing the
background to dominate.

The assignment follows NASA's description of gold-coated beryllium mirrors and
of five Kapton layers coated with aluminum, with doped silicon giving the two
hottest layers their lavender/pink cast:

- https://science.nasa.gov/mission/webb/webbs-mirrors/
- https://science.nasa.gov/mission/webb/webbs-sunshield/

PHAROS retains all four PBR maps when tint alpha selects its translucent
material. Opacity is uniform per component; this format does not provide a
separate spatial opacity map.

All display components are fixed, massless, have zero inertia, and set
`srp.included_in_proxy = false`. The optional TGSCNs also set the physical
`Bus` and `Propellant` visuals to invisible so their default blue primitives do
not protrude through the detailed assembly.

## Provenance

The STL package is the STScI/Webb Telescope detailed 3D-printing model. Its
embedded metadata credits NASA, ESA, and CSA and is preserved verbatim under
`source_documentation/`. The metadata did not state a redistribution license;
check the originating STScI/Webb media page before public redistribution.

See `geometry_transform_manifest.json` and `textures/jwst_pbr_manifest.json`
for hashes and transformation/material parameters.
