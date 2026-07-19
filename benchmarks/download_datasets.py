#!/usr/bin/env python3
"""
Standalone wrapper for dataset downloader

Downloads benchmark datasets from official sources with verification.
"""

import sys
from pathlib import Path

# Add lib directory to path
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

from dataset_downloader import main

if __name__ == '__main__':
    main()