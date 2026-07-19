#!/usr/bin/env python3
"""
DwarFS Dataset Manager for Comprehensive Benchmarking

Manages datasets and creates test images in multiple formats:
- Downloads real-world datasets (Linux kernel, etc.)
- Validates dataset integrity
- Creates DwarFS images in FlatBuffers and Thrift formats
- Tracks dataset metadata
"""

import hashlib
import subprocess
import tarfile
import urllib.request
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import json


@dataclass
class DatasetInfo:
    """Metadata about a dataset"""
    name: str
    slug: str
    description: str
    url: Optional[str]
    sha256: Optional[str]
    size_mb: int
    file_count: int
    source_type: str  # 'download', 'local', 'repository'


class DatasetManager:
    """
    Manages benchmark datasets
    
    Single Responsibility: Dataset preparation and image creation
    """
    
    # Dataset definitions with real-world data
    DATASETS = {
        'linux-kernel': DatasetInfo(
            name='Linux Kernel 6.6',
            slug='linux-kernel',
            description='Linux kernel source code - typical developer workload',
            url='https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz',
            sha256='d926a06c63dd8ac7df3f86ee1ffc2ce2a3b81a2d168484e76b5b389aba8e56d0',
            size_mb=1100,
            file_count=70000,
            source_type='download',
        ),
        'dwarfs-source': DatasetInfo(
            name='DwarFS Source Code',
            slug='dwarfs-source',
            description='Current DwarFS repository - real C++ project',
            url=None,  # Local repository
            sha256=None,
            size_mb=15,
            file_count=2000,
            source_type='repository',
        ),
        'perl-installs': DatasetInfo(
            name='Perl Installations',
            slug='perl-installs',
            description='Multiple Perl installations - extreme deduplication',
            url=None,  # May be in benchmark_data/
            sha256=None,
            size_mb=47000,
            file_count=1139000,
            source_type='local',
        ),
    }
    
    def __init__(self, workspace_dir: Path, cache_dir: Optional[Path] = None):
        """
        Initialize dataset manager
        
        Args:
            workspace_dir: DwarFS workspace root
            cache_dir: Directory to cache downloads (default: workspace/benchmark-data)
        """
        self.workspace_dir = Path(workspace_dir).resolve()
        self.cache_dir = cache_dir or (self.workspace_dir / 'benchmark-data')
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        
        # Directory for prepared datasets
        self.datasets_dir = self.cache_dir / 'datasets'
        self.datasets_dir.mkdir(parents=True, exist_ok=True)
        
        # Directory for test images
        self.images_dir = self.cache_dir / 'images'
        self.images_dir.mkdir(parents=True, exist_ok=True)
    
    def prepare_dataset(self, dataset_name: str, force: bool = False) -> Optional[Path]:
        """
        Prepare a dataset for benchmarking
        
        Args:
            dataset_name: Dataset identifier
            force: Force re-download/re-prepare
            
        Returns:
            Path to prepared dataset directory
        """
        if dataset_name not in self.DATASETS:
            print(f"ERROR: Unknown dataset '{dataset_name}'")
            print(f"Available: {', '.join(self.DATASETS.keys())}")
            return None
        
        dataset = self.DATASETS[dataset_name]
        dataset_path = self.datasets_dir / dataset.slug
        
        # Check if already prepared
        if dataset_path.exists() and not force:
            print(f"✓ Dataset '{dataset_name}' already prepared: {dataset_path}")
            return dataset_path
        
        print(f"\n{'='*70}")
        print(f"Preparing dataset: {dataset.name}")
        print(f"{'='*70}\n")
        
        if dataset.source_type == 'download':
            return self._download_and_extract(dataset, dataset_path, force)
        elif dataset.source_type == 'repository':
            return self._prepare_repository(dataset, dataset_path)
        elif dataset.source_type == 'local':
            return self._find_local_dataset(dataset, dataset_path)
        
        return None
    
    def _download_and_extract(self, dataset: DatasetInfo, 
                              dest_path: Path, force: bool) -> Optional[Path]:
        """Download and extract a dataset"""
        if not dataset.url:
            print(f"ERROR: No URL for dataset {dataset.slug}")
            return None
        
        # Download to cache
        archive_name = Path(dataset.url).name
        archive_path = self.cache_dir / archive_name
        
        if not archive_path.exists() or force:
            print(f"Downloading: {dataset.url}")
            print(f"Size: ~{dataset.size_mb} MB")
            
            try:
                self._download_with_progress(dataset.url, archive_path)
            except Exception as e:
                print(f"ERROR: Download failed: {e}")
                return None
            
            # Verify checksum
            if dataset.sha256:
                print("\nVerifying checksum...")
                if not self._verify_checksum(archive_path, dataset.sha256):
                    print("ERROR: Checksum mismatch!")
                    archive_path.unlink()
                    return None
                print("✓ Checksum verified")
        else:
            print(f"✓ Using cached archive: {archive_path}")
        
        # Extract
        print(f"\nExtracting to: {dest_path}")
        dest_path.mkdir(parents=True, exist_ok=True)
        
        try:
            if archive_path.suffix in ['.xz', '.gz', '.bz2']:
                # Assume tar archive
                subprocess.run(
                    ['tar', 'xf', str(archive_path), '-C', str(dest_path), 
                     '--strip-components=1'],
                    check=True,
                    capture_output=True
                )
            else:
                print(f"ERROR: Unsupported archive format: {archive_path}")
                return None
            
            print(f"✓ Extraction complete")
            
        except Exception as e:
            print(f"ERROR: Extraction failed: {e}")
            return None
        
        return dest_path
    
    def _prepare_repository(self, dataset: DatasetInfo, dest_path: Path) -> Optional[Path]:
        """Prepare dataset from current repository"""
        # For dwarfs-source, just link to the workspace
        if dataset.slug == 'dwarfs-source':
            # Create a subset of the repository (exclude build dirs, etc.)
            print(f"Preparing repository dataset...")
            
            # We'll just use the workspace directly for now
            # In a real scenario, we might want to create a clean copy
            if self.workspace_dir.exists():
                print(f"✓ Using repository at: {self.workspace_dir}")
                return self.workspace_dir
        
        print(f"ERROR: Cannot prepare repository dataset {dataset.slug}")
        return None
    
    def _find_local_dataset(self, dataset: DatasetInfo, dest_path: Path) -> Optional[Path]:
        """Find local dataset (e.g., perl-installs)"""
        # Look in several locations
        search_paths = [
            self.cache_dir / 'perl.tar',
            self.cache_dir / 'perl',
            self.workspace_dir / 'benchmark_data' / 'perl.tar',
            self.workspace_dir / 'benchmark_data' / 'perl',
        ]
        
        for path in search_paths:
            if path.exists():
                print(f"✓ Found local dataset: {path}")
                
                # If it's an archive, extract it
                if path.suffix == '.tar':
                    dest_path.mkdir(parents=True, exist_ok=True)
                    print(f"Extracting to: {dest_path}")
                    subprocess.run(
                        ['tar', 'xf', str(path), '-C', str(dest_path)],
                        check=True
                    )
                    return dest_path
                else:
                    # Already extracted
                    return path
        
        print(f"⚠ Dataset '{dataset.slug}' not found locally")
        print(f"  Searched: {[str(p) for p in search_paths]}")
        print(f"  This dataset must be provided manually")
        return None
    
    def _download_with_progress(self, url: str, dest: Path):
        """Download file with progress indicator"""
        def reporthook(count, block_size, total_size):
            if total_size > 0:
                percent = int(count * block_size * 100 / total_size)
                mb_downloaded = (count * block_size) / (1024 * 1024)
                total_mb = total_size / (1024 * 1024)
                bar_len = 50
                filled = int(bar_len * percent / 100)
                bar = '█' * filled + '░' * (bar_len - filled)
                print(f'\r[{bar}] {percent:3d}% ({mb_downloaded:.1f}/{total_mb:.1f} MB)', 
                      end='', flush=True)
        
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
    
    def create_images(self,
                     dataset_name: str,
                     mkdwarfs_path: Path,
                     formats: List[str],
                     compression: str = 'zstd:level=9',
                     block_size_bits: int = 22,
                     force: bool = False) -> Dict[str, Path]:
        """
        Create DwarFS images in multiple formats
        
        Args:
            dataset_name: Dataset to create images from
            mkdwarfs_path: Path to mkdwarfs binary
            formats: List of formats ('flatbuffers', 'thrift')
            compression: Compression algorithm
            block_size_bits: Block size in bits (10-30, default 22 = 4MB)
            force: Force recreation
            
        Returns:
            Dictionary mapping format to image path
        """
        # Prepare dataset first
        dataset_path = self.prepare_dataset(dataset_name)
        if not dataset_path:
            return {}
        
        images = {}
        
        for fmt in formats:
            image_name = f"{dataset_name}_{fmt}.dwarfs"
            image_path = self.images_dir / image_name
            
            # Check if already exists
            if image_path.exists() and not force:
                print(f"✓ Image already exists: {image_path}")
                images[fmt] = image_path
                continue
            
            print(f"\n{'='*70}")
            print(f"Creating {fmt} image for {dataset_name}")
            print(f"{'='*70}\n")
            
            # Map format to --format option
            format_arg = 'flatbuffers' if fmt == 'flatbuffers' else 'thrift'
            
            # Build command
            cmd = [
                str(mkdwarfs_path),
                '-i', str(dataset_path),
                '-o', str(image_path),
                '--format', format_arg,
                '--compression', compression,
                '--block-size-bits', str(block_size_bits),
            ]
            
            print(f"Command: {' '.join(cmd)}\n")
            
            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    check=True
                )
                
                # Show output
                if result.stdout:
                    print(result.stdout)
                
                # Get image size
                size_mb = image_path.stat().st_size / 1024 / 1024
                print(f"\n✓ Created {fmt} image: {image_path}")
                print(f"  Size: {size_mb:.2f} MB")
                
                images[fmt] = image_path
                
            except subprocess.CalledProcessError as e:
                print(f"ERROR: Failed to create {fmt} image")
                print(f"stdout: {e.stdout}")
                print(f"stderr: {e.stderr}")
        
        return images
    
    def validate_dataset(self, dataset_path: Path) -> bool:
        """
        Verify dataset integrity
        
        Args:
            dataset_path: Path to dataset
            
        Returns:
            True if valid
        """
        if not dataset_path.exists():
            print(f"ERROR: Dataset not found: {dataset_path}")
            return False
        
        if not dataset_path.is_dir():
            print(f"ERROR: Dataset is not a directory: {dataset_path}")
            return False
        
        # Count files
        file_count = sum(1 for _ in dataset_path.rglob('*') if _.is_file())
        total_size = sum(f.stat().st_size for f in dataset_path.rglob('*') if f.is_file())
        
        print(f"Dataset validation:")
        print(f"  Path: {dataset_path}")
        print(f"  Files: {file_count:,}")
        print(f"  Size: {total_size / 1024 / 1024:.2f} MB")
        
        return file_count > 0
    
    def list_datasets(self) -> None:
        """List available datasets"""
        print("\nAvailable Datasets:")
        print(f"{'='*70}\n")
        
        for name, dataset in self.DATASETS.items():
            dataset_path = self.datasets_dir / dataset.slug
            
            # Check status
            if dataset_path.exists():
                status = "✓ Ready"
            elif dataset.source_type == 'repository':
                status = "✓ Available (repo)"
            elif dataset.source_type == 'local':
                status = "⚠ Not found"
            else:
                status = "○ Not downloaded"
            
            print(f"{status} {dataset.name} ({dataset.slug})")
            print(f"  Type: {dataset.source_type}")
            print(f"  Description: {dataset.description}")
            print(f"  Size: ~{dataset.size_mb} MB, ~{dataset.file_count:,} files")
            if dataset.url:
                print(f"  URL: {dataset.url}")
            print()
    
    def get_dataset_info(self, dataset_name: str) -> Optional[DatasetInfo]:
        """Get metadata about a dataset"""
        return self.DATASETS.get(dataset_name)


def main():
    """CLI interface for dataset manager"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='DwarFS Dataset Manager for Comprehensive Benchmarking')
    parser.add_argument('--workspace', type=Path, default=Path.cwd(),
                       help='DwarFS workspace directory')
    parser.add_argument('--list', action='store_true',
                       help='List available datasets')
    parser.add_argument('--prepare', choices=list(DatasetManager.DATASETS.keys()),
                       help='Prepare specific dataset')
    parser.add_argument('--create-images', choices=list(DatasetManager.DATASETS.keys()),
                       help='Create test images for dataset')
    parser.add_argument('--formats', nargs='+', 
                       choices=['flatbuffers', 'thrift'],
                       default=['flatbuffers', 'thrift'],
                       help='Image formats to create')
    parser.add_argument('--mkdwarfs', type=Path,
                       help='Path to mkdwarfs binary (required for --create-images)')
    parser.add_argument('--force', action='store_true',
                       help='Force re-prepare/re-create')
    parser.add_argument('--validate', type=Path,
                       help='Validate dataset at path')
    
    args = parser.parse_args()
    
    manager = DatasetManager(args.workspace)
    
    if args.list:
        manager.list_datasets()
        return 0
    
    if args.validate:
        if manager.validate_dataset(args.validate):
            print("✓ Dataset is valid")
            return 0
        else:
            print("✗ Dataset validation failed")
            return 1
    
    if args.prepare:
        result = manager.prepare_dataset(args.prepare, force=args.force)
        if result:
            print(f"\n✓ Dataset ready: {result}")
            return 0
        else:
            print(f"\n✗ Failed to prepare dataset")
            return 1
    
    if args.create_images:
        if not args.mkdwarfs:
            print("ERROR: --mkdwarfs required for --create-images")
            return 1
        
        images = manager.create_images(
            args.create_images,
            args.mkdwarfs,
            args.formats,
            force=args.force
        )
        
        if images:
            print(f"\n✓ Created {len(images)} image(s):")
            for fmt, path in images.items():
                size_mb = path.stat().st_size / 1024 / 1024
                print(f"  {fmt}: {path} ({size_mb:.2f} MB)")
            return 0
        else:
            print("\n✗ Failed to create images")
            return 1
    
    parser.print_help()
    return 0


if __name__ == '__main__':
    exit(main())