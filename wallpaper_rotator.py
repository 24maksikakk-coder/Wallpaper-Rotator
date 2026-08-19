# wallpaper_rotator.py
import os
import sys
import time
import glob
import random
import threading
import argparse
import subprocess
import platform
from pathlib import Path

class WallpaperRotator:
    def __init__(self):
        self.dir = None
        self.images = []
        self.interval = 60
        self.random_order = False
        self.current_index = 0
        self.running = False
        self.thread = None
        self.current_wallpaper = None
        self.load_state()

    def load_state(self):
        # Simple persistence via file
        state_file = os.path.expanduser("~/.wallpaper_rotator_state.txt")
        if os.path.exists(state_file):
            with open(state_file, 'r') as f:
                lines = f.readlines()
                if lines:
                    self.dir = lines[0].strip()
                    self.interval = int(lines[1].strip()) if len(lines) > 1 else 60
                    self.random_order = lines[2].strip() == 'True' if len(lines) > 2 else False
                    self.current_wallpaper = lines[3].strip() if len(lines) > 3 else None
                    self.scan_images()

    def save_state(self):
        state_file = os.path.expanduser("~/.wallpaper_rotator_state.txt")
        with open(state_file, 'w') as f:
            f.write(f"{self.dir or ''}\n")
            f.write(f"{self.interval}\n")
            f.write(f"{self.random_order}\n")
            f.write(f"{self.current_wallpaper or ''}\n")

    def scan_images(self):
        if not self.dir or not os.path.isdir(self.dir):
            return
        exts = ['*.jpg', '*.jpeg', '*.png', '*.bmp', '*.gif', '*.tiff']
        self.images = []
        for ext in exts:
            self.images.extend(glob.glob(os.path.join(self.dir, ext)))
            self.images.extend(glob.glob(os.path.join(self.dir, ext.upper())))
        self.images = list(set(self.images))  # deduplicate

    def set_wallpaper(self, path):
        if not os.path.exists(path):
            print(f"File not found: {path}")
            return False
        system = platform.system()
        try:
            if system == 'Windows':
                import ctypes
                ctypes.windll.user32.SystemParametersInfoW(20, 0, path, 3)
            elif system == 'Darwin':  # macOS
                script = f'tell application "Finder" to set desktop picture to POSIX file "{path}"'
                subprocess.run(['osascript', '-e', script], check=True)
            else:  # Linux
                # Try GNOME
                if os.system("gsettings get org.gnome.desktop.background picture-uri > /dev/null 2>&1") == 0:
                    subprocess.run(['gsettings', 'set', 'org.gnome.desktop.background', 'picture-uri', f'file://{path}'], check=True)
                else:
                    # Fallback: use feh
                    subprocess.run(['feh', '--bg-scale', path], check=True)
            self.current_wallpaper = path
            self.save_state()
            return True
        except Exception as e:
            print(f"Error setting wallpaper: {e}")
            return False

    def get_current_wallpaper(self):
        if self.current_wallpaper:
            return self.current_wallpaper
        # Try to detect current
        system = platform.system()
        try:
            if system == 'Windows':
                # Not trivial with ctypes; fallback
                pass
            elif system == 'Darwin':
                result = subprocess.run(['osascript', '-e', 'tell application "Finder" to get desktop picture'], capture_output=True, text=True)
                if result.returncode == 0:
                    return result.stdout.strip()
            else:
                # GNOME
                result = subprocess.run(['gsettings', 'get', 'org.gnome.desktop.background', 'picture-uri'], capture_output=True, text=True)
                if result.returncode == 0:
                    uri = result.stdout.strip().strip("'")
                    if uri.startswith('file://'):
                        return uri[7:]
        except:
            pass
        return "Unknown"

    def next_wallpaper(self):
        if not self.images:
            print("No images in directory. Please add images.")
            return
        if self.random_order:
            idx = random.randint(0, len(self.images)-1)
        else:
            self.current_index = (self.current_index + 1) % len(self.images)
            idx = self.current_index
        path = self.images[idx]
        if self.set_wallpaper(path):
            print(f"Wallpaper changed to: {path}")

    def rotation_loop(self):
        while self.running:
            if self.images:
                self.next_wallpaper()
            else:
                print("No images to rotate. Waiting...")
            time.sleep(self.interval)

    def start(self):
        if self.running:
            print("Already running.")
            return
        if not self.dir or not os.path.isdir(self.dir):
            print("Please set a directory with --add-dir")
            return
        self.scan_images()
        if not self.images:
            print("No images found in directory.")
            return
        self.running = True
        self.thread = threading.Thread(target=self.rotation_loop, daemon=True)
        self.thread.start()
        print(f"Started rotation every {self.interval} seconds.")
        self.save_state()

    def stop(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1)
        print("Stopped rotation.")

    def set_directory(self, path):
        if not os.path.isdir(path):
            print(f"Directory not found: {path}")
            return
        self.dir = path
        self.scan_images()
        self.current_index = 0
        self.save_state()
        print(f"Directory set to: {path} ({len(self.images)} images)")

def main():
    parser = argparse.ArgumentParser(description="Wallpaper Rotator")
    parser.add_argument('--add-dir', help='Directory with wallpapers')
    parser.add_argument('--interval', type=int, help='Rotation interval in seconds')
    parser.add_argument('--random', action='store_true', help='Random order')
    parser.add_argument('--start', action='store_true', help='Start rotation')
    parser.add_argument('--stop', action='store_true', help='Stop rotation')
    parser.add_argument('--next', action='store_true', help='Force next wallpaper')
    parser.add_argument('--current', action='store_true', help='Show current wallpaper')
    args = parser.parse_args()

    rotator = WallpaperRotator()

    if args.add_dir:
        rotator.set_directory(args.add_dir)
    if args.interval is not None:
        rotator.interval = args.interval
        rotator.save_state()
    if args.random:
        rotator.random_order = True
        rotator.save_state()
    if args.start:
        rotator.start()
    elif args.stop:
        rotator.stop()
    elif args.next:
        rotator.next_wallpaper()
    elif args.current:
        current = rotator.get_current_wallpaper()
        print(f"Current wallpaper: {current}")
    else:
        parser.print_help()

    # If start was given, keep main thread alive
    if args.start:
        try:
            while rotator.running:
                time.sleep(1)
        except KeyboardInterrupt:
            rotator.stop()

if __name__ == '__main__':
    main()
