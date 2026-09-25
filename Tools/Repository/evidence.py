"""Path and integrity checks shared by the optional-evidence tools."""
from pathlib import Path
import csv
import hashlib
import re
import sys


def long_path(path):
    path = Path(path).resolve()
    if sys.platform == 'win32' and not str(path).startswith('\\\\?\\'):
        value = str(path)
        path = Path('\\\\?\\UNC\\' + value[2:] if value.startswith('\\\\') else '\\\\?\\' + value)
    return path


def safe_path(root, relative):
    """Resolve a manifest path without permitting traversal or symlink escape."""
    if (not relative or '\\' in relative or ':' in relative
            or relative.startswith('/')
            or any(part in {'', '.', '..'} for part in relative.split('/'))):
        raise ValueError(f'Unsafe evidence path: {relative}')
    base = long_path(root)
    path = (base / relative).resolve()
    if not path.is_relative_to(base):
        raise ValueError(f'Evidence path escapes its directory: {relative}')
    return path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_manifest(path):
    records = {}
    with path.open(encoding='utf-8', newline='') as stream:
        for row in csv.DictReader(stream):
            name = row['path']
            safe_path(path.parent, name)
            if name in records:
                raise ValueError(f'Duplicate evidence path: {name}')
            size = int(row['bytes'])
            sha = row['sha256']
            if size < 0 or not re.fullmatch('[0-9a-f]{64}', sha):
                raise ValueError(f'Invalid evidence record: {name}')
            records[name] = (size, sha)
    if not records:
        raise ValueError('The evidence manifest is empty.')
    return records


def verify_files(root, records):
    errors = []
    for name, (size, sha) in records.items():
        path = safe_path(root, name)
        if not path.is_file():
            errors.append(f'Missing evidence: {name}')
        elif path.stat().st_size != size or digest(path) != sha:
            errors.append(f'Evidence differs from the recorded snapshot: {name}')
    return errors
