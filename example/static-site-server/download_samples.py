#!/usr/bin/env python3
"""
Sample Downloader for DwarFS Static Site Server Example

Downloads Project Gutenberg HTML files and builds DwarFS images for testing.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, Any, Optional

# Script location (example/static-site-server/)
SCRIPT_DIR = Path(__file__).parent.resolve()
# Base directory (example/)
BASE_DIR = SCRIPT_DIR.parent

# Sample registry with Project Gutenberg sources
SAMPLES = {
    'aesop': {
        'name': "Aesop's Fables (Project Gutenberg #11339)",
        'url': 'https://www.gutenberg.org/cache/epub/11339/pg11339-h.zip',
        'sha256': '62e61e6bee82901b6e0ed7fd1cb5f117f93edbf38e3e8feeb49875a87f6348f2',
        'extract_dir': 'pg11339-h',
        'output_image': 'aesop.dff',
        'description': "Aesop's Fables with illustrations - 116 images + HTML",
        'size_mb': 4
    },
    'candide': {
        'name': 'Candide (Project Gutenberg #19942)',
        'url': 'https://www.gutenberg.org/cache/epub/19942/pg19942-h.zip',
        'sha256': '8034ed9604417b477a70e8ec77fec913b3fe52f4a6583d0dd23a47979ab35c78',
        'extract_dir': 'pg19942-h',
        'output_image': 'candide.dff',
        'description': "Voltaire's Candide with illustrations - 4 images + HTML",
        'size_mb': 0.3
    }
}


class SampleDownloader:
    """Download Project Gutenberg samples and build DwarFS images"""

    def __init__(self, mkdwarfs_path: Optional[str] = None):
        self.mkdwarfs_path = self._find_mkdwarfs(mkdwarfs_path)

    def _find_mkdwarfs(self, explicit_path: Optional[str] = None) -> Optional[str]:
        """Find mkdwarfs executable that actually works (no missing libraries)."""
        candidates = []

        # 1. Explicitly specified path (from CLI)
        if explicit_path:
            candidates.append(Path(explicit_path))

        # 2. DWARFS_BUILD_DIR environment variable
        if env_dir := os.environ.get('DWARFS_BUILD_DIR'):
            candidates.append(Path(env_dir) / 'mkdwarfs')

        # 3. Production builds (prefer - no FUSE dependency issues)
        for build_name in ['build-arm64-osx-production', 'build-x64-osx-production']:
            candidates.append(SCRIPT_DIR.parent.parent / build_name / 'mkdwarfs')

        # 4. Generic build directory
        candidates.append(SCRIPT_DIR.parent.parent / 'build' / 'mkdwarfs')

        # 5. Other build directories (may have FUSE dependency issues)
        for build_name in ['build-both-formats', 'build-flatbuffers-only']:
            candidates.append(SCRIPT_DIR.parent.parent / build_name / 'mkdwarfs')

        # 6. Homebrew on macOS
        if sys.platform == 'darwin':
            candidates.extend([
                Path('/opt/homebrew/bin/mkdwarfs'),
                Path('/usr/local/bin/mkdwarfs'),
            ])

        # 7. System PATH
        if path_mk := shutil.which('mkdwarfs'):
            candidates.append(Path(path_mk))

        # Return first found that can actually execute (verify with --version)
        # Set up environment for macOS FUSE-T library
        env = os.environ.copy()
        if sys.platform == 'darwin':
            # Add common FUSE-T library paths (prefer /usr/local/lib first)
            fuse_paths = ['/usr/local/lib', '/opt/homebrew/lib']
            dyld_parts = []
            for path in fuse_paths:
                if Path(path).exists():
                    libfuse = Path(path) / 'libfuse-t.dylib'
                    if libfuse.exists():
                        dyld_parts.append(path)
            if dyld_parts:
                env['DYLD_LIBRARY_PATH'] = ':'.join(dyld_parts)

        for candidate in candidates:
            if candidate.exists() and os.access(candidate, os.X_OK):
                try:
                    result = subprocess.run(
                        [str(candidate), '--version'],
                        capture_output=True,
                        text=True,
                        timeout=5,
                        env=env
                    )
                    # Check if output contains "mkdwarfs" (some builds return exit 1)
                    if 'mkdwarfs' in result.stdout.lower() or 'mkdwarfs' in result.stderr.lower():
                        return str(candidate)
                except (subprocess.TimeoutExpired, OSError):
                    pass

        return None

    def download_sample(self, sample_id: str, force: bool = False) -> bool:
        """Download and extract a sample"""
        if sample_id not in SAMPLES:
            print(f"ERROR: Unknown sample: {sample_id}")
            print(f"Available: {', '.join(SAMPLES.keys())}")
            return False

        config = SAMPLES[sample_id]
        extract_dir = BASE_DIR / config['extract_dir']

        # Check if already exists
        if extract_dir.exists() and not force:
            print(f"✓ Sample '{sample_id}' already exists at {extract_dir}")
            return True

        print(f"\n📦 Downloading {config['name']}...")
        print(f"   Source: {config['url']}")
        print(f"   Size: ~{config['size_mb']} MB")

        # Download file
        archive_path = BASE_DIR / f"{sample_id}.zip"
        try:
            self._download_with_progress(config['url'], archive_path)
        except Exception as e:
            print(f"ERROR: Download failed: {e}")
            return False

        # Verify checksum
        print(f"\n🔍 Verifying checksum...")
        if not self._verify_checksum(archive_path, config['sha256']):
            print(f"ERROR: Checksum mismatch!")
            archive_path.unlink()
            return False
        print(f"✓ Checksum verified")

        # Extract ZIP
        print(f"\n📂 Extracting to {extract_dir}...")
        try:
            self._extract_zip(archive_path, BASE_DIR)
            print(f"✓ Extraction complete")
        except Exception as e:
            print(f"ERROR: Extraction failed: {e}")
            return False
        finally:
            # Clean up archive
            if archive_path.exists():
                archive_path.unlink()

        print(f"\n✅ Sample '{sample_id}' ready at {extract_dir}")
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

    def _extract_zip(self, archive_path: Path, dest_dir: Path):
        """Extract ZIP archive to destination"""
        with zipfile.ZipFile(archive_path, 'r') as zf:
            zf.extractall(dest_dir)

    def build_image(self, sample_id: str, force: bool = False) -> bool:
        """Build DwarFS image from sample"""
        if sample_id not in SAMPLES:
            print(f"ERROR: Unknown sample: {sample_id}")
            return False

        config = SAMPLES[sample_id]
        source_dir = BASE_DIR / config['extract_dir']
        output_image = SCRIPT_DIR / config['output_image']

        # Check source exists
        if not source_dir.exists():
            print(f"ERROR: Source directory not found: {source_dir}")
            print(f"Run './download_samples.py --download {sample_id}' first")
            return False

        # Check output exists
        if output_image.exists() and not force:
            print(f"✓ Image already exists: {output_image}")
            return True

        # Find mkdwarfs
        mkdwarfs = self.mkdwarfs_path or self._find_mkdwarfs()
        if not mkdwarfs:
            print("ERROR: mkdwarfs not found")
            print("\nTo build DwarFS images, mkdwarfs must be available:")
            print("  1. Set DWARFS_BUILD_DIR environment variable")
            print("  2. Build DwarFS in project build/ directory")
            print("  3. Install via Homebrew (macOS): brew install dwarfs")
            print("  4. Use --mkdwarfs option to specify path")
            return False

        print(f"\n🔨 Building DwarFS image...")
        print(f"   mkdwarfs: {mkdwarfs}")
        print(f"   Source: {source_dir}")
        print(f"   Output: {output_image}")

        # Build command
        cmd = [
            mkdwarfs,
            '-i', str(source_dir),
            '-o', str(output_image),
            '-l0'  # No compression for quick testing
        ]

        if force and output_image.exists():
            cmd.append('--force')

        # Set up environment for macOS FUSE-T library
        env = os.environ.copy()
        if sys.platform == 'darwin':
            # Add common FUSE-T library paths (prefer /usr/local/lib first)
            fuse_paths = ['/usr/local/lib', '/opt/homebrew/lib']
            dyld_parts = []
            for path in fuse_paths:
                if Path(path).exists():
                    libfuse = Path(path) / 'libfuse-t.dylib'
                    if libfuse.exists():
                        dyld_parts.append(path)
            if dyld_parts:
                env['DYLD_LIBRARY_PATH'] = ':'.join(dyld_parts)

        result = subprocess.run(cmd, capture_output=True, text=True, env=env)

        if result.returncode != 0:
            error_msg = result.stderr
            # Check for macOS FUSE-T library loading issue
            if 'libfuse-t.dylib' in error_msg and sys.platform == 'darwin':
                print("ERROR: Build failed: FUSE-T library not found")
                print("\nmacOS System Integrity Protection (SIP) may prevent library loading.")
                print("Run this command instead:")
                print(f"  DYLD_LIBRARY_PATH=/usr/local/lib {sys.argv[0]} --build-only {sample_id} --force")
                return False
            print(f"ERROR: Build failed: {error_msg}")
            return False

        # Get output size
        size_mb = output_image.stat().st_size / (1024 * 1024)
        print(f"✓ Image built: {output_image.name} ({size_mb:.2f} MB)")

        return True

    def download_all(self, force: bool = False) -> bool:
        """Download all samples"""
        print("📚 Downloading all samples...")
        success = True
        for sample_id in SAMPLES.keys():
            if not self.download_sample(sample_id, force):
                success = False
        return success

    def setup_all(self, force: bool = False) -> bool:
        """Download and build all samples"""
        print("🚀 Setting up all samples...")
        success = True
        for sample_id in SAMPLES.keys():
            if not self.download_sample(sample_id, force):
                success = False
                continue
            if not self.build_image(sample_id, force):
                success = False
        return success

    def list_samples(self):
        """List available samples with status"""
        print("\n📊 Available Project Gutenberg Samples:\n")
        for sample_id, config in SAMPLES.items():
            source_dir = BASE_DIR / config['extract_dir']
            output_image = SCRIPT_DIR / config['output_image']

            # Status indicators
            source_status = "✓ DOWNLOADED" if source_dir.exists() else "○ NOT DOWNLOADED"
            image_status = ""
            if output_image.exists():
                size_mb = output_image.stat().st_size / (1024 * 1024)
                image_status = f" (BUILT, {size_mb:.2f} MB)"
            else:
                image_status = " (not built)"

            print(f"  {sample_id} [{source_status}]{image_status}")
            print(f"    Name: {config['name']}")
            print(f"    Description: {config['description']}")
            print(f"    URL: {config['url']}")
            print()

        # Show mkdwarfs status
        mkdwarfs = self.mkdwarfs_path or self._find_mkdwarfs()
        if mkdwarfs:
            print(f"  mkdwarfs: {mkdwarfs}")
        else:
            print("  mkdwarfs: NOT FOUND (required for --setup)")


def main():
    """CLI interface for sample downloader"""
    import argparse

    parser = argparse.ArgumentParser(
        description='DwarFS Static Site Server Sample Downloader',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --list                      # List available samples
  %(prog)s --download aesop            # Download Aesop's Fables
  %(prog)s --download all              # Download all samples
  %(prog)s --setup candide             # Download and build Candide image
  %(prog)s --setup all                 # Download and build all
  %(prog)s --build-only aesop          # Build image (skip download)
"""
    )
    parser.add_argument('--list', action='store_true',
                       help='List available samples with status')
    parser.add_argument('--download', choices=list(SAMPLES.keys()) + ['all'],
                       help='Download specific sample or all')
    parser.add_argument('--setup', choices=list(SAMPLES.keys()) + ['all'],
                       help='Download and build image for sample(s)')
    parser.add_argument('--build-only', choices=list(SAMPLES.keys()) + ['all'],
                       help='Build image only (assumes source already downloaded)')
    parser.add_argument('--force', action='store_true',
                       help='Force re-download/rebuild even if exists')
    parser.add_argument('--mkdwarfs', type=str,
                       help='Path to mkdwarfs executable (auto-detect if not specified)')

    args = parser.parse_args()

    downloader = SampleDownloader(mkdwarfs_path=args.mkdwarfs)

    if args.list:
        downloader.list_samples()
        return 0

    if args.download:
        if args.download == 'all':
            success = downloader.download_all(args.force)
        else:
            success = downloader.download_sample(args.download, args.force)
        return 0 if success else 1

    if args.setup:
        if args.setup == 'all':
            success = downloader.setup_all(args.force)
        else:
            success = downloader.download_sample(args.setup, args.force)
            if success:
                success = downloader.build_image(args.setup, args.force)
        if success:
            print("\n" + "="*50)
            print("Setup complete! Run the server with:")
            if args.setup == 'all':
                print("  ./build/static-site-server --image candide.dff")
                print("  ./build/static-site-server --image aesop.dff")
            else:
                print(f"  ./build/static-site-server --image {SAMPLES[args.setup]['output_image']}")
        return 0 if success else 1

    if args.build_only:
        if args.build_only == 'all':
            success = True
            for sample_id in SAMPLES.keys():
                if not downloader.build_image(sample_id, args.force):
                    success = False
        else:
            success = downloader.build_image(args.build_only, args.force)
        return 0 if success else 1

    parser.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
