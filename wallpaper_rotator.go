// wallpaper_rotator.go
package main

import (
	"bufio"
	"flag"
	"fmt"
	"io/ioutil"
	"math/rand"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

type WallpaperRotator struct {
	dir          string
	images       []string
	interval     int
	randomOrder  bool
	currentIndex int
	running      bool
	currentWall  string
	stopChan     chan bool
}

func NewWallpaperRotator() *WallpaperRotator {
	w := &WallpaperRotator{
		interval:     60,
		randomOrder:  false,
		currentIndex: 0,
		running:      false,
		stopChan:     make(chan bool),
	}
	w.loadState()
	return w
}

func (w *WallpaperRotator) loadState() {
	stateFile := filepath.Join(os.Getenv("HOME"), ".wallpaper_rotator_state.txt")
	data, err := ioutil.ReadFile(stateFile)
	if err != nil {
		return
	}
	lines := strings.Split(string(data), "\n")
	if len(lines) >= 4 {
		w.dir = lines[0]
		interval, _ := strconv.Atoi(lines[1])
		w.interval = interval
		w.randomOrder = lines[2] == "true"
		w.currentWall = lines[3]
		w.scanImages()
	}
}

func (w *WallpaperRotator) saveState() {
	stateFile := filepath.Join(os.Getenv("HOME"), ".wallpaper_rotator_state.txt")
	content := fmt.Sprintf("%s\n%d\n%t\n%s\n", w.dir, w.interval, w.randomOrder, w.currentWall)
	ioutil.WriteFile(stateFile, []byte(content), 0644)
}

func (w *WallpaperRotator) scanImages() {
	if w.dir == "" {
		return
	}
	exts := []string{".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff"}
	files, _ := ioutil.ReadDir(w.dir)
	w.images = []string{}
	for _, f := range files {
		if !f.IsDir() {
			ext := strings.ToLower(filepath.Ext(f.Name()))
			for _, e := range exts {
				if ext == e {
					w.images = append(w.images, filepath.Join(w.dir, f.Name()))
					break
				}
			}
		}
	}
}

func (w *WallpaperRotator) setWallpaper(path string) bool {
	if _, err := os.Stat(path); os.IsNotExist(err) {
		fmt.Printf("File not found: %s\n", path)
		return false
	}
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("cmd", "/c", "reg", "add", "HKEY_CURRENT_USER\\Control Panel\\Desktop", "/v", "Wallpaper", "/t", "REG_SZ", "/d", path, "/f")
		cmd.Run()
		cmd = exec.Command("cmd", "/c", "RUNDLL32.EXE", "user32.dll,UpdatePerUserSystemParameters", "1", "True")
	case "darwin":
		script := fmt.Sprintf(`tell application "Finder" to set desktop picture to POSIX file "%s"`, path)
		cmd = exec.Command("osascript", "-e", script)
	default: // Linux
		// Try GNOME
		if _, err := exec.Command("gsettings", "get", "org.gnome.desktop.background", "picture-uri").Output(); err == nil {
			cmd = exec.Command("gsettings", "set", "org.gnome.desktop.background", "picture-uri", "file://"+path)
		} else {
			cmd = exec.Command("feh", "--bg-scale", path)
		}
	}
	err := cmd.Run()
	if err != nil {
		fmt.Printf("Error setting wallpaper: %v\n", err)
		return false
	}
	w.currentWall = path
	w.saveState()
	return true
}

func (w *WallpaperRotator) getCurrentWallpaper() string {
	if w.currentWall != "" {
		return w.currentWall
	}
	// Try to detect
	switch runtime.GOOS {
	case "darwin":
		out, _ := exec.Command("osascript", "-e", `tell application "Finder" to get desktop picture`).Output()
		return strings.TrimSpace(string(out))
	case "linux":
		out, _ := exec.Command("gsettings", "get", "org.gnome.desktop.background", "picture-uri").Output()
		uri := strings.Trim(string(out), "'\n")
		if strings.HasPrefix(uri, "file://") {
			return uri[7:]
		}
	}
	return "Unknown"
}

func (w *WallpaperRotator) nextWallpaper() {
	if len(w.images) == 0 {
		fmt.Println("No images in directory. Please add images.")
		return
	}
	var idx int
	if w.randomOrder {
		idx = rand.Intn(len(w.images))
	} else {
		w.currentIndex = (w.currentIndex + 1) % len(w.images)
		idx = w.currentIndex
	}
	path := w.images[idx]
	if w.setWallpaper(path) {
		fmt.Printf("Wallpaper changed to: %s\n", path)
	}
}

func (w *WallpaperRotator) rotationLoop() {
	ticker := time.NewTicker(time.Duration(w.interval) * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			w.nextWallpaper()
		case <-w.stopChan:
			return
		}
	}
}

func (w *WallpaperRotator) start() {
	if w.running {
		fmt.Println("Already running.")
		return
	}
	if w.dir == "" {
		fmt.Println("Please set a directory with --add-dir")
		return
	}
	w.scanImages()
	if len(w.images) == 0 {
		fmt.Println("No images found in directory.")
		return
	}
	w.running = true
	go w.rotationLoop()
	fmt.Printf("Started rotation every %d seconds.\n", w.interval)
	w.saveState()
}

func (w *WallpaperRotator) stop() {
	if !w.running {
		fmt.Println("Not running.")
		return
	}
	w.running = false
	w.stopChan <- true
	fmt.Println("Stopped rotation.")
}

func (w *WallpaperRotator) setDirectory(path string) {
	if _, err := os.Stat(path); os.IsNotExist(err) {
		fmt.Printf("Directory not found: %s\n", path)
		return
	}
	w.dir = path
	w.scanImages()
	w.currentIndex = 0
	w.saveState()
	fmt.Printf("Directory set to: %s (%d images)\n", path, len(w.images))
}

func main() {
	var (
		addDir   = flag.String("add-dir", "", "Directory with wallpapers")
		interval = flag.Int("interval", 0, "Rotation interval in seconds")
		random   = flag.Bool("random", false, "Random order")
		start    = flag.Bool("start", false, "Start rotation")
		stop     = flag.Bool("stop", false, "Stop rotation")
		next     = flag.Bool("next", false, "Force next wallpaper")
		current  = flag.Bool("current", false, "Show current wallpaper")
	)
	flag.Parse()

	w := NewWallpaperRotator()

	if *addDir != "" {
		w.setDirectory(*addDir)
	}
	if *interval > 0 {
		w.interval = *interval
		w.saveState()
	}
	if *random {
		w.randomOrder = true
		w.saveState()
	}
	if *start {
		w.start()
	} else if *stop {
		w.stop()
	} else if *next {
		w.nextWallpaper()
	} else if *current {
		cur := w.getCurrentWallpaper()
		fmt.Printf("Current wallpaper: %s\n", cur)
	} else {
		flag.Usage()
	}

	if *start {
		// keep alive
		select {}
	}
}
