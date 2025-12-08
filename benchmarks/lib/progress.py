"""
Progress Display Module

Provides Docker-style progress indicators for long-running benchmark operations.
"""

import sys
import time
from typing import Optional


class ProgressBar:
    """Simple progress bar with percentage and timing"""

    def __init__(self, total: int, description: str = "", width: int = 40):
        self.total = total
        self.current = 0
        self.description = description
        self.width = width
        self.start_time = time.time()
        self.last_update = 0

    def update(self, n: int = 1, status: str = ""):
        """Update progress by n steps"""
        self.current = min(self.current + n, self.total)

        # Only update display every 0.5 seconds to avoid flicker
        now = time.time()
        if now - self.last_update < 0.5 and self.current < self.total:
            return

        self.last_update = now
        self._display(status)

    def finish(self, status: str = "Complete"):
        """Mark as finished"""
        self.current = self.total
        self._display(status)
        print()  # New line after completion

    def _display(self, status: str = ""):
        """Display progress bar"""
        pct = (self.current / self.total) * 100 if self.total > 0 else 100
        filled = int((self.current / self.total) * self.width) if self.total > 0 else self.width
        bar = '█' * filled + '░' * (self.width - filled)

        elapsed = time.time() - self.start_time
        if self.current > 0 and self.current < self.total:
            eta = (elapsed / self.current) * (self.total - self.current)
            eta_str = f" ETA: {self._format_time(eta)}"
        else:
            eta_str = ""

        status_str = f" {status}" if status else ""

        # Clear line and print progress
        sys.stdout.write('\r')
        sys.stdout.write(f"{self.description} [{bar}] {pct:5.1f}%{eta_str}{status_str}")
        sys.stdout.flush()

    def _format_time(self, seconds: float) -> str:
        """Format seconds as HH:MM:SS or MM:SS"""
        if seconds < 60:
            return f"{int(seconds)}s"
        elif seconds < 3600:
            mins = int(seconds / 60)
            secs = int(seconds % 60)
            return f"{mins}m{secs:02d}s"
        else:
            hours = int(seconds / 3600)
            mins = int((seconds % 3600) / 60)
            return f"{hours}h{mins:02d}m"


class StageProgress:
    """Docker-style multi-stage progress display"""

    def __init__(self):
        self.stages = []
        self.current_stage = None

    def start_stage(self, name: str, total_steps: int):
        """Start a new stage"""
        if self.current_stage:
            self.current_stage.finish()

        self.current_stage = ProgressBar(total_steps, f"  {name}")
        self.stages.append(self.current_stage)

    def update(self, n: int = 1, status: str = ""):
        """Update current stage"""
        if self.current_stage:
            self.current_stage.update(n, status)

    def finish_stage(self, status: str = "✓"):
        """Finish current stage"""
        if self.current_stage:
            self.current_stage.finish(status)
            self.current_stage = None


def print_header(text: str, char: str = "=", width: int = 60):
    """Print a header line"""
    print(f"\n{char * width}")
    print(f"{text}")
    print(f"{char * width}")


def print_status(emoji: str, text: str):
    """Print a status line with emoji"""
    print(f"{emoji} {text}")


class ProgressDisplay:
    """Simple progress display for benchmark operations"""
    
    def __init__(self):
        self.enabled = True
    
    def start(self, message: str):
        """Start a new operation"""
        if self.enabled:
            print(f"\n{message}")
    
    def update(self, message: str):
        """Update progress"""
        if self.enabled:
            print(f"  {message}")
    
    def finish(self, message: str = "Done"):
        """Finish operation"""
        if self.enabled:
            print(f"  {message}")