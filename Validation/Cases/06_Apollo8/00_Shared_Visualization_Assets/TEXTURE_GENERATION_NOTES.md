# Apollo 8 CSM texture-generation notes

The six base-color materials were generated with OpenAI ImageGen in reference-
guided generation mode, using the two supplied Apollo CSM screenshots only for
material and color guidance. They were then made tileable, resized to 1024 by
1024 pixels, and used to derive restrained normal, roughness, and metallic maps.
The final PBR maps are runtime assets; no generated-image location or absolute
path is required.

## Final prompts

### Service-module silver

Create one square 1024×1024 seamless tileable PBR base-color source texture for
the Apollo 8 Command and Service Module, using the two attached Apollo reference
images only as material/color guidance. Material: the Service Module exterior,
cool satin silver-gray spacecraft sheet metal with very subtle longitudinal
brushed grain, faint alternating panel tones, restrained small manufacturing
variation, and a clean late-1960s aerospace appearance. Orthographic flat
material swatch filling the whole canvas. Uniform diffuse studio illumination.
No object, spacecraft silhouette, perspective, directional shading, cast
shadows, strong reflections, baked specular highlight, labels, lettering,
insignia, seams that do not tile, border, or watermark. Photorealistic but
understated; texture scale suitable for a full-size spacecraft cylinder.

### Command-module charcoal

Create one square 1024×1024 seamless tileable PBR base-color source texture for
the Apollo 8 Command and Service Module, using the two attached Apollo reference
images only as material/color guidance. Material: Command Module upper shell
exterior, dark charcoal graphite-gray metallic thermal covering, nearly black in
shadow but with restrained cool steel variation, fine overlapping foil/sheet
character, subtle creases and small-scale irregularity, historically plausible
late-1960s spacecraft finish. Orthographic flat material swatch filling the
canvas. Uniform diffuse studio illumination. No object, cone, spacecraft
silhouette, perspective, directional shading, cast shadows, bright glare, baked
specular highlight, labels, lettering, insignia, large seams, border, or
watermark. Photorealistic, low-relief and moderately smooth rather than coarse.

### Heat-shield ablative surface

Create one square 1024×1024 seamless tileable PBR base-color source texture for
the Apollo 8 Command and Service Module, using the two attached Apollo reference
images only as contextual guidance. Material: Command Module aft heat shield,
very dark carbonized phenolic ablative surface, charcoal black with restrained
warm brown undertones, fine dense mottling and faint cured-resin variation,
intact pre-entry condition, low relief and not rocky. Orthographic flat material
swatch filling the whole canvas under uniform diffuse illumination. No object,
disk, spacecraft silhouette, perspective, directional shading, cast shadows,
glowing embers, cracks, craters, large seams, baked specular highlight, labels,
lettering, border, or watermark. Photorealistic and seamless.

### SPS nozzle

Create one square 1024×1024 seamless tileable PBR base-color source texture for
the Apollo 8 Service Propulsion System engine and nozzle, using the two attached
Apollo reference images only as material/color guidance. Material: heat-aged
polished nickel-steel aerospace metal, medium-to-bright silver with extremely
subtle concentric machining grain, restrained blue-gray and faint straw heat
tint variation, smooth metallic surface, no soot damage. Orthographic flat
material swatch filling the whole canvas under uniform diffuse illumination. No
engine shape, bell, spacecraft silhouette, perspective, directional shading,
cast shadows, mirror-like glare, baked specular highlight, labels, lettering,
fasteners, large seams, border, or watermark. Photorealistic, smooth, seamless,
and suitable for a PBR base-color map.

### Dark external hardware

Create one square 1024×1024 seamless tileable PBR base-color source texture for
Apollo 8 spacecraft external mechanisms, using the two attached Apollo reference
images only as material/color guidance. Material: dark gunmetal aerospace
hardware for RCS quads, brackets, umbilical housing, and aft mechanisms; neutral
charcoal gray with subtle machined-metal grain, small restrained tonal variation,
clean flight hardware, moderately smooth satin finish. Orthographic flat material
swatch filling the whole canvas under uniform diffuse illumination. No hardware
object, thruster, spacecraft silhouette, perspective, directional shading, cast
shadows, glare, baked specular highlight, bolts, labels, lettering, border, or
watermark. Photorealistic, fine scale, and seamless.

### High-gain-antenna coating

Create one square 1024×1024 seamless tileable PBR base-color source texture for
the Apollo 8 deployed S-band high-gain antenna dishes, using the two attached
Apollo reference images only as contextual material guidance. Material: clean
off-white to pale-silver spacecraft antenna surface, lightly coated aluminum,
faint fine mesh/fabric grain, restrained cool-gray variation, smooth satin
finish. Orthographic flat material swatch filling the whole canvas under uniform
diffuse illumination. No antenna shape, dish, spacecraft silhouette,
perspective, directional shading, cast shadows, glare, baked specular highlight,
grid lines, labels, lettering, border, or watermark. Photorealistic, subtle,
seamless, and not rough.

