#!/usr/bin/env python3
"""
Dataset Downloader Module

Automated download and preparation of benchmark datasets with verification.
"""

import hashlib
import os
import subprocess
import sys
import urllib.request
from pathlib import Path
from typing import Dict, Any

# Dataset registry with official sources
DATASETS = {
    'perl': {
        'name': 'Perl 5.43.3 Source Code',
        'url': 'https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz',
        'sha256': '318651ee5bd94acb6a2d9ab925f3d43fe2192c9c691160d76b65071fad8c9acd',
        'extract_dir': 'perl-5.43.3',
        'description': 'Perl source code - small files workload (6,802 files, ~95MB)',
        'size_mb': 18
    },
    'raspios': {
        'name': 'Raspberry Pi OS Lite ARM64 Sample',
        'url': 'https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2025-10-02/2025-10-01-raspios-trixie-arm64-lite.img.xz',
        'sha256': '79146135607ffe8acac94e5ff501de6fc49583117de5ad08c45a32c73ae2a027',
        'extract_dir': 'raspios_dataset',
        'description': 'Raspberry Pi OS image - large single file workload (~2.7GB)',
        'size_mb': 476,
        'extract_first_file_only': True,
        'rename_to': 'raspios_sample.img'
    }
}


class DatasetDownloader:
    """Download and prepare benchmark datasets"""

    def __init__(self, base_dir: str = 'benchmark-files'):
        self.base_dir = Path(base_dir)
        self.base_dir.mkdir(parents=True, exist_ok=True)

    def download_dataset(self, dataset_id: str, force: bool = False) -> bool:
        """Download and prepare a dataset"""
        if dataset_id not in DATASETS:
            print(f"ERROR: Unknown dataset: {dataset_id}")
            print(f"Available: {', '.join(DATASETS.keys())}")
            return False

        config = DATASETS[dataset_id]
        extract_dir = self.base_dir / config['extract_dir']

        # Check if already exists
        if extract_dir.exists() and not force:
            print(f"✓ Dataset '{dataset_id}' already exists at {extract_dir}")
            return True

        print(f"\n📦 Downloading {config['name']}...")
        print(f"   Source: {config['url']}")
        print(f"   Size: ~{config['size_mb']} MB")

        # Download file (preserve extension for extraction)
        url_ext = Path(config['url']).suffix
        if url_ext == '.gz' and Path(config['url']).stem.endswith('.tar'):
            url_ext = '.tar.gz'
        elif url_ext == '.xz' and Path(config['url']).stem.endswith('.tar'):
            url_ext = '.tar.xz'
        elif url_ext == '.xz' and Path(config['url']).stem.endswith('.img'):
            url_ext = '.img.xz'
        
        archive_path = self.base_dir / f"{dataset_id}{url_ext}"
        try:
            self._download_with_progress(config['url'], archive_path)
        except Exception as e:
            print(f"ERROR: Download failed: {e}")
            return False

        # Verify checksum
        print(f"\n🔍 Verifying checksum...")
        if not self._verify_checksum(archive_path, config['sha256']):
            print(f"ERROR: Checksum mismatch!")
            return False
        print(f"✓ Checksum verified")

        # Extract archive
        print(f"\n📂 Extracting to {extract_dir}...")
        try:
            self._extract_archive(archive_path, extract_dir, config)
            print(f"✓ Extraction complete")
        except Exception as e:
            print(f"ERROR: Extraction failed: {e}")
            return False

        # Clean up archive
        archive_path.unlink()

        print(f"\n✅ Dataset '{dataset_id}' ready at {extract_dir}")
        return True

    def _download_with_progress(self, url: str, dest: Path):
        """Download file with progress indicator"""
        def reporthook(count, block_size, total_size):
            if total_size > 0:
                percent = int(count * block_size * 100 / total_size)
                mb_downloaded = (count * block_size) / (1024 * 1024)
                total_mb = total_size / (1024 * 1024)
                bar_len = 40
                filled = int(bar_len * percent / 100)
                bar = '█' * filled + '░' * (bar_len - filled)
                print(f'\r   [{bar}] {percent:3d}% ({mb_downloaded:.1f}/{total_mb:.1f} MB)', end='', flush=True)

        urllib.request.urlretrieve(url, dest, reporthook)
        print()  # New line after progress

    def _verify_checksum(self, file_path: Path, expected_sha256: str) -> bool:
        """Verify SHA-256 checksum"""
        sha256 = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                sha256.update(chunk)

        actual = sha256.hexdigest()
        return actual == expected_sha256

    def _extract_archive(self, archive_path: Path, extract_dir: Path, config: Dict[str, Any]):
        """Extract archive to destination"""
        extract_dir.mkdir(parents=True, exist_ok=True)

        # Determine archive type
        if str(archive_path).endswith('.tar.gz'):
            cmd = ['tar', 'xzf', str(archive_path), '-C', str(extract_dir)]
        elif str(archive_path).endswith('.tar.xz') or str(archive_path).endswith('.img.xz'):
            # For .img.xz, extract the image file
            if config.get('extract_first_file_only'):
                cmd = ['xz', '-dc', str(archive_path)]
                output_file = extract_dir / config.get('rename_to', 'extracted.img')
                with open(output_file, 'wb') as f:
                    subprocess.run(cmd, stdout=f, check=True)
                return
            else:
                cmd = ['tar', 'xJf', str(archive_path), '-C', str(extract_dir)]
        else:
            raise ValueError(f"Unsupported archive format: {archive_path}")

        subprocess.run(cmd, check=True)

    def download_all(self, force: bool = False) -> bool:
        """Download all datasets"""
        print("📚 Downloading all benchmark datasets...")
        success = True
        for dataset_id in DATASETS.keys():
            if not self.download_dataset(dataset_id, force):
                success = False
        return success

    def list_datasets(self):
        """List available datasets"""
        print("\n📊 Available Benchmark Datasets:\n")
        for dataset_id, config in DATASETS.items():
            status = "✓" if (self.base_dir / config['extract_dir']).exists() else "○"
            print(f"{status} {dataset_id}")
            print(f"   Name: {config['name']}")
            print(f"   URL: {config['url']}")
            print(f"   Size: ~{config['size_mb']} MB")
            print(f"   Description: {config['description']}")
            print()


def main():
    """CLI interface for dataset downloader"""
    import argparse

    parser = argparse.ArgumentParser(
        description='DwarFS Benchmark Dataset Downloader'
    )
    parser.add_argument('--list', action='store_true',
                       help='List available datasets')
    parser.add_argument('--download', choices=list(DATASETS.keys()) + ['all'],
                       help='Download specific dataset or all')
    parser.add_argument('--force', action='store_true',
                       help='Force re-download even if exists')
    parser.add_argument('--base-dir', default='benchmark-files',
                       help='Base directory for datasets (default: benchmark-files)')

    args = parser.parse_args()

    downloader = DatasetDownloader(args.base_dir)

    if args.list:
        downloader.list_datasets()
        return 0

    if args.download:
        if args.download == 'all':
            success = downloader.download_all(args.force)
        else:
            success = downloader.download_dataset(args.download, args.force)

        return 0 if success else 1

    parser.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())