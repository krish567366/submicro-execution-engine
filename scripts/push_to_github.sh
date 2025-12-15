#!/bin/bash

# Push to GitHub Repository
# Repository: https://github.com/krish567366/submicro-execution-engine.git

echo "=== Pushing Low-Latency Trading System to GitHub ==="
echo ""
echo "✓ All .hpp files excluded (proprietary logic protected)"
echo "✓ All .cpp files excluded (implementation protected)"  
echo "✓ All .rs files excluded (Rust implementation protected)"
echo ""
echo "What WILL be published:"
echo "  - Documentation (.md files)"
echo "  - Build scripts (.sh)"
echo "  - Configuration (CMakeLists.txt, Cargo.toml)"
echo "  - Python utilities (data generation, verification)"
echo "  - Demo logs (synthetic data only)"
echo "  - Dashboard (HTML/JS only)"
echo ""

# Commit the changes
echo "Step 1: Creating initial commit..."
git commit -m "Initial commit: Trading system research platform (no proprietary code)

Documentation only - core logic protected:
- Architecture and system design docs
- Institutional logging framework
- Build system and utilities  
- Demo logs with synthetic data

All .hpp/.cpp/.rs files excluded per .gitignore"

# Add remote
echo ""
echo "Step 2: Adding GitHub remote..."
git remote add origin https://github.com/krish567366/submicro-execution-engine.git

# Push to GitHub
echo ""
echo "Step 3: Pushing to GitHub..."
echo "You will need to authenticate with GitHub"
echo ""
git branch -M main
git push -u origin main

echo ""
echo "=== Done! ==="
echo "Repository: https://github.com/krish567366/submicro-execution-engine"
