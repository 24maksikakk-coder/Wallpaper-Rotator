// wallpaper_rotator.js
#!/usr/bin/env node
const fs = require('fs');
const path = require('path');
const { exec, execSync } = require('child_process');
const os = require('os');

class WallpaperRotator {
    constructor() {
        this.dir = null;
        this.images = [];
        this.interval = 60;
        this.randomOrder = false;
        this.currentIndex = 0;
        this.running = false;
        this.timer = null;
        this.currentWall = null;
        this.loadState();
    }

    loadState() {
        const stateFile = path.join(os.homedir(), '.wallpaper_rotator_state.txt');
        if (fs.existsSync(stateFile)) {
            const data = fs.readFileSync(stateFile, 'utf8').split('\n');
            if (data.length >= 4) {
                this.dir = data[0] || null;
                this.interval = parseInt(data[1]) || 60;
                this.randomOrder = data[2] === 'true';
                this.currentWall = data[3] || null;
                this.scanImages();
            }
        }
    }

    saveState() {
        const stateFile = path.join(os.homedir(), '.wallpaper_rotator_state.txt');
        const content = `${this.dir || ''}\n${this.interval}\n${this.randomOrder}\n${this.currentWall || ''}`;
        fs.writeFileSync(stateFile, content);
    }

    scanImages() {
        if (!this.dir || !fs.existsSync(this.dir)) {
            this.images = [];
            return;
        }
        const exts = ['.jpg', '.jpeg', '.png', '.bmp', '.gif', '.tiff'];
        const files = fs.readdirSync(this.dir);
        this.images = files.filter(f => {
            const ext = path.extname(f).toLowerCase();
            return exts.includes(ext);
        }).map(f => path.join(this.dir, f));
    }

    setWallpaper(filePath) {
        if (!fs.existsSync(filePath)) {
            console.error(`File not found: ${filePath}`);
            return false;
        }
        const platform = os.platform();
        try {
            if (platform === 'win32') {
                execSync(`reg add "HKEY_CURRENT_USER\\Control Panel\\Desktop" /v Wallpaper /t REG_SZ /d "${filePath}" /f`);
                execSync('RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters 1 True');
            } else if (platform === 'darwin') {
                const script = `tell application "Finder" to set desktop picture to POSIX file "${filePath}"`;
                execSync(`osascript -e '${script}'`);
            } else {
                // Linux: try GNOME
                try {
                    execSync('gsettings get org.gnome.desktop.background picture-uri');
                    execSync(`gsettings set org.gnome.desktop.background picture-uri "file://${filePath}"`);
                } catch (e) {
                    execSync(`feh --bg-scale "${filePath}"`);
                }
            }
            this.currentWall = filePath;
            this.saveState();
            return true;
        } catch (err) {
            console.error(`Error setting wallpaper: ${err.message}`);
            return false;
        }
    }

    getCurrentWallpaper() {
        if (this.currentWall) return this.currentWall;
        const platform = os.platform();
        try {
            if (platform === 'darwin') {
                const out = execSync('osascript -e \'tell application "Finder" to get desktop picture\'', { encoding: 'utf8' });
                return out.trim();
            } else if (platform === 'linux') {
                const out = execSync('gsettings get org.gnome.desktop.background picture-uri', { encoding: 'utf8' });
                let uri = out.trim().replace(/^'|'$/g, '');
                if (uri.startsWith('file://')) return uri.slice(7);
            }
        } catch (e) {}
        return 'Unknown';
    }

    nextWallpaper() {
        if (this.images.length === 0) {
            console.log('No images in directory. Please add images.');
            return;
        }
        let idx;
        if (this.randomOrder) {
            idx = Math.floor(Math.random() * this.images.length);
        } else {
            this.currentIndex = (this.currentIndex + 1) % this.images.length;
            idx = this.currentIndex;
        }
        const img = this.images[idx];
        if (this.setWallpaper(img)) {
            console.log(`Wallpaper changed to: ${img}`);
        }
    }

    rotationLoop() {
        this.timer = setInterval(() => {
            this.nextWallpaper();
        }, this.interval * 1000);
    }

    start() {
        if (this.running) {
            console.log('Already running.');
            return;
        }
        if (!this.dir || !fs.existsSync(this.dir)) {
            console.log('Please set a directory with --add-dir');
            return;
        }
        this.scanImages();
        if (this.images.length === 0) {
            console.log('No images found in directory.');
            return;
        }
        this.running = true;
        this.rotationLoop();
        console.log(`Started rotation every ${this.interval} seconds.`);
        this.saveState();
    }

    stop() {
        if (this.timer) {
            clearInterval(this.timer);
            this.timer = null;
        }
        this.running = false;
        console.log('Stopped rotation.');
    }

    setDirectory(dirPath) {
        if (!fs.existsSync(dirPath) || !fs.statSync(dirPath).isDirectory()) {
            console.error(`Directory not found: ${dirPath}`);
            return;
        }
        this.dir = dirPath;
        this.scanImages();
        this.currentIndex = 0;
        this.saveState();
        console.log(`Directory set to: ${dirPath} (${this.images.length} images)`);
    }
}

const args = process.argv.slice(2);
let addDir = null, interval = null, random = false, start = false, stop = false, next = false, current = false;
for (let i = 0; i < args.length; i++) {
    switch (args[i]) {
        case '--add-dir': addDir = args[++i]; break;
        case '--interval': interval = parseInt(args[++i]); break;
        case '--random': random = true; break;
        case '--start': start = true; break;
        case '--stop': stop = true; break;
        case '--next': next = true; break;
        case '--current': current = true; break;
        case '--help':
            console.log(`Usage: node wallpaper_rotator.js --add-dir <path> [--interval <sec>] [--random] [--start|--stop|--next|--current]`);
            process.exit(0);
    }
}

const rotator = new WallpaperRotator();
if (addDir) rotator.setDirectory(addDir);
if (interval !== null) { rotator.interval = interval; rotator.saveState(); }
if (random) { rotator.randomOrder = true; rotator.saveState(); }
if (start) rotator.start();
else if (stop) rotator.stop();
else if (next) rotator.nextWallpaper();
else if (current) console.log(`Current wallpaper: ${rotator.getCurrentWallpaper()}`);

if (start) {
    // keep process alive
    process.stdin.resume();
}
