#!/usr/bin/env python3
"""
Dataset Preparation Script

Single Responsibility: Prepare disk image for benchmarking
- Mount disk image (if needed)
- Extract to directory for DwarFS consumption
- Handle cleanup
"""

import argparse
import subprocess
import tempfile
import shutil
from pathlib import Path


class DatasetPreparer:
    """
    Single Responsibility: Prepare datasets for benchmarking

    Handles disk images, tarballs, directories
    """

    def __init__(self, image_path: Path, output_dir: Path):
        self.image_path = image_path
        self.output_dir = output_dir

    def prepare_disk_image(self):
        """Prepare disk image dataset"""

        print(f"Preparing disk image: {self.image_path}")

        # Create temporary mount point
        with tempfile.TemporaryDirectory() as mount_point:
            print(f"Mounting at: {mount_point}")

            # Mount disk image (loop device, read-only)
            mount_cmd = [
                'sudo', 'mount',
                '-o', 'loop,ro',
                str(self.image_path),
                mount_point
            ]

            try:
                subprocess.run(mount_cmd, check=True)

                # Copy files to output directory
                print(f"Extracting to: {self.output_dir}")
                self.output_dir.mkdir(parents=True, exist_ok=True)

                # Use rsync for efficient copy
                rsync_cmd = [
                    'rsync',
                    '-a',  # archive mode
                    '--exclude', 'lost+found',  # skip this
                    f'{mount_point}/',
                    f'{self.output_dir}/'
                ]
                subprocess.run(rsync_cmd, check=True)

                print(f"Extraction complete: {self.output_dir}")

            finally:
                # Always unmount
                subprocess.run(['sudo', 'umount', mount_point],
                             check=False)

    def prepare(self):
        """Auto-detect and prepare dataset"""

        if self.image_path.suffix == '.img':
            self.prepare_disk_image()
        else:
            raise ValueError(f"Unsupported format: {self.image_path.suffix}")


def main():
    parser = argparse.ArgumentParser(
        description='Prepare benchmark dataset')
    parser.add_argument('--image', required=True, type=Path,
                       help='Path to disk image')
    parser.add_argument('--output', required=True, type=Path,
                       help='Output directory for extracted files')
    args = parser.parse_args()

    preparer = DatasetPreparer(args.image, args.output)
    preparer.prepare()

    print("Dataset preparation complete!")


if __name__ == '__main__':
    main()