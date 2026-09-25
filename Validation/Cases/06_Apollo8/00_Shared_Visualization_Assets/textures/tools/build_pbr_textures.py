"""Build portable Apollo 8 CSM PBR texture sets.

The checked-in base-color maps are sufficient to rebuild the derived normal,
roughness, and metallic maps.  When ``source_imagegen`` is present, the script
also prepares seamless 1024-pixel base-color maps from the selected ImageGen
source swatches.
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter


TEXTURE_DIR = Path(__file__).resolve().parents[1]
SOURCE_DIR = TEXTURE_DIR / "source_imagegen"
SIZE = 1024

# name: (source file, contrast, roughness, roughness variation, metallic,
#        metallic variation, normal strength)
MATERIALS = {
    "service_module_silver": (
        "service_module_silver_source.png", 0.88, 0.27, 0.035, 0.94, 0.015, 0.75
    ),
    "command_module_charcoal": (
        "command_module_charcoal_source.png", 0.90, 0.31, 0.045, 0.82, 0.020, 0.85
    ),
    "heat_shield_ablative": (
        "heat_shield_ablative_source.png", 0.90, 0.52, 0.055, 0.04, 0.010, 0.70
    ),
    "sps_nozzle": (
        "sps_nozzle_source.png", 0.86, 0.20, 0.030, 0.97, 0.010, 0.50
    ),
    "dark_hardware": (
        "dark_hardware_source.png", 0.88, 0.29, 0.040, 0.88, 0.015, 0.65
    ),
    "hga_coated_aluminum": (
        "hga_coated_aluminum_source.png", 0.82, 0.36, 0.030, 0.42, 0.015, 0.45
    ),
}


def _seamless_base(source: Image.Image, contrast: float) -> Image.Image:
    image = source.convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    image = ImageEnhance.Contrast(image).enhance(contrast)

    # Mirrored 2x2 tiling followed by a centered crop makes opposite boundaries
    # continuous without introducing a visible blur band at the tile edge.
    mirror_x = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    mirror_y = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    mirror_xy = mirror_x.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    tiled = Image.new("RGB", (2 * SIZE, 2 * SIZE))
    tiled.paste(image, (0, 0))
    tiled.paste(mirror_x, (SIZE, 0))
    tiled.paste(mirror_y, (0, SIZE))
    tiled.paste(mirror_xy, (SIZE, SIZE))
    image = tiled.crop((SIZE // 2, SIZE // 2, 3 * SIZE // 2, 3 * SIZE // 2))

    array = np.asarray(image, dtype=np.float32)
    array[:, 0, :] = array[:, -1, :] = 0.5 * (array[:, 0, :] + array[:, -1, :])
    array[0, :, :] = array[-1, :, :] = 0.5 * (array[0, :, :] + array[-1, :, :])
    return Image.fromarray(np.clip(array, 0, 255).astype(np.uint8), "RGB")


def _detail_field(base: Image.Image) -> np.ndarray:
    gray = np.asarray(base.convert("L"), dtype=np.float32) / 255.0
    low = np.asarray(base.convert("L").filter(ImageFilter.GaussianBlur(7.0)), dtype=np.float32) / 255.0
    detail = gray - low
    scale = float(np.percentile(np.abs(detail), 98.0))
    if scale < 1.0e-6:
        return np.zeros_like(detail)
    return np.clip(detail / scale, -1.0, 1.0)


def _normal_map(detail: np.ndarray, strength: float) -> Image.Image:
    smooth = np.asarray(
        Image.fromarray(((detail + 1.0) * 127.5).astype(np.uint8), "L").filter(
            ImageFilter.GaussianBlur(0.75)
        ),
        dtype=np.float32,
    )
    smooth = smooth / 127.5 - 1.0
    dy, dx = np.gradient(smooth)
    nx = -strength * dx
    ny = -strength * dy
    nz = np.ones_like(nx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack((nx / norm, ny / norm, nz / norm), axis=-1)
    rgb = np.clip((rgb * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
    rgb[:, 0, :] = rgb[:, -1, :] = 0.5 * (
        rgb[:, 0, :].astype(np.float32) + rgb[:, -1, :].astype(np.float32)
    )
    rgb[0, :, :] = rgb[-1, :, :] = 0.5 * (
        rgb[0, :, :].astype(np.float32) + rgb[-1, :, :].astype(np.float32)
    )
    return Image.fromarray(rgb.astype(np.uint8), "RGB")


def _scalar_map(mean: float, variation: float, detail: np.ndarray) -> Image.Image:
    values = np.clip(mean + variation * detail, 0.0, 1.0)
    values[:, 0] = values[:, -1] = 0.5 * (values[:, 0] + values[:, -1])
    values[0, :] = values[-1, :] = 0.5 * (values[0, :] + values[-1, :])
    return Image.fromarray(np.round(values * 255.0).astype(np.uint8), "L")


def main() -> None:
    report: dict[str, object] = {"size_px": SIZE, "materials": {}}
    preview_rows: list[Image.Image] = []

    for name, spec in MATERIALS.items():
        source_name, contrast, roughness, rough_var, metallic, metal_var, normal_strength = spec
        base_path = TEXTURE_DIR / f"{name}_base_color.png"
        source_path = SOURCE_DIR / source_name

        if source_path.exists():
            with Image.open(source_path) as source:
                base = _seamless_base(source, contrast)
            base.save(base_path, optimize=True)
        elif base_path.exists():
            with Image.open(base_path) as existing:
                base = existing.convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
            base.save(base_path, optimize=True)
        else:
            raise FileNotFoundError(f"Missing both {source_path} and {base_path}")

        detail = _detail_field(base)
        normal = _normal_map(detail, normal_strength)
        rough = _scalar_map(roughness, rough_var, detail)
        metal = _scalar_map(metallic, metal_var, -detail)

        normal_path = TEXTURE_DIR / f"{name}_normal.png"
        rough_path = TEXTURE_DIR / f"{name}_roughness.png"
        metal_path = TEXTURE_DIR / f"{name}_metallic.png"
        normal.save(normal_path, optimize=True)
        rough.save(rough_path, optimize=True)
        metal.save(metal_path, optimize=True)

        thumb_size = 256
        row = Image.new("RGB", (4 * thumb_size, thumb_size))
        for index, item in enumerate((base, normal, rough.convert("RGB"), metal.convert("RGB"))):
            row.paste(item.resize((thumb_size, thumb_size), Image.Resampling.LANCZOS), (index * thumb_size, 0))
        preview_rows.append(row)

        report["materials"][name] = {
            "base_color": base_path.name,
            "normal": normal_path.name,
            "roughness": rough_path.name,
            "metallic": metal_path.name,
            "mean_roughness": roughness,
            "mean_metallic": metallic,
        }

    preview = Image.new("RGB", (1024, 256 * len(preview_rows)), (24, 24, 24))
    for index, row in enumerate(preview_rows):
        preview.paste(row, (0, index * 256))
    preview.save(TEXTURE_DIR / "apollo8_pbr_contact_sheet.png", optimize=True)

    (TEXTURE_DIR / "apollo8_pbr_manifest.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
