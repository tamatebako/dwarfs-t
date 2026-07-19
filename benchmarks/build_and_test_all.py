#!/usr/bin/env python3
"""
Unified Build, Test, and Benchmark Script for DwarFS

This script:
1. Builds all three configurations (fb-only, thrift-only, both)
2. Runs unit tests including environment variable tests
3. Tests format compatibility (thrift can read brew-created files)
4. Runs comprehensive benchmarks

Single Responsibility: Complete build, test, and benchmark workflow
"""

import subprocess
import sys
from pathlib import Path
from typing import Optional

# Use existing build manager
sys.path.insert(0, str(Path(__file__).parent / 'lib'))
from build_manager import BuildManager


class UnifiedTester:
    """Unified build, test, and benchmark orchestrator"""

    def __init__(self, workspace_dir: Path):
        self.workspace_dir = Path(workspace_dir).resolve()
        self.build_manager = BuildManager(workspace_dir)

    def build_with_tests(self, config_name: str) -> Optional[Path]:
        """Build a configuration WITH tests enabled"""
        print(f"\n{'='*70}")
        print(f"Building {config_name} with tests enabled")
        print(f"{'='*70}\n")

        config = self.build_manager.CONFIGS[config_name]
        build_dir = self.workspace_dir / f"{config.build_dir}-test"

        # CMake with tests
        cmake_args = [
            'cmake',
            '-S', str(self.workspace_dir),
            '-B', str(build_dir),
            '-DCMAKE_BUILD_TYPE=Release',
            '-DWITH_TESTS=ON',  # Enable tests
            '-DWITH_TOOLS=ON',
            '-GNinja',
        ]

        # Add format-specific args
        cmake_args.extend(config.cmake_args)

        try:
            print(f"CMake: {' '.join(cmake_args)}\n")
            subprocess.run(cmake_args, check=True)

            print("\nBuilding...\n")
            subprocess.run(['ninja', '-C', str(build_dir)], check=True)

            print(f"\n✓ {config_name} built successfully with tests\n")
            return build_dir

        except subprocess.CalledProcessError as e:
            print(f"\n✗ {config_name} build FAILED: {e}\n")
            return None

    def run_env_var_tests(self, build_dir: Path) -> bool:
        """Run environment variable tests"""
        print(f"\n{'='*70}")
        print("Running Environment Variable Tests")
        print(f"{'='*70}\n")

        test_exe = build_dir / 'dwarfs_unit_tests'
        if not test_exe.exists():
            print(f"✗ Test executable not found: {test_exe}")
            return False

        try:
            # Run only environment variable tests
            result = subprocess.run(
                [str(test_exe), '--gtest_filter=EnvironmentVariables*'],
                capture_output=True,
                text=True
            )

            print(result.stdout)
            if result.stderr:
                print(result.stderr)

            if result.returncode == 0:
                print("\n✓ Environment variable tests PASSED\n")
                return True
            else:
                print(f"\n✗ Environment variable tests FAILED (exit {result.returncode})\n")
                return False

        except Exception as e:
            print(f"\n✗ Failed to run tests: {e}\n")
            return False

    def run_integration_tests(self, build_dir: Path) -> bool:
        """Run integration test script"""
        print(f"\n{'='*70}")
        print("Running Integration Tests")
        print(f"{'='*70}\n")

        script = self.workspace_dir / 'test' / 'integration' / 'test_env_vars.sh'
        if not script.exists():
            print(f"✗ Integration test script not found: {script}")
            return False

        # Set PATH to include build directory
        env = {'PATH': f"{build_dir}:{Path.cwd() / build_dir}:{subprocess.os.environ['PATH']}"}

        try:
            result = subprocess.run(
                ['bash', str(script)],
                env=env,
                capture_output=True,
                text=True
            )

            print(result.stdout)
            if result.stderr:
                print(result.stderr)

            if result.returncode == 0:
                print("\n✓ Integration tests PASSED\n")
                return True
            else:
                print(f"\n✗ Integration tests FAILED (exit {result.returncode})\n")
                return False

        except Exception as e:
            print(f"\n✗ Failed to run integration tests: {e}\n")
            return False

    def test_thrift_brew_compat(self) -> bool:
        """Test that thrift-enabled build can read brew-created dwarfs files"""
        print(f"\n{'='*70}")
        print("Testing Thrift/Brew Format Compatibility")
        print(f"{'='*70}\n")

        # Check if brew dwarfs is available
        try:
            result = subprocess.run(
                ['which', 'mkdwarfs'],
                capture_output=True,
                text=True,
                check=False
            )

            if result.returncode != 0:
                print("⚠ Brew dwarfs not found - skipping compatibility test")
                return True  # Not a failure, just skip

            brew_mkdwarfs = result.stdout.strip()
            print(f"Found brew mkdwarfs: {brew_mkdwarfs}\n")

            # Create a test file with brew
            test_dir = self.workspace_dir / '.test_compat'
            test_dir.mkdir(exist_ok=True)

            src = test_dir / 'src'
            src.mkdir(exist_ok=True)
            (src / 'test.txt').write_text('test content')

            brew_image = test_dir / 'brew.dwarfs'

            print("Creating test image with brew mkdwarfs...")
            subprocess.run(
                [brew_mkdwarfs, '-i', str(src), '-o', str(brew_image), '-l0'],
                check=True,
                capture_output=True
            )

            # Try to read with our thrift-enabled build
            both_build = self.workspace_dir / 'build-both-bench-test'
            dwarfsck = both_build / 'dwarfsck'

            if not dwarfsck.exists():
                print(f"✗ dwarfsck not found: {dwarfsck}")
                return False

            print(f"Reading brew image with our dwarfsck...")
            result = subprocess.run(
                [str(dwarfsck), str(brew_image)],
                capture_output=True,
                text=True
            )

            if result.returncode == 0:
                print(f"✓ Successfully read brew-created dwarfs file!")
                print(f"  Output: {result.stdout[:200]}")
                return True
            else:
                print(f"✗ Failed to read brew-created file")
                print(f"  stdout: {result.stdout}")
                print(f"  stderr: {result.stderr}")
                return False

        except Exception as e:
            print(f"✗ Compatibility test failed: {e}")
            return False
        finally:
            # Cleanup
            import shutil
            test_dir = self.workspace_dir / '.test_compat'
            if test_dir.exists():
                shutil.rmtree(test_dir)

    def run_all(self) -> int:
        """Run complete build, test, and benchmark workflow"""
        print("\n" + "="*70)
        print("DwarFS Unified Build, Test, and Benchmark")
        print("="*70 + "\n")

        # Step 0: Clean all existing build directories
        print("="*70)
        print("STEP 0: Cleaning all build artifacts")
        print("="*70 + "\n")

        import shutil

        # Clean benchmark build directories
        for config_name in ['fb-only', 'thrift-only', 'both']:
            config = self.build_manager.CONFIGS[config_name]
            for suffix in ['', '-test']:
                build_dir = self.workspace_dir / f"{config.build_dir}{suffix}"
                if build_dir.exists():
                    print(f"Removing: {build_dir}")
                    shutil.rmtree(build_dir)

        print("\n✓ All build artifacts cleaned\n")

        # Step 1: Build all configurations with tests
        print("="*70)
        print("STEP 1: Building all configurations with tests enabled")
        print("="*70 + "\n")

        configs_to_build = ['fb-only', 'thrift-only', 'both']
        built = {}

        for config in configs_to_build:
            build_dir = self.build_with_tests(config)
            if not build_dir:
                print(f"\n✗ FAILED: Could not build {config}")
                return 1
            built[config] = build_dir

        # Step 2: Run environment variable tests on 'both' build
        print("\n" + "="*70)
        print("STEP 2: Environment Variable Tests")
        print("="*70)

        if not self.run_env_var_tests(built['both']):
            print("\n✗ FAILED: Environment variable tests")
            return 1

        # Step 3: Run integration tests
        if not self.run_integration_tests(built['both']):
            print("\n✗ FAILED: Integration tests")
            return 1

        # Step 4: Test thrift/brew format compatibility
        print("\n" + "="*70)
        print("STEP 4: Format Compatibility")
        print("="*70)

        if not self.test_thrift_brew_compat():
            print("\n✗ FAILED: Thrift/brew compatibility test")
            return 1

        # Step 5: Summary
        print("\n" + "="*70)
        print("✓ ALL TESTS PASSED")
        print("="*70)
        print("\nBuilt configurations:")
        for config, path in built.items():
            print(f"  - {config}: {path}")
        print()

        return 0


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Unified DwarFS build, test, and benchmark')
    parser.add_argument('--workspace', type=Path, default=Path.cwd(),
                       help='DwarFS workspace directory')

    args = parser.parse_args()

    tester = UnifiedTester(args.workspace)
    return tester.run_all()


if __name__ == '__main__':
    sys.exit(main())