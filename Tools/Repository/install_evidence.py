"""Verify and restore the matching PHAROS reproducibility bundle."""
from pathlib import Path
import argparse
import shutil
from evidence import digest, long_path, read_manifest, safe_path, verify_files


def install(repository, bundle, check_only=False):
    repository = long_path(repository)
    bundle = long_path(bundle)
    expected = read_manifest(repository / 'Validation/Catalog/evidence_manifest.csv')
    supplied = read_manifest(bundle / 'MANIFEST_SHA256.csv')
    if supplied != expected:
        raise ValueError('This bundle does not match the repository evidence index.')
    errors = verify_files(bundle / 'Payload', expected)
    pending = []
    for name, (size, sha) in expected.items():
        target = safe_path(repository, name)
        if target.exists():
            if not target.is_file() or target.stat().st_size != size or digest(target) != sha:
                errors.append(f'Existing file differs; not overwritten: {name}')
        else:
            pending.append(name)
    if errors:
        raise ValueError('\n'.join(errors[:30]) + f'\n{len(errors)} check(s) failed.')
    if not check_only:
        for name in pending:
            target = safe_path(repository, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('xb') as output:
                try:
                    with safe_path(bundle / 'Payload', name).open('rb') as source:
                        shutil.copyfileobj(source, output, length=4 * 1024 * 1024)
                except BaseException:
                    output.close()
                    target.unlink()
                    raise
    return len(expected), len(pending)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    count, pending = install(repository, args.bundle, args.check_only)
    action = 'ready to install' if args.check_only else 'installed'
    print(f'Verified {count} evidence files; {pending} {action}, {count - pending} already present.')


if __name__ == '__main__':
    main()
