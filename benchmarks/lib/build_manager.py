#!/usr/bin/env python3
"""
DwarFS Build Configuration Manager

Manages multiple build configurations for comprehensive benchmarking:
- fb-only: FlatBuffers only (Thrift disabled) - VALID
- thrift-only: Thrift only (FlatBuffers disabled) - VALID
- both: Both formats enabled - VALID

Both FlatBuffers and Thrift are OPTIONAL and INDEPENDENT.
At least one format must be enabled.
"""

import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass


@dataclass
class BuildConfig:
    """Configuration for a single build variant"""
    name: str
    slug: str
    cmake_args: List[str]
    build_dir: str
    can_read: List[str]  # Which formats this build can read
    can_write: List[str]  # Which formats this build can write
    expected_to_fail: bool = False  # Whether build is expected to fail


class BuildManager:
    """
    Manages multiple DwarFS build configurations

    Single Responsibility: Build and validate different format configurations
    """

    # Build configuration definitions
    CONFIGS = {
        'fb-only': BuildConfig(
            name='FlatBuffers Only',
            slug='fb-only',
            cmake_args=[
                '-DDWARFS_WITH_FLATBUFFERS=ON',
                '-DDWARFS_WITH_THRIFT=OFF',
            ],
            build_dir='build-fb-bench',
            can_read=['flatbuffers'],
            can_write=['flatbuffers'],
            expected_to_fail=False,
        ),
        'thrift-only': BuildConfig(
            name='Thrift Only',
            slug='thrift-only',
            cmake_args=[
                '-DDWARFS_WITH_FLATBUFFERS=OFF',
                '-DDWARFS_WITH_THRIFT=ON',
            ],
            build_dir='build-thrift-bench',
            can_read=['thrift'],
            can_write=['thrift'],
            expected_to_fail=False,  # Thrift-only is a VALID configuration
        ),
        'both': BuildConfig(
            name='Both Formats',
            slug='both',
            cmake_args=[
                '-DDWARFS_WITH_FLATBUFFERS=ON',
                '-DDWARFS_WITH_THRIFT=ON',
            ],
            build_dir='build-both-bench',
            can_read=['flatbuffers', 'thrift'],
            can_write=['flatbuffers', 'thrift'],
            expected_to_fail=False,
        ),
    }

    def __init__(self, workspace_dir: Path):
        """
        Initialize build manager

        Args:
            workspace_dir: Root directory of DwarFS workspace
        """
        self.workspace_dir = Path(workspace_dir).resolve()
        self.built_configs: Dict[str, Path] = {}

    def build_config(self, config_name: str,
                     ninja: bool = True,
                     jobs: Optional[int] = None,
                     build_type: str = 'Release',
                     verbose: bool = False) -> Optional[Path]:
        """
        Build a specific configuration

        Args:
            config_name: One of 'fb-only', 'thrift-only', 'both'
            ninja: Use Ninja generator (faster)
            jobs: Number of parallel jobs (default: auto)
            build_type: CMake build type
            verbose: Show build output

        Returns:
            Path to build directory on success, None on failure
        """
        if config_name not in self.CONFIGS:
            print(f"ERROR: Unknown config '{config_name}'")
            print(f"Available: {', '.join(self.CONFIGS.keys())}")
            return None

        config = self.CONFIGS[config_name]
        build_dir = self.workspace_dir / config.build_dir

        print(f"\n{'='*70}")
        print(f"Building: {config.name} ({config.slug})")
        print(f"Directory: {build_dir}")
        print(f"{'='*70}\n")

        # Prepare CMake arguments
        cmake_args = [
            'cmake',
            '-S', str(self.workspace_dir),
            '-B', str(build_dir),
            f'-DCMAKE_BUILD_TYPE={build_type}',
            '-DWITH_TESTS=OFF',  # Skip tests for benchmark builds
            '-DWITH_BENCHMARKS=OFF',
        ]

        if ninja:
            cmake_args.append('-GNinja')

        # Add format-specific args
        cmake_args.extend(config.cmake_args)

        print("CMake configuration:")
        print(f"  {' '.join(cmake_args)}\n")

        # Run CMake
        try:
            result = subprocess.run(
                cmake_args,
                capture_output=not verbose,
                text=True,
                check=False
            )

            if result.returncode != 0:
                print(f"ERROR: CMake configuration failed")
                if not verbose:
                    print(f"stdout: {result.stdout}")
                    print(f"stderr: {result.stderr}")
                return None

        except Exception as e:
            print(f"ERROR: CMake failed: {e}")
            return None

        print("✓ CMake configuration successful\n")

        # Build
        build_cmd = ['cmake', '--build', str(build_dir)]
        if jobs:
            build_cmd.extend(['--parallel', str(jobs)])
        elif ninja:
            # Ninja auto-detects parallelism
            pass
        else:
            # Make default
            import os
            cpus = os.cpu_count() or 4
            build_cmd.extend(['--parallel', str(cpus)])

        print(f"Building with: {' '.join(build_cmd)}\n")

        try:
            result = subprocess.run(
                build_cmd,
                capture_output=not verbose,
                text=True,
                check=False
            )

            if result.returncode != 0:
                print(f"ERROR: Build failed")
                if not verbose:
                    print(f"stdout: {result.stdout[-500:]}")  # Last 500 chars
                    print(f"stderr: {result.stderr[-500:]}")
                return None

        except Exception as e:
            print(f"ERROR: Build failed: {e}")
            return None

        print(f"✓ Build successful: {build_dir}\n")

        # Verify tools exist
        if not self.verify_config(config_name, verbose=verbose):
            print(f"ERROR: Build verification failed")
            return None

        self.built_configs[config_name] = build_dir
        return build_dir

    def get_tool_path(self, config_name: str, tool_name: str) -> Optional[Path]:
        """
        Get path to a built tool

        Args:
            config_name: Build configuration name
            tool_name: Tool name (mkdwarfs, dwarfsextract, dwarfs, dwarfsck)

        Returns:
            Path to tool binary or None if not found
        """
        if config_name not in self.CONFIGS:
            return None

        config = self.CONFIGS[config_name]
        build_dir = self.workspace_dir / config.build_dir

        # Check if build exists
        if not build_dir.exists():
            return None

        # Tool might be dwarfs-bin on some systems
        tool_paths = [
            build_dir / tool_name,
            build_dir / f"{tool_name}-bin",
        ]

        for path in tool_paths:
            if path.exists() and path.is_file():
                return path

        return None

    def verify_config(self, config_name: str, verbose: bool = False) -> bool:
        """
        Verify that a build configuration works

        Args:
            config_name: Configuration to verify
            verbose: Show output

        Returns:
            True if all tools exist and execute
        """
        if config_name not in self.CONFIGS:
            return False

        config = self.CONFIGS[config_name]
        build_dir = self.workspace_dir / config.build_dir

        if not build_dir.exists():
            print(f"Build directory not found: {build_dir}")
            return False

        print(f"Verifying build: {config.name}")

        # Required tools
        required_tools = ['mkdwarfs', 'dwarfsextract', 'dwarfsck']
        optional_tools = ['dwarfs']  # FUSE driver may not be available

        all_ok = True

        for tool in required_tools:
            tool_path = self.get_tool_path(config_name, tool)
            if not tool_path:
                print(f"  ✗ {tool}: NOT FOUND")
                all_ok = False
                continue

            # Try to execute --version
            try:
                result = subprocess.run(
                    [str(tool_path), '--version'],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                version = result.stdout.split('\n')[0] if result.stdout else 'unknown'
                print(f"  ✓ {tool}: {tool_path.name} ({version})")

            except Exception as e:
                print(f"  ✗ {tool}: Cannot execute ({e})")
                all_ok = False

        # Optional tools
        for tool in optional_tools:
            tool_path = self.get_tool_path(config_name, tool)
            if tool_path:
                try:
                    result = subprocess.run(
                        [str(tool_path), '--version'],
                        capture_output=True,
                        text=True,
                        timeout=5
                    )
                    version = result.stdout.split('\n')[0] if result.stdout else 'unknown'
                    print(f"  ✓ {tool} (optional): {tool_path.name} ({version})")
                except:
                    print(f"  ⚠ {tool} (optional): Found but cannot execute")
            else:
                print(f"  ⚠ {tool} (optional): Not found (OK)")

        print()
        return all_ok

    def get_all_tools(self, config_name: str) -> Dict[str, Path]:
        """
        Get paths to all available tools for a configuration

        Returns:
            Dictionary mapping tool names to paths
        """
        tools = {}
        tool_names = ['mkdwarfs', 'dwarfsextract', 'dwarfsck', 'dwarfs']

        for tool in tool_names:
            path = self.get_tool_path(config_name, tool)
            if path:
                tools[tool] = path

        return tools

    def clean_config(self, config_name: str) -> bool:
        """
        Remove build directory for a configuration

        Args:
            config_name: Configuration to clean

        Returns:
            True on success
        """
        if config_name not in self.CONFIGS:
            return False

        config = self.CONFIGS[config_name]
        build_dir = self.workspace_dir / config.build_dir

        if build_dir.exists():
            print(f"Removing: {build_dir}")
            shutil.rmtree(build_dir)
            return True

        return False

    def list_configs(self) -> None:
        """Print available build configurations"""
        print("\nAvailable Build Configurations:")
        print(f"{'='*70}\n")

        for name, config in self.CONFIGS.items():
            print(f"{config.name} ({config.slug})")
            print(f"  Build dir: {config.build_dir}")
            print(f"  Can read: {', '.join(config.can_read)}")
            print(f"  Can write: {', '.join(config.can_write)}")
            print()


def main():
    """CLI interface for build manager"""
    import argparse

    parser = argparse.ArgumentParser(
        description='DwarFS Build Configuration Manager')
    parser.add_argument('--workspace', type=Path, default=Path.cwd(),
                       help='DwarFS workspace directory')
    parser.add_argument('--build', choices=['fb-only', 'thrift-only', 'both'],
                       action='append', dest='builds',
                       help='Build specific configuration(s)')
    parser.add_argument('--build-all', action='store_true',
                       help='Build all configurations (fb-only, thrift-only, both)')
    parser.add_argument('--verify', choices=['fb-only', 'thrift-only', 'both'],
                       help='Verify a configuration')
    parser.add_argument('--clean', choices=['fb-only', 'thrift-only', 'both'],
                       action='append', dest='cleans',
                       help='Clean configuration(s)')
    parser.add_argument('--list', action='store_true',
                       help='List available configurations')
    parser.add_argument('--jobs', type=int,
                       help='Number of parallel build jobs')
    parser.add_argument('--verbose', action='store_true',
                       help='Show build output')
    parser.add_argument('--no-ninja', action='store_true',
                       help='Use Make instead of Ninja')

    args = parser.parse_args()

    manager = BuildManager(args.workspace)

    if args.list:
        manager.list_configs()
        return 0

    # Clean
    if args.cleans:
        for config in args.cleans:
            manager.clean_config(config)

    # Build
    if args.builds or args.build_all:
        configs = args.builds if args.builds else ['fb-only', 'thrift-only', 'both']

        for config in configs:
            result = manager.build_config(
                config,
                ninja=not args.no_ninja,
                jobs=args.jobs,
                verbose=args.verbose
            )

            if result:
                print(f"✓ {config} built successfully")
            else:
                print(f"✗ {config} build FAILED")
                return 1

    # Verify
    if args.verify:
        if not manager.verify_config(args.verify, verbose=args.verbose):
            return 1

    return 0


if __name__ == '__main__':
    exit(main())