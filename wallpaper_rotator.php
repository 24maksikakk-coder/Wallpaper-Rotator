# wallpaper_rotator.php
#!/usr/bin/env php
<?php
class WallpaperRotator
{
    private $dir;
    private $images = [];
    private $interval = 60;
    private $randomOrder = false;
    private $currentIndex = 0;
    private $running = false;
    private $currentWall = null;
    private $pidFile;

    public function __construct()
    {
        $this->pidFile = getenv('HOME') . '/.wallpaper_rotator.pid';
        $this->loadState();
    }

    private function loadState()
    {
        $stateFile = getenv('HOME') . '/.wallpaper_rotator_state.txt';
        if (file_exists($stateFile)) {
            $lines = file($stateFile, FILE_IGNORE_NEW_LINES);
            if (count($lines) >= 4) {
                $this->dir = $lines[0] ?: null;
                $this->interval = (int)$lines[1] ?: 60;
                $this->randomOrder = $lines[2] === 'true';
                $this->currentWall = $lines[3] ?: null;
                $this->scanImages();
            }
        }
    }

    private function saveState()
    {
        $stateFile = getenv('HOME') . '/.wallpaper_rotator_state.txt';
        file_put_contents($stateFile, implode("\n", [
            $this->dir ?? '',
            $this->interval,
            $this->randomOrder ? 'true' : 'false',
            $this->currentWall ?? ''
        ]));
    }

    private function scanImages()
    {
        if (!$this->dir || !is_dir($this->dir)) {
            $this->images = [];
            return;
        }
        $exts = ['jpg', 'jpeg', 'png', 'bmp', 'gif', 'tiff'];
        $this->images = [];
        $dh = opendir($this->dir);
        while (($file = readdir($dh)) !== false) {
            if ($file === '.' || $file === '..') continue;
            $ext = strtolower(pathinfo($file, PATHINFO_EXTENSION));
            if (in_array($ext, $exts)) {
                $this->images[] = $this->dir . DIRECTORY_SEPARATOR . $file;
            }
        }
        closedir($dh);
    }

    private function setWallpaper($path)
    {
        if (!file_exists($path)) {
            echo "File not found: $path\n";
            return false;
        }
        $os = strtoupper(substr(PHP_OS, 0, 3));
        try {
            if ($os === 'WIN') {
                exec("reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"$path\" /f");
                exec("RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters 1 True");
            } elseif ($os === 'DAR') { // macOS
                $script = "tell application \"Finder\" to set desktop picture to POSIX file \"$path\"";
                exec("osascript -e '$script'");
            } else { // Linux
                // Try GNOME
                exec("gsettings get org.gnome.desktop.background picture-uri 2>/dev/null", $out, $ret);
                if ($ret === 0) {
                    exec("gsettings set org.gnome.desktop.background picture-uri \"file://$path\"");
                } else {
                    exec("feh --bg-scale \"$path\"");
                }
            }
            $this->currentWall = $path;
            $this->saveState();
            return true;
        } catch (Exception $e) {
            echo "Error setting wallpaper: " . $e->getMessage() . "\n";
            return false;
        }
    }

    public function getCurrentWallpaper()
    {
        if ($this->currentWall) return $this->currentWall;
        $os = strtoupper(substr(PHP_OS, 0, 3));
        if ($os === 'DAR') {
            $out = shell_exec("osascript -e 'tell application \"Finder\" to get desktop picture'");
            return trim($out);
        } elseif ($os === 'LIN') {
            $out = shell_exec("gsettings get org.gnome.desktop.background picture-uri 2>/dev/null");
            $uri = trim($out, "'\n");
            if (strpos($uri, 'file://') === 0) return substr($uri, 7);
        }
        return "Unknown";
    }

    public function nextWallpaper()
    {
        if (empty($this->images)) {
            echo "No images in directory. Please add images.\n";
            return;
        }
        $idx = $this->randomOrder ? rand(0, count($this->images)-1) :
            ($this->currentIndex = ($this->currentIndex + 1) % count($this->images));
        $path = $this->images[$idx];
        if ($this->setWallpaper($path)) {
            echo "Wallpaper changed to: $path\n";
        }
    }

    public function start()
    {
        if ($this->running) {
            echo "Already running.\n";
            return;
        }
        if (!$this->dir || !is_dir($this->dir)) {
            echo "Please set a directory with --add-dir\n";
            return;
        }
        $this->scanImages();
        if (empty($this->images)) {
            echo "No images found in directory.\n";
            return;
        }
        $this->running = true;
        // Write PID for stop command
        file_put_contents($this->pidFile, getmypid());
        echo "Started rotation every {$this->interval} seconds.\n";
        $this->saveState();
        // Main loop
        while ($this->running) {
            $this->nextWallpaper();
            sleep($this->interval);
        }
    }

    public function stop()
    {
        if (file_exists($this->pidFile)) {
            $pid = (int)file_get_contents($this->pidFile);
            if ($pid) {
                if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') {
                    exec("taskkill /PID $pid /F");
                } else {
                    posix_kill($pid, SIGTERM);
                }
                unlink($this->pidFile);
                echo "Stopped rotation.\n";
                return;
            }
        }
        echo "No running instance found.\n";
    }

    public function setDirectory($path)
    {
        if (!is_dir($path)) {
            echo "Directory not found: $path\n";
            return;
        }
        $this->dir = $path;
        $this->scanImages();
        $this->currentIndex = 0;
        $this->saveState();
        echo "Directory set to: $path (" . count($this->images) . " images)\n";
    }
}

$opts = getopt("", ["add-dir:", "interval:", "random", "start", "stop", "next", "current", "help"]);
if (isset($opts['help'])) {
    echo "Usage: php wallpaper_rotator.php --add-dir <dir> [--interval <sec>] [--random] [--start|--stop|--next|--current]\n";
    exit(0);
}
$rotator = new WallpaperRotator();
if (isset($opts['add-dir'])) $rotator->setDirectory($opts['add-dir']);
if (isset($opts['interval'])) {
    $rotator->interval = (int)$opts['interval'];
    $rotator->saveState();
}
if (isset($opts['random'])) {
    $rotator->randomOrder = true;
    $rotator->saveState();
}
if (isset($opts['start'])) {
    $rotator->start();
} elseif (isset($opts['stop'])) {
    $rotator->stop();
} elseif (isset($opts['next'])) {
    $rotator->nextWallpaper();
} elseif (isset($opts['current'])) {
    echo "Current wallpaper: " . $rotator->getCurrentWallpaper() . "\n";
} else {
    echo "No command given. Use --help for usage.\n";
}
?>
