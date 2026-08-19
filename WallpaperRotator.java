// WallpaperRotator.java
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.stream.Collectors;

public class WallpaperRotator {
    private String dir;
    private List<String> images = new ArrayList<>();
    private int interval = 60;
    private boolean randomOrder = false;
    private int currentIndex = 0;
    private boolean running = false;
    private String currentWall = null;
    private ScheduledExecutorService scheduler;
    private final String stateFile = System.getProperty("user.home") + "/.wallpaper_rotator_state.txt";

    public WallpaperRotator() {
        loadState();
    }

    private void loadState() {
        try {
            Path path = Paths.get(stateFile);
            if (Files.exists(path)) {
                List<String> lines = Files.readAllLines(path);
                if (lines.size() >= 4) {
                    if (!lines.get(0).isEmpty()) dir = lines.get(0);
                    interval = Integer.parseInt(lines.get(1));
                    randomOrder = Boolean.parseBoolean(lines.get(2));
                    if (!lines.get(3).isEmpty()) currentWall = lines.get(3);
                    scanImages();
                }
            }
        } catch (Exception e) {}
    }

    private void saveState() {
        try {
            String content = String.join("\n", 
                dir == null ? "" : dir,
                String.valueOf(interval),
                String.valueOf(randomOrder),
                currentWall == null ? "" : currentWall
            );
            Files.write(Paths.get(stateFile), content.getBytes());
        } catch (Exception e) {}
    }

    private void scanImages() {
        if (dir == null || !Files.isDirectory(Paths.get(dir))) {
            images.clear();
            return;
        }
        String[] exts = {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff"};
        try {
            images = Files.list(Paths.get(dir))
                .filter(Files::isRegularFile)
                .map(Path::toString)
                .filter(p -> {
                    String ext = p.substring(p.lastIndexOf('.')).toLowerCase();
                    return Arrays.asList(exts).contains(ext);
                })
                .collect(Collectors.toList());
        } catch (IOException e) {
            images.clear();
        }
    }

    private boolean setWallpaper(String path) {
        if (!Files.exists(Paths.get(path))) {
            System.out.println("File not found: " + path);
            return false;
        }
        String os = System.getProperty("os.name").toLowerCase();
        try {
            if (os.contains("win")) {
                Runtime.getRuntime().exec("reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"" + path + "\" /f");
                Runtime.getRuntime().exec("RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters 1 True");
            } else if (os.contains("mac")) {
                String script = "tell application \"Finder\" to set desktop picture to POSIX file \"" + path + "\"";
                Runtime.getRuntime().exec(new String[]{"osascript", "-e", script});
            } else {
                // Linux
                Process p = Runtime.getRuntime().exec(new String[]{"gsettings", "get", "org.gnome.desktop.background", "picture-uri"});
                if (p.waitFor() == 0) {
                    Runtime.getRuntime().exec(new String[]{"gsettings", "set", "org.gnome.desktop.background", "picture-uri", "file://" + path});
                } else {
                    Runtime.getRuntime().exec(new String[]{"feh", "--bg-scale", path});
                }
            }
            currentWall = path;
            saveState();
            return true;
        } catch (Exception e) {
            System.err.println("Error setting wallpaper: " + e.getMessage());
            return false;
        }
    }

    public String getCurrentWallpaper() {
        if (currentWall != null) return currentWall;
        String os = System.getProperty("os.name").toLowerCase();
        try {
            if (os.contains("mac")) {
                Process p = Runtime.getRuntime().exec(new String[]{"osascript", "-e", "tell application \"Finder\" to get desktop picture"});
                try (BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
                    return br.readLine().trim();
                }
            } else if (os.contains("linux")) {
                Process p = Runtime.getRuntime().exec(new String[]{"gsettings", "get", "org.gnome.desktop.background", "picture-uri"});
                try (BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
                    String line = br.readLine();
                    if (line != null) {
                        String uri = line.trim().replace("'", "");
                        if (uri.startsWith("file://")) return uri.substring(7);
                    }
                }
            }
        } catch (Exception e) {}
        return "Unknown";
    }

    public void nextWallpaper() {
        if (images.isEmpty()) {
            System.out.println("No images in directory. Please add images.");
            return;
        }
        int idx = randomOrder ? new Random().nextInt(images.size()) :
            (currentIndex = (currentIndex + 1) % images.size());
        String path = images.get(idx);
        if (setWallpaper(path)) {
            System.out.println("Wallpaper changed to: " + path);
        }
    }

    private void rotationLoop() {
        scheduler = Executors.newSingleThreadScheduledExecutor();
        scheduler.scheduleAtFixedRate(this::nextWallpaper, interval, interval, TimeUnit.SECONDS);
    }

    public void start() {
        if (running) {
            System.out.println("Already running.");
            return;
        }
        if (dir == null || !Files.isDirectory(Paths.get(dir))) {
            System.out.println("Please set a directory with --add-dir");
            return;
        }
        scanImages();
        if (images.isEmpty()) {
            System.out.println("No images found in directory.");
            return;
        }
        running = true;
        rotationLoop();
        System.out.println("Started rotation every " + interval + " seconds.");
        saveState();
    }

    public void stop() {
        if (scheduler != null) {
            scheduler.shutdownNow();
            scheduler = null;
        }
        running = false;
        System.out.println("Stopped rotation.");
    }

    public void setDirectory(String path) {
        if (!Files.isDirectory(Paths.get(path))) {
            System.out.println("Directory not found: " + path);
            return;
        }
        dir = path;
        scanImages();
        currentIndex = 0;
        saveState();
        System.out.println("Directory set to: " + path + " (" + images.size() + " images)");
    }

    public static void main(String[] args) throws Exception {
        Map<String, String> opts = new HashMap<>();
        for (int i = 0; i < args.length; i++) {
            if (args[i].startsWith("--")) {
                String key = args[i].substring(2);
                if (i+1 < args.length && !args[i+1].startsWith("--")) {
                    opts.put(key, args[++i]);
                } else {
                    opts.put(key, "");
                }
            }
        }
        WallpaperRotator rotator = new WallpaperRotator();
        if (opts.containsKey("add-dir")) rotator.setDirectory(opts.get("add-dir"));
        if (opts.containsKey("interval")) {
            rotator.interval = Integer.parseInt(opts.get("interval"));
            rotator.saveState();
        }
        if (opts.containsKey("random")) {
            rotator.randomOrder = true;
            rotator.saveState();
        }
        if (opts.containsKey("start")) {
            rotator.start();
            // keep alive
            while (rotator.running) {
                Thread.sleep(1000);
            }
        } else if (opts.containsKey("stop")) {
            rotator.stop();
        } else if (opts.containsKey("next")) {
            rotator.nextWallpaper();
        } else if (opts.containsKey("current")) {
            System.out.println("Current wallpaper: " + rotator.getCurrentWallpaper());
        } else {
            System.out.println("No command given. Use --help.");
        }
    }
}
