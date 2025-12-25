# Contributing to SubMicro Execution Engine

Thank you for your interest in contributing to the SubMicro Execution Engine! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Documentation](#documentation)
- [Reporting Issues](#reporting-issues)

## Code of Conduct

This project follows a code of conduct to ensure a welcoming environment for all contributors. By participating, you agree to abide by its terms. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before contributing.

## Getting Started

### Prerequisites

- **C++17 compatible compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **CMake 3.15+**
- **Rust 1.50+** (for Rust components)
- **Python 3.8+** (for scripts and testing)
- **Linux environment** (recommended for full functionality)

### Quick Start

1. Fork the repository
2. Clone your fork: `git clone https://github.com/your-username/submicro-execution-engine.git`
3. Create a feature branch: `git checkout -b feature/your-feature-name`
4. Set up the development environment (see below)
5. Make your changes
6. Run tests: `./scripts/run_tests.sh`
7. Submit a pull request

## Development Setup

### Building the Project

```bash
# Clone the repository
git clone https://github.com/krish567366/submicro-execution-engine.git
cd submicro-execution-engine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests
ctest
```

### Development Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake clang-format clang-tidy \
                     python3 python3-pip rustc cargo \
                     libboost-all-dev libtbb-dev

# Python dependencies
pip3 install -r requirements.txt
```

## How to Contribute

### Types of Contributions

- **Bug fixes**: Fix issues in the issue tracker
- **Features**: Implement new functionality
- **Documentation**: Improve documentation, tutorials, or examples
- **Performance**: Optimize existing code
- **Testing**: Add tests or improve test coverage

### Finding Issues to Work On

- Check the [issue tracker](https://github.com/krish567366/submicro-execution-engine/issues) for open issues
- Look for issues labeled `good first issue` or `help wanted`
- Check the [project board](https://github.com/krish567366/submicro-execution-engine/projects) for planned work

## Pull Request Process

1. **Fork** the repository and create your branch from `main`
2. **Update documentation** if your changes affect user-facing functionality
3. **Add tests** for new features or bug fixes
4. **Ensure tests pass** locally and in CI
5. **Update CHANGELOG.md** if your changes are user-facing
6. **Submit a pull request** with a clear description of the changes

### Pull Request Guidelines

- Use a clear, descriptive title
- Reference any related issues
- Include screenshots for UI changes
- Keep pull requests focused on a single feature or fix
- Ensure all CI checks pass

### Commit Message Guidelines

Use clear, descriptive commit messages:

```
type(scope): description

[optional body]

[optional footer]
```

Types:
- `feat`: New features
- `fix`: Bug fixes
- `docs`: Documentation changes
- `style`: Code style changes
- `refactor`: Code refactoring
- `test`: Adding tests
- `chore`: Maintenance tasks

## Coding Standards

### C++ Standards

- Use C++17 features
- Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with modifications:
  - Use 4 spaces for indentation
  - Use `snake_case` for variables and functions
  - Use `PascalCase` for types and classes
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) instead of raw pointers
- Use RAII principles
- Document complex algorithms and data structures

### Rust Standards

- Follow the official [Rust Style Guide](https://doc.rust-lang.org/style-guide/)
- Use `rustfmt` for code formatting
- Use `clippy` for linting
- Write comprehensive documentation with `cargo doc`

### Python Standards

- Follow [PEP 8](https://pep8.org/)
- Use type hints for function parameters and return values
- Write docstrings for all public functions and classes

## Testing

### Running Tests

```bash
# Run all tests
./scripts/run_tests.sh

# Run specific test suite
./scripts/run_tests.sh --suite performance

# Run with verbose output
./scripts/run_tests.sh --verbose
```

### Writing Tests

- Write unit tests for all new functionality
- Include integration tests for complex features
- Add performance regression tests
- Ensure test coverage remains above 80%

### Performance Testing

- Use the benchmark suite for performance-critical code
- Include latency measurements in tests
- Document performance expectations

## Documentation

### Code Documentation

- Use Doxygen-style comments for C++ code
- Use Rustdoc for Rust code
- Use docstrings for Python code
- Document all public APIs

### User Documentation

- Update README.md for significant changes
- Add examples for new features
- Update API documentation
- Maintain changelog

## Reporting Issues

### Bug Reports

When reporting bugs, please include:

- **Description**: Clear description of the issue
- **Steps to reproduce**: Step-by-step instructions
- **Expected behavior**: What should happen
- **Actual behavior**: What actually happens
- **Environment**: OS, compiler version, hardware specs
- **Logs**: Relevant log output

### Feature Requests

For feature requests, please include:

- **Description**: Clear description of the proposed feature
- **Use case**: Why this feature would be useful
- **Implementation ideas**: Any thoughts on how to implement it
- **Alternatives**: Other solutions you've considered

## Recognition

Contributors will be recognized in:
- CHANGELOG.md for significant contributions
- GitHub's contributor insights
- Project documentation

Thank you for contributing to the SubMicro Execution Engine!