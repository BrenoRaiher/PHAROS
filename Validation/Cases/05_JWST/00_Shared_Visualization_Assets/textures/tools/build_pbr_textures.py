"""Rebuild the derived JWST PBR maps from checked-in base colors.

This helper intentionally preserves the checked-in base-color maps and only
regenerates normal, roughness, metallic, contact-sheet, and manifest products.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


@dataclass(frozen=True)
class Material:
    key: str
    source_image: str
    generated_file: str
    contrast: float
    brightness: float
    roughness: float
    roughness_variation: float
    metallic: float
    metallic_variation: float
    normal_strength: float


TEXTURE_DIR = Path(__file__).resolve().parents[1]

def detail_field(base: Image.Image) -> np.ndarray:
    gray_image = base.convert("L")
    gray = np.asarray(gray_image, dtype=np.float32) / 255.0
    low = np.asarray(gray_image.filter(ImageFilter.GaussianBlur(7.0)), dtype=np.float32) / 255.0
    detail = gray - low
    scale = float(np.percentile(np.abs(detail), 98.0))
    return np.zeros_like(detail) if scale < 1.0e-6 else np.clip(detail / scale, -1.0, 1.0)


def normal_map(detail: np.ndarray, strength: float) -> Image.Image:
    smooth_image = Image.fromarray(((detail + 1.0) * 127.5).astype(np.uint8), "L")
    smooth = np.asarray(smooth_image.filter(ImageFilter.GaussianBlur(0.75)), dtype=np.float32)
    smooth = smooth / 127.5 - 1.0
    dy, dx = np.gradient(smooth)
    nx = -strength * dx
    ny = -strength * dy
    nz = np.ones_like(nx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack((nx / norm, ny / norm, nz / norm), axis=-1)
    return Image.fromarray(np.clip((rgb * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8), "RGB")


def scalar_map(mean: float, variation: float, detail: np.ndarray) -> Image.Image:
    values = np.clip(mean + variation * detail, 0.0, 1.0)
    values[:, 0] = values[:, -1] = 0.5 * (values[:, 0] + values[:, -1])
    values[0, :] = values[-1, :] = 0.5 * (values[0, :] + values[-1, :])
    return Image.fromarray(np.round(values * 255.0).astype(np.uint8), "L")



MATERIALS = (Material(key='mirror_gold', source_image='mirror_gold_source.png', generated_file='exec-786ce902-eed1-43f9-a19f-96612f64a5c3.png', contrast=0.72, brightness=1.04, roughness=0.12, roughness_variation=0.01, metallic=0.72, metallic_variation=0.005, normal_strength=0.06), Material(key='graphite_structure', source_image='graphite_structure_source.png', generated_file='exec-02c8c053-41ca-4773-9022-09313f9c3242.png', contrast=0.9, brightness=1.0, roughness=0.24, roughness_variation=0.02, metallic=0.2, metallic_variation=0.015, normal_strength=0.32), Material(key='sunshield_silver', source_image='sunshield_silver_source.png', generated_file='exec-f504a324-700e-491c-98b8-389c9932e43c.png', contrast=0.65, brightness=1.05, roughness=0.16, roughness_variation=0.015, metallic=0.74, metallic_variation=0.015, normal_strength=0.08), Material(key='thermal_blanket_rose_silver', source_image='thermal_blanket_rose_silver_source.png', generated_file='exec-e972d68c-46ae-47f1-b9e4-b1827f27d3d2.png', contrast=0.68, brightness=1.04, roughness=0.09, roughness_variation=0.012, metallic=0.93, metallic_variation=0.008, normal_strength=0.07), Material(key='polished_aluminum_structure', source_image='polished_aluminum_structure_source.png', generated_file='exec-e04f519c-90ce-4990-b315-eb7318b2fc32.png', contrast=0.75, brightness=1.02, roughness=0.1, roughness_variation=0.01, metallic=0.95, metallic_variation=0.005, normal_strength=0.05), Material(key='solar_cell_blue', source_image='solar_cell_blue_source.png', generated_file='exec-060b66a3-67bd-49e5-ad25-47be577eea9f.png', contrast=0.9, brightness=1.05, roughness=0.2, roughness_variation=0.02, metallic=0.3, metallic_variation=0.015, normal_strength=0.12))

def main() -> None:
    report = {"size_px": 1024, "materials": {}}
    rows = []
    for material in MATERIALS:
        base_path = TEXTURE_DIR / f"{material.key}_base_color.png"
        with Image.open(base_path) as image:
            base = image.convert("RGB").resize((1024, 1024), Image.Resampling.LANCZOS)
        detail = detail_field(base)
        normal = normal_map(detail, material.normal_strength)
        roughness = scalar_map(material.roughness, material.roughness_variation, detail)
        metallic = scalar_map(material.metallic, material.metallic_variation, -detail)
        outputs = {
            "normal": TEXTURE_DIR / f"{material.key}_normal.png",
            "roughness": TEXTURE_DIR / f"{material.key}_roughness.png",
            "metallic": TEXTURE_DIR / f"{material.key}_metallic.png",
        }
        normal.save(outputs["normal"], optimize=True)
        roughness.save(outputs["roughness"], optimize=True)
        metallic.save(outputs["metallic"], optimize=True)
        row = Image.new("RGB", (1024, 256))
        for index, item in enumerate((base, normal, roughness.convert("RGB"), metallic.convert("RGB"))):
            row.paste(item.resize((256, 256), Image.Resampling.LANCZOS), (256 * index, 0))
        rows.append(row)
        report["materials"][material.key] = {
            "base_color": base_path.name,
            "normal": outputs["normal"].name,
            "roughness": outputs["roughness"].name,
            "metallic": outputs["metallic"].name,
            "mean_roughness": material.roughness,
            "mean_metallic": material.metallic,
        }
    contact = Image.new("RGB", (1024, 256 * len(rows)), (24, 24, 24))
    for index, row in enumerate(rows):
        contact.paste(row, (0, 256 * index))
    contact.save(TEXTURE_DIR / "jwst_pbr_contact_sheet.png", optimize=True)
    (TEXTURE_DIR / "jwst_pbr_manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
