// WallpaperRotator.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Diagnostics;

class WallpaperRotator
{
    private string dir;
    private List<string> images = new List<string>();
    private int interval = 60;
    private bool randomOrder = false;
    private int currentIndex = 0;
    private bool running = false;
    private string currentWall = null;
    private Timer timer;
    private readonly string stateFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".wallpaper_rotator_state.txt");

    public WallpaperRotator() => LoadState();

    private void LoadState()
    {
        if (!File.Exists(stateFile)) return;
        var lines = File.ReadAllLines(stateFile);
        if (lines.Length >= 4)
        {
            if (!string.IsNullOrEmpty(lines[0])) dir = lines[0];
            int.TryParse(lines[1], out interval);
            bool.TryParse(lines[2], out randomOrder);
            if (!string.IsNullOrEmpty(lines[3])) currentWall = lines[3];
            ScanImages();
        }
    }

    private void SaveState()
    {
        File.WriteAllLines(stateFile, new string[] {
            dir ?? "",
            interval.ToString(),
            randomOrder.ToString(),
            currentWall ?? ""
        });
    }

    private void ScanImages()
    {
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
        {
            images.Clear();
            return;
        }
        var exts = new HashSet<string> { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff" };
        images = Directory.GetFiles(dir)
            .Where(f => exts.Contains(Path.GetExtension(f).ToLower()))
            .ToList();
    }

    private bool SetWallpaper(string path)
    {
        if (!File.Exists(path))
        {
            Console.WriteLine($"File not found: {path}");
            return false;
        }
        try
        {
            var os = Environment.OSVersion.Platform;
            if (os == PlatformID.Win32NT)
            {
                Process.Start("reg", $"add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"{path}\" /f");
                Process.Start("RUNDLL32.EXE", "user32.dll,UpdatePerUserSystemParameters 1 True");
            }
            else if (os == PlatformID.MacOSX)
            {
                Process.Start("osascript", $"-e 'tell application \"Finder\" to set desktop picture to POSIX file \"{path}\"'");
            }
            else // Linux
            {
                // Try GNOME
                var p = Process.Start("gsettings", "get org.gnome.desktop.background picture-uri");
                p.WaitForExit();
                if (p.ExitCode == 0)
                    Process.Start("gsettings", $"set org.gnome.desktop.background picture-uri \"file://{path}\"");
                else
                    Process.Start("feh", $"--bg-scale \"{path}\"");
            }
            currentWall = path;
            SaveState();
            return true;
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error setting wallpaper: {e.Message}");
            return false;
        }
    }

    public string GetCurrentWallpaper()
    {
        if (!string.IsNullOrEmpty(currentWall)) return currentWall;
        var os = Environment.OSVersion.Platform;
        try
        {
            if (os == PlatformID.MacOSX)
            {
                var p = Process.Start("osascript", "-e 'tell application \"Finder\" to get desktop picture'");
                p.WaitForExit();
                return p.StandardOutput.ReadToEnd().Trim();
            }
            else if (os == PlatformID.Unix) // Linux
            {
                var p = Process.Start("gsettings", "get org.gnome.desktop.background picture-uri");
                p.WaitForExit();
                string line = p.StandardOutput.ReadToEnd().Trim().Trim('\'');
                if (line.StartsWith("file://")) return line.Substring(7);
            }
        }
        catch { }
        return "Unknown";
    }

    public void NextWallpaper()
    {
        if (images.Count == 0)
        {
            Console.WriteLine("No images in directory. Please add images.");
            return;
        }
        int idx = randomOrder ? new Random().Next(images.Count) :
            (currentIndex = (currentIndex + 1) % images.Count);
        string path = images[idx];
        if (SetWallpaper(path))
            Console.WriteLine($"Wallpaper changed to: {path}");
    }

    private void TimerCallback(object state) => NextWallpaper();

    public void Start()
    {
        if (running)
        {
            Console.WriteLine("Already running.");
            return;
        }
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
        {
            Console.WriteLine("Please set a directory with --add-dir");
            return;
        }
        ScanImages();
        if (images.Count == 0)
        {
            Console.WriteLine("No images found in directory.");
            return;
        }
        running = true;
        timer = new Timer(TimerCallback, null, TimeSpan.FromSeconds(interval), TimeSpan.FromSeconds(interval));
        Console.WriteLine($"Started rotation every {interval} seconds.");
        SaveState();
    }

    public void Stop()
    {
        timer?.Dispose();
        timer = null;
        running = false;
        Console.WriteLine("Stopped rotation.");
    }

    public void SetDirectory(string path)
    {
        if (!Directory.Exists(path))
        {
            Console.WriteLine($"Directory not found: {path}");
            return;
        }
        dir = path;
        ScanImages();
        currentIndex = 0;
        SaveState();
        Console.WriteLine($"Directory set to: {path} ({images.Count} images)");
    }

    static void Main(string[] args)
    {
        var opts = new Dictionary<string, string>();
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i].StartsWith("--"))
            {
                string key = args[i].Substring(2);
                if (i + 1 < args.Length && !args[i + 1].StartsWith("--"))
                    opts[key] = args[++i];
                else
                    opts[key] = "";
            }
        }

        var rotator = new WallpaperRotator();
        if (opts.ContainsKey("add-dir")) rotator.SetDirectory(opts["add-dir"]);
        if (opts.ContainsKey("interval"))
        {
            rotator.interval = int.Parse(opts["interval"]);
            rotator.SaveState();
        }
        if (opts.ContainsKey("random"))
        {
            rotator.randomOrder = true;
            rotator.SaveState();
        }

        if (opts.ContainsKey("start"))
        {
            rotator.Start();
            // keep alive
            while (rotator.running)
                Thread.Sleep(1000);
        }
        else if (opts.ContainsKey("stop"))
            rotator.Stop();
        else if (opts.ContainsKey("next"))
            rotator.NextWallpaper();
        else if (opts.ContainsKey("current"))
            Console.WriteLine($"Current wallpaper: {rotator.GetCurrentWallpaper()}");
        else
            Console.WriteLine("No command given. Use --help.");
    }
}
