#!/usr/bin/env python3
"""
Institutional Verification Package - Final Checklist
Verifies all required artifacts are present and valid
"""

import os
import json
import hashlib

def check_file_exists(filepath, description):
    """Check if file exists and return status"""
    exists = os.path.exists(filepath)
    status = "✅" if exists else "❌"
    size = ""
    if exists:
        size_bytes = os.path.getsize(filepath)
        if size_bytes > 1024*1024:
            size = f"({size_bytes/1024/1024:.1f} MB)"
        elif size_bytes > 1024:
            size = f"({size_bytes/1024:.1f} KB)"
        else:
            size = f"({size_bytes} bytes)"
    print(f"  {status} {description:<50} {size}")
    return exists

print("=" * 80)
print("  INSTITUTIONAL VERIFICATION PACKAGE - FINAL CHECKLIST")
print("=" * 80)
print()

# Check core data files
print("📊 CORE DATA FILES:")
print("-" * 80)
market_data_ok = check_file_exists("synthetic_ticks_with_alpha.csv", "Market data (CSV)")
metadata_ok = check_file_exists("market_data_metadata.json", "Market data metadata (JSON)")
print()

# Check verification logs
print("📄 VERIFICATION LOGS:")
print("-" * 80)
logs = [
    ("logs/INSTITUTIONAL_VERIFICATION_PACKAGE.txt", "Master verification package"),
    ("logs/institutional_replay.log", "Event replay log"),
    ("logs/latency_distributions.log", "Latency distributions"),
    ("logs/clock_synchronization.log", "Clock sync proof"),
    ("logs/risk_breaches.log", "Risk kill-switch logs"),
    ("logs/slippage_analysis.log", "Slippage analysis"),
    ("logs/system_verification.log", "System verification"),
    ("logs/strategy_metrics.log", "Strategy metrics"),
]

logs_ok = all(check_file_exists(path, desc) for path, desc in logs)
print()

# Check documentation
print("📖 DOCUMENTATION:")
print("-" * 80)
doc1_ok = check_file_exists("INSTITUTIONAL_VERIFICATION_README.md", "Institutional README")
doc2_ok = check_file_exists("INSTITUTIONAL_DATA_SUMMARY.txt", "Data management summary")
print()

# Verify SHA256 checksum
print("🔒 DATA INTEGRITY:")
print("-" * 80)
if market_data_ok and metadata_ok:
    # Calculate SHA256
    sha256_hash = hashlib.sha256()
    with open("synthetic_ticks_with_alpha.csv", 'rb') as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    calculated_checksum = sha256_hash.hexdigest()
    
    # Load expected checksum
    with open('market_data_metadata.json', 'r') as f:
        metadata = json.load(f)
    expected_checksum = metadata['sha256']
    
    if calculated_checksum == expected_checksum:
        print(f"  ✅ SHA256 checksum verified: {calculated_checksum}")
    else:
        print(f"  ❌ SHA256 mismatch!")
        print(f"     Expected: {expected_checksum}")
        print(f"     Got:      {calculated_checksum}")
else:
    print("  ⚠️  Cannot verify checksum (missing files)")
print()

# Overall status
print("=" * 80)
print("  VERIFICATION SUMMARY")
print("=" * 80)
print()

all_ok = market_data_ok and metadata_ok and logs_ok and doc1_ok and doc2_ok

if all_ok:
    print("✅ ALL REQUIRED ARTIFACTS PRESENT")
    print()
    print("Package Contents:")
    print("  • Core data files:     2 files")
    print("  • Verification logs:   8 files")
    print("  • Documentation:       2 files")
    print()
    print("Status: READY FOR INSTITUTIONAL REVIEW")
else:
    print("❌ INCOMPLETE PACKAGE")
    print()
    print("Missing files detected. Please run:")
    print("  1. python3 generate_institutional_data.py")
    print("  2. python3 generate_institutional_verification.py")

print()
print("=" * 80)

# List key metrics
if metadata_ok:
    with open('market_data_metadata.json', 'r') as f:
        metadata = json.load(f)
    
    print()
    print("KEY METRICS:")
    print("-" * 80)
    print(f"  Market Data Events:    {metadata['total_events']:,}")
    print(f"  Alpha Bursts:          {metadata['alpha_bursts']}")
    print(f"  Deterministic Seed:    {metadata['seed']}")
    print(f"  Duration:              {metadata['duration_seconds']} seconds")
    print(f"  SHA256:                {metadata['sha256'][:32]}...")
    print()

print("To view full verification package:")
print("  cat logs/INSTITUTIONAL_VERIFICATION_PACKAGE.txt")
print()
