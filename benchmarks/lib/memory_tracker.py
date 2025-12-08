"""
Memory Tracker Module

Cross-platform memory usage tracking for benchmark commands using /usr/bin/time.
"""

import platform
import re
import subprocess
import time
from typing import Dict, Any


class MemoryTracker:
    """Track memory usage for command execution"""

    def __init__(self):
        self.platform = platform.system()

    def measure_command(self, cmd, shell: bool = False) -> Dict[str, Any]:
        """
        Execute command and measure time + memory

        Args:
            cmd: Command as string or list
            shell: Whether to use shell

        Returns:
            {
                'exit_code': int,
                'returncode': int,  # alias for exit_code
                'stdout': str,
                'stderr': str,
                'wall_time': float,
                'user_time': float,
                'sys_time': float,
                'memory_mb': float
            }
        """
        # Convert list command to string if needed
        if isinstance(cmd, list):
            if shell:
                cmd = ' '.join(cmd)
            cmd_list = cmd
        else:
            cmd_list = cmd
            
        if self.platform == 'Darwin':  # macOS
            # Use /usr/bin/time -l for detailed metrics
            if isinstance(cmd_list, list):
                time_cmd = ['/usr/bin/time', '-l'] + cmd_list
                use_shell = False
            else:
                time_cmd = f"/usr/bin/time -l {cmd_list}"
                use_shell = True
        else:  # Linux
            if isinstance(cmd_list, list):
                time_cmd = ['/usr/bin/time', '-v'] + cmd_list
                use_shell = False
            else:
                time_cmd = f"/usr/bin/time -v {cmd_list}"
                use_shell = True

        start = time.time()

        try:
            result = subprocess.run(
                time_cmd,
                shell=use_shell,
                capture_output=True,
                text=True
            )

            wall_time = time.time() - start
            memory_mb = self._parse_memory(result.stderr)
            user_time, sys_time = self._parse_cpu_time(result.stderr)

            return {
                'exit_code': result.returncode,
                'returncode': result.returncode,  # alias
                'stdout': result.stdout,
                'stderr': result.stderr,
                'wall_time': wall_time,
                'user_time': user_time,
                'sys_time': sys_time,
                'memory_mb': memory_mb
            }
        except Exception as e:
            return {
                'exit_code': -1,
                'returncode': -1,
                'stdout': '',
                'stderr': str(e),
                'wall_time': 0,
                'user_time': 0,
                'sys_time': 0,
                'memory_mb': 0
            }

    def _parse_memory(self, time_output: str) -> float:
        """Parse memory from /usr/bin/time output"""
        if self.platform == 'Darwin':
            # macOS: "maximum resident set size" in bytes
            match = re.search(r'(\d+)\s+maximum resident set size', time_output)
            if match:
                bytes_val = int(match.group(1))
                return bytes_val / (1024 * 1024)  # Convert to MB
        else:
            # Linux: "Maximum resident set size (kbytes)"
            match = re.search(r'Maximum resident set size \(kbytes\):\s+(\d+)', time_output)
            if match:
                kb_val = int(match.group(1))
                return kb_val / 1024  # Convert to MB

        return 0.0
    
    def _parse_cpu_time(self, time_output: str) -> tuple:
        """Parse user and system CPU time from /usr/bin/time output"""
        user_time = 0.0
        sys_time = 0.0
        
        if self.platform == 'Darwin':
            # macOS format: "X.XX user Y.YY system"
            match = re.search(r'([\d.]+)\s+user\s+([\d.]+)\s+system', time_output)
            if match:
                user_time = float(match.group(1))
                sys_time = float(match.group(2))
        else:
            # Linux format: "User time (seconds): X.XX"
            user_match = re.search(r'User time \(seconds\):\s+([\d.]+)', time_output)
            sys_match = re.search(r'System time \(seconds\):\s+([\d.]+)', time_output)
            if user_match:
                user_time = float(user_match.group(1))
            if sys_match:
                sys_time = float(sys_match.group(1))
        
        return user_time, sys_time