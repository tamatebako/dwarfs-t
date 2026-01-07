#!/usr/bin/env python3
"""
Directory Tree Hash Utility

Computes a Merkle-tree style hash of a directory tree including:
- File paths (relative to root)
- File names
- File contents (SHA256)
- Directory structure

Usage:
    python3 dirtree_hash.py <directory>
    python3 dirtree_hash.py --compare <dir1> <dir2>
"""

import hashlib
import os
import sys
from pathlib import Path
from typing import Dict, List, Tuple


def sha256_file(filepath: Path) -> str:
    """Compute SHA256 hash of a file."""
    hasher = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while chunk := f.read(65536):  # 64KB chunks
            hasher.update(chunk)
    return hasher.hexdigest()


def compute_tree_hash(root_dir: Path) -> Tuple[str, Dict[str, str]]:
    """
    Compute Merkle-tree style hash of directory tree.

    Returns:
        (tree_hash, file_hashes_dict)

    The tree hash is computed from a canonical representation:
    - Sorted file paths (relative to root)
    - Each entry: <relpath>:<sha256>:<size>
    - Hash of concatenated entries
    """
    root_dir = root_dir.resolve()
    entries: List[Tuple[str, str, int]] = []
    file_hashes: Dict[str, str] = {}

    # Walk directory tree in sorted order for determinism
    for dirpath_str, dirnames, filenames in os.walk(root_dir):
        dirpath = Path(dirpath_str)

        # Sort for deterministic ordering
        dirnames.sort()
        filenames.sort()

        # Process files
        for filename in filenames:
            filepath = dirpath / filename
            relpath = filepath.relative_to(root_dir)

            try:
                # Compute file hash
                file_hash = sha256_file(filepath)
                file_size = filepath.stat().st_size

                # Store in results
                relpath_str = str(relpath)
                entries.append((relpath_str, file_hash, file_size))
                file_hashes[relpath_str] = file_hash

            except (OSError, PermissionError) as e:
                print(f"Warning: Cannot hash {relpath}: {e}", file=sys.stderr)

    # Sort entries by path for canonical ordering
    entries.sort(key=lambda x: x[0])

    # Build canonical representation
    canonical = []
    for relpath, file_hash, size in entries:
        canonical.append(f"{relpath}:{file_hash}:{size}")

    # Compute tree hash
    tree_hasher = hashlib.sha256()
    for entry in canonical:
        tree_hasher.update(entry.encode('utf-8'))
        tree_hasher.update(b'\n')

    tree_hash = tree_hasher.hexdigest()

    return tree_hash, file_hashes


def print_tree_info(root_dir: Path, tree_hash: str, file_hashes: Dict[str, str], verbose: bool = False):
    """Print tree hash information."""
    print(f"Directory: {root_dir}")
    print(f"Files:     {len(file_hashes)}")
    print(f"Tree Hash: {tree_hash}")

    if verbose:
        print("\nFile Hashes:")
        for relpath in sorted(file_hashes.keys()):
            print(f"  {relpath}: {file_hashes[relpath]}")


def compare_trees(dir1: Path, dir2: Path) -> bool:
    """Compare two directory trees."""
    print(f"Computing hash for: {dir1}")
    hash1, files1 = compute_tree_hash(dir1)

    print(f"Computing hash for: {dir2}")
    hash2, files2 = compute_tree_hash(dir2)

    print("\n" + "="*70)
    print("COMPARISON RESULTS")
    print("="*70)

    print(f"\nDirectory 1: {dir1}")
    print(f"  Files:     {len(files1)}")
    print(f"  Tree Hash: {hash1}")

    print(f"\nDirectory 2: {dir2}")
    print(f"  Files:     {len(files2)}")
    print(f"  Tree Hash: {hash2}")

    # Compare
    if hash1 == hash2:
        print("\n✅ IDENTICAL - Tree hashes match!")
        print("Both directories have:")
        print(f"  - Same file structure")
        print(f"  - Same file contents")
        print(f"  - Same file sizes")
        return True

    print("\n❌ DIFFERENT - Tree hashes DO NOT match!")

    # Find differences
    files1_set = set(files1.keys())
    files2_set = set(files2.keys())

    only_in_1 = files1_set - files2_set
    only_in_2 = files2_set - files1_set
    common = files1_set & files2_set

    if only_in_1:
        print(f"\n📁 Files only in {dir1.name}: {len(only_in_1)}")
        for path in sorted(only_in_1)[:10]:  # Show first 10
            print(f"  - {path}")
        if len(only_in_1) > 10:
            print(f"  ... and {len(only_in_1) - 10} more")

    if only_in_2:
        print(f"\n📁 Files only in {dir2.name}: {len(only_in_2)}")
        for path in sorted(only_in_2)[:10]:
            print(f"  - {path}")
        if len(only_in_2) > 10:
            print(f"  ... and {len(only_in_2) - 10} more")

    # Check for content differences in common files
    different_content = []
    for path in common:
        if files1[path] != files2[path]:
            different_content.append(path)

    if different_content:
        print(f"\n📝 Files with different content: {len(different_content)}")
        for path in sorted(different_content)[:10]:
            print(f"  - {path}")
            print(f"    Dir 1: {files1[path]}")
            print(f"    Dir 2: {files2[path]}")
        if len(different_content) > 10:
            print(f"  ... and {len(different_content) - 10} more")

    return False


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    # Parse arguments
    if sys.argv[1] == '--compare':
        if len(sys.argv) != 4:
            print("Usage: dirtree_hash.py --compare <dir1> <dir2>")
            sys.exit(1)

        dir1 = Path(sys.argv[2])
        dir2 = Path(sys.argv[3])

        if not dir1.is_dir():
            print(f"Error: {dir1} is not a directory", file=sys.stderr)
            sys.exit(1)
        if not dir2.is_dir():
            print(f"Error: {dir2} is not a directory", file=sys.stderr)
            sys.exit(1)

        identical = compare_trees(dir1, dir2)
        sys.exit(0 if identical else 1)

    else:
        # Single directory hash
        root_dir = Path(sys.argv[1])

        if not root_dir.is_dir():
            print(f"Error: {root_dir} is not a directory", file=sys.stderr)
            sys.exit(1)

        verbose = '--verbose' in sys.argv or '-v' in sys.argv

        tree_hash, file_hashes = compute_tree_hash(root_dir)
        print_tree_info(root_dir, tree_hash, file_hashes, verbose)


if __name__ == '__main__':
    main()