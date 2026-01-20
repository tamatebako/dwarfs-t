/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \page compat_testing Homebrew Compatibility Testing Architecture
 *
 * \section overview Overview
 *
 * This test suite verifies that our Legacy Thrift implementation can:
 * - Read DFT files created by Homebrew `dwarfs` tool
 * - Write DFT files that Homebrew `dwarfs` can read
 * - Perform full round-trip serialization/deserialization
 *
 * \section architecture Architecture
 *
 * \subsection components Components
 *
 * \subsubsection fixture_generator Fixture Generator
 *
 * The FixtureGenerator class is responsible for generating test fixture
 * DFT files using the Homebrew mkdwarfs tool.
 *
 * Key responsibilities:
 * - Generate test metadata (files, directories, symlinks)
 * - Invoke mkdwarfs to create DFT files
 * - Store fixtures with platform-specific naming
 *
 * \subsubsection fixture_cache Fixture Cache
 *
 * The FixtureCache class manages generated DFT file fixtures.
 *
 * Key responsibilities:
 * - Check if fixture exists and is valid
 * - Load fixtures from cache
 * - Validate fixture checksums
 *
 * \subsubsection homebrew_detector Homebrew Detector
 *
 * The HomebrewDetector class detects and provides information about the
 * Homebrew installation and dwarfs tool.
 *
 * Key responsibilities:
 * - Detect platform (macOS/Linux)
 * - Detect architecture (arm64/x86_64)
 * - Find mkdwarfs/dwarfs binaries
 * - Determine dwarfs version
 *
 * \subsubsection compatibility_tester Compatibility Tester
 *
 * The CompatibilityTester class executes the actual compatibility tests.
 *
 * Key responsibilities:
 * - Test reading Homebrew-generated DFT files
 * - Test writing DFT files Homebrew can read
 * - Test full round-trip serialization
 *
 * \subsection naming Fixture Naming Convention
 *
 * Fixtures are named using the following convention:
 *
 * ```
 * dwarfs-v{version}-{platform}-{arch}.dft
 *
 * Examples:
 * - dwarfs-v0.14.1-darwin-arm64.dft
 * - dwarfs-v0.14.1-darwin-x86_64.dft
 * - dwarfs-v0.14.1-linux-arm64.dft
 * - dwarfs-v0.14.1-linux-x86_64.dft
 * ```
 *
 * \section usage Usage
 *
 * \subsection developer_usage Developer Usage
 *
 * ```bash
 * # Run all compatibility tests
 * make test-compat
 *
 # Individual steps
 * make check-homebrew      # Verify Homebrew dwarfs installed
 * make generate-fixtures   # Generate test fixtures
 * make clean-fixtures      # Remove cached fixtures
 * ```
 *
 * \subsection ci_usage CI/CD Usage
 *
 * The `.github/workflows/compat-test.yml` workflow runs tests on:
 * - Every push to main branch
 * - Every pull request
 * - Weekly schedule (Sundays at midnight UTC)
 * - Manual trigger via workflow_dispatch
 *
 * \subsection packager_usage Packager Usage
 *
 * ```bash
 * # Generate compatibility report for packaging
 * make verify-compatibility
 *
 # Generates:
 * - compat-report.md
 * - compat-results.json
 * - fixture-checksums.txt
 * ```
 *
 * \section configuration Configuration
 *
 * See config.yaml for configuration options.
 *
 * Key options:
 * - `homebrew.versions`: List of dwarfs versions to test
 * - `cache.enabled`: Enable/disable fixture caching
 * - `tests`: Enable/disable specific test types
 *
 * \section platforms Platform Support
 *
 * Supported platforms:
 *
 * | Platform | Architecture | Homebrew Path |
 * |----------|-------------|---------------|
 * | macOS    | arm64       | /opt/homebrew/bin/mkdwarfs |
 * | macOS    | x86_64      | /usr/local/bin/mkdwarfs |
 * | Linux    | arm64       | ~/.linuxbrew/bin/mkdwarfs |
 * | Linux    | x86_64      | /home/linuxbrew/.linuxbrew/bin/mkdwarfs |
 *
 * \section workflows Workflows
 *
 * \subsection developer_workflow Developer Workflow
 *
 * 1. Developer makes changes to metadata serialization code
 * 2. Developer runs `make test-compat`
 * 3. Script detects if Homebrew dwarfs is installed
 * 4. If not installed, prompts to install
 * 5. Generates fixtures if missing
 * 6. Runs tests
 * 7. Reports results
 *
 * \subsection packager_workflow Packager Workflow
 *
 * 1. Package manager runs `make verify-compatibility`
 * 2. Script tests all configured versions
 * 3. Generates compatibility matrix
 * 4. Creates compatibility report
 * 5. Package manager reviews report before packaging
 *
 * \subsection release_workflow Release Manager Workflow
 *
 * 1. Release manager runs `make test-compat-full`
 * 2. CI runs full matrix (all platforms, all versions)
 * 3. Generates comprehensive report
 * 4. Creates release artifacts
 * 5. Release manager approves for release
 *
 * \section design_principles Design Principles
 *
 * - **Configuration over Convention**: config.yaml is explicit
 * - **OOP**: C++ classes with clear responsibilities
 * - **MECE**: Components are mutually exclusive, collectively exhaustive
 * - **DRY**: Reusable workflows and composite actions
 * - **Future-proof**: Easy to add new versions/platforms
 */
