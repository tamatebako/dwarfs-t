"""
FUSE Manager Module

Manage FUSE mount/unmount operations across platforms (macOS/Linux).
"""

import os
import platform
import subprocess
import time


class FUSEManager:
    """Manage FUSE mount/unmount operations"""

    def __init__(self, dwarfs_path: str):
        self.dwarfs_path = dwarfs_path
        self.platform = platform.system()
        self.perfmon_trace_file = None

    def mount(self, image_path: str, mount_point: str, perfmon: bool = True) -> subprocess.Popen:
        """
        Mount FUSE filesystem in background

        Returns process handle
        """
        import tempfile

        # Build mount command
        cmd = [self.dwarfs_path, image_path, mount_point]
        if perfmon:
            if self.platform == 'Darwin':
                # macOS/fuse-t: Use perfmon_trace file instead of xattr
                self.perfmon_trace_file = tempfile.NamedTemporaryFile(mode='w+', suffix='.perfmon', delete=False)
                self.perfmon_trace_file.close()
                cmd.extend(['-o', f'perfmon=fuse,perfmon_trace={self.perfmon_trace_file.name}'])
            else:
                # Linux: Use xattr
                cmd.extend(['-o', 'perfmon=fuse'])

        # Start in background
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        # Wait for mount to be ready
        time.sleep(0.5)
        max_wait = 10
        waited = 0
        while not os.path.ismount(mount_point) and waited < max_wait:
            time.sleep(0.1)
            waited += 0.1

        if not os.path.ismount(mount_point):
            raise RuntimeError(f"Failed to mount {image_path} at {mount_point}")

        return proc

    def unmount(self, mount_point: str, proc: subprocess.Popen):
        """Unmount filesystem and terminate process"""
        # Unmount
        if self.platform == 'Darwin':
            subprocess.run(['umount', mount_point], capture_output=True)
        else:
            subprocess.run(['fusermount', '-u', mount_point], capture_output=True)

        # Wait for process
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.terminate()
            proc.wait(timeout=5)

    def get_perfmon_data(self, mount_point: str) -> str:
        """Get perfmon data before unmounting (xattr or trace file)"""
        try:
            if self.platform == 'Darwin' and self.perfmon_trace_file:
                # macOS: Read from trace file
                if os.path.exists(self.perfmon_trace_file.name):
                    with open(self.perfmon_trace_file.name, 'r') as f:
                        content = f.read()
                    # Clean up trace file
                    os.unlink(self.perfmon_trace_file.name)
                    self.perfmon_trace_file = None
                    return content
                return ""
            else:
                # Linux: Use xattr
                attr_name = 'user.dwarfs.driver.perfmon'
                result = subprocess.run(
                    ['attr', '-qg', attr_name, mount_point],
                    capture_output=True,
                    text=True
                )
                if result.returncode == 0:
                    return result.stdout

                # Try getfattr as fallback
                result = subprocess.run(
                    ['getfattr', '-n', attr_name, '--only-values', mount_point],
                    capture_output=True,
                    text=True
                )
                if result.returncode == 0:
                    return result.stdout

                return ""
        except Exception as e:
            print(f"Error getting perfmon data: {e}")
            return ""