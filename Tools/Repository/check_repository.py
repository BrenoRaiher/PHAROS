"""Read-only checks for the source tree and optional recorded evidence."""
from pathlib import Path
import argparse
import ast
import re
import sys
import tomllib
from evidence import long_path, read_manifest, verify_files

ROOT = long_path(Path(__file__).resolve().parents[2])


def structure():
    errors = []
    count = 0
    required = ['PHAROS.uproject', 'LICENSE', 'Docs/GettingStarted/Build.md',
                'ControllerSDK/PHAROSControllerAPI.h', 'Validation/README.md',
                'Validation/Catalog/CaseMap.md', 'Docs/GettingStarted/Reproduce.md']
    for relative in required:
        if not (ROOT / relative).is_file():
            errors.append(f'Missing required file: {relative}')
    optional = read_manifest(ROOT / 'Validation/Catalog/evidence_manifest.csv')
    for path in ROOT.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT)
        if any(part in {'.git', '__pycache__', 'scratch', 'Intermediate', 'Saved', 'Binaries', 'DerivedDataCache'} for part in relative.parts):
            continue
        if relative.as_posix().startswith(('Source/ThirdParty/', 'Docs/Legal/Licenses/', 'Docs/Legal/Sources/')):
            continue
        if 'current_source' in relative.parts:
            continue
        if path.suffix == '.py':
            try:
                ast.parse(path.read_text(encoding='utf-8-sig'), filename=str(relative))
            except (SyntaxError, UnicodeError) as error:
                errors.append(f'{relative}: {error}')
        if path.suffix != '.tgscn':
            continue
        try:
            document = tomllib.loads(path.read_text(encoding='utf-8-sig'))
        except (ValueError, UnicodeError) as error:
            errors.append(f'{relative}: {error}')
            continue
        if 'templates' in relative.parts:
            continue

        def visit(value):
            if isinstance(value, dict):
                for key, item in value.items():
                    if key.endswith('_file') and isinstance(item, str) and item:
                        target = (path.parent / item).resolve()
                        if re.match(r'^[A-Za-z]:[/\\]', item) or item.startswith(('\\\\', '/')):
                            errors.append(f'Nonportable resource in {relative}: {key}')
                        elif not target.is_relative_to(ROOT):
                            errors.append(f'Resource outside repository in {relative}: {item}')
                        elif not target.is_file() and target.relative_to(ROOT).as_posix() not in optional:
                            errors.append(f'Missing resource in {relative}: {item}')
                    visit(item)
            elif isinstance(value, list):
                for item in value:
                    visit(item)
        visit(document)
        count += 1
    return errors, count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-evidence', action='store_true',
                        help='Require installed optional evidence and verify all its hashes.')
    args = parser.parse_args()
    errors, scenarios = structure()
    print(f'Checked Python syntax and resources in {scenarios} standalone scenarios.')
    if args.verify_evidence:
        expected = read_manifest(ROOT / 'Validation/Catalog/evidence_manifest.csv')
        errors += verify_files(ROOT, expected)
        print(f'Checked {len(expected)} installed evidence hashes.')
    for error in errors[:80]:
        print(error, file=sys.stderr)
    if errors:
        raise SystemExit(f'{len(errors)} repository check(s) failed.')
    print('Repository checks passed.')


if __name__ == '__main__':
    main()
