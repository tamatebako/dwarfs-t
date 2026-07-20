# Contributing to DwarFS

Thank you for your interest in contributing to DwarFS! This document will help you get started.

## Quick Start

1. **Read the Developer Workflow**: See [DEVELOPER_WORKFLOW.md](DEVELOPER_WORKFLOW.md) for the complete guide on:
   - Setting up your development environment
   - Daily development workflow
   - Running tests
   - Creating pull requests
   - Release process

2. **Understand the Architecture**: See [ARCHITECTURE.md](ARCHITECTURE.md) for an overview of the system design.

3. **Build the Project**: See [BUILD_SYSTEM_ARCHITECTURE.md](BUILD_SYSTEM_ARCHITECTURE.md) for build instructions.

## Development Setup

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t.git
cd dwarfs-t

# Install vcpkg (one-time)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Quick validation
./scripts/one-step/test-everything.sh --quick
```

## Running Tests

```bash
# Quick test (2-5 minutes)
./scripts/one-step/test-everything.sh --quick

# Full test suite (10-30 minutes)
./scripts/one-step/test-everything.sh
```

## Pull Request Process

1. Create a feature branch from `main`
2. Make your changes
3. Run `./scripts/one-step/test-everything.sh --quick`
4. Create a pull request with a clear description
5. Address review feedback

## Code Style

- C++20 with modern idioms
- Follow existing code patterns
- Keep functions focused and readable
- Add tests for new functionality

## Questions?

- Open an issue for bugs or feature requests
- See [TESTING.md](TESTING.md) for testing details
- See [.github/README.md](.github/README.md) for CI/CD information

## License

By contributing, you agree that your contributions will be licensed under the GPL-3.0 License.
