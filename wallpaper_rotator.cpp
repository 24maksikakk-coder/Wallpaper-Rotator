// wallpaper_rotator.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <random>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#endif

using namespace std;

class WallpaperRotator {
private:
    string dir;
    vector<string> images;
    int interval = 60;
    bool randomOrder = false;
    int currentIndex = 0;
    bool running = false;
    string currentWall;
    string stateFile;

public:
    WallpaperRotator() {
        stateFile = string(getenv("HOME")) + "/.wallpaper_rotator_state.txt";
        loadState();
    }

    void loadState() {
        ifstream f(stateFile);
        if (!f.is_open()) return;
        string line;
        if (getline(f, line)) dir = line;
        if (getline(f, line)) interval = stoi(line);
        if (getline(f, line)) randomOrder = (line == "true");
        if (getline(f, line)) currentWall = line;
        scanImages();
    }

    void saveState() {
        ofstream f(stateFile);
        f << dir << "\n" << interval << "\n" << (randomOrder ? "true" : "false") << "\n" << currentWall << "\n";
    }

    void scanImages() {
        images.clear();
        if (dir.empty()) return;
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* entry;
        vector<string> exts = {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff"};
        while ((entry = readdir(d)) != nullptr) {
            string name = entry->d_name;
            if (name == "." || name == "..") continue;
            string ext = name.substr(name.find_last_of('.'));
            for (auto& e : exts) {
                if (ext == e) {
                    images.push_back(dir + "/" + name);
                    break;
                }
            }
        }
        closedir(d);
    }

    bool setWallpaper(const string& path) {
        if (access(path.c_str(), F_OK) != 0) {
            cerr << "File not found: " << path << endl;
            return false;
        }
#ifdef _WIN32
        // Windows
        string cmd = "reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"" + path + "\" /f";
        system(cmd.c_str());
        system("RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters 1 True");
#else
        // Linux/macOS
#ifdef __APPLE__
        string script = "tell application \"Finder\" to set desktop picture to POSIX file \"" + path + "\"";
        string cmd = "osascript -e '" + script + "'";
        system(cmd.c_str());
#else
        // Linux: try GNOME
        if (system("gsettings get org.gnome.desktop.background picture-uri 2>/dev/null") == 0) {
            string cmd = "gsettings set org.gnome.desktop.background picture-uri \"file://" + path + "\"";
            system(cmd.c_str());
        } else {
            string cmd = "feh --bg-scale \"" + path + "\"";
            system(cmd.c_str());
        }
#endif
#endif
        currentWall = path;
        saveState();
        return true;
    }

    string getCurrentWallpaper() {
        if (!currentWall.empty()) return currentWall;
#ifdef __APPLE__
        string cmd = "osascript -e 'tell application \"Finder\" to get desktop picture'";
        char buffer[256];
        string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
        }
        result.erase(result.find_last_not_of("\n") + 1);
        return result;
#elif !defined(_WIN32)
        string cmd = "gsettings get org.gnome.desktop.background picture-uri 2>/dev/null";
        char buffer[256];
        string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
        }
        if (!result.empty()) {
            result.erase(result.find_last_not_of("\n") + 1);
            if (result.front() == '\'' && result.back() == '\'')
                result = result.substr(1, result.length()-2);
            if (result.find("file://") == 0)
                return result.substr(7);
        }
#endif
        return "Unknown";
    }

    void nextWallpaper() {
        if (images.empty()) {
            cout << "No images in directory. Please add images.\n";
            return;
        }
        int idx;
        if (randomOrder) {
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(0, images.size()-1);
            idx = dis(gen);
        } else {
            currentIndex = (currentIndex + 1) % images.size();
            idx = currentIndex;
        }
        string path = images[idx];
        if (setWallpaper(path))
            cout << "Wallpaper changed to: " << path << "\n";
    }

    void rotationLoop() {
        while (running) {
            if (images.empty())
                cout << "No images to rotate. Waiting...\n";
            else
                nextWallpaper();
            this_thread::sleep_for(chrono::seconds(interval));
        }
    }

    void start() {
        if (running) {
            cout << "Already running.\n";
            return;
        }
        if (dir.empty() || access(dir.c_str(), F_OK) != 0) {
            cout << "Please set a directory with --add-dir\n";
            return;
        }
        scanImages();
        if (images.empty()) {
            cout << "No images found in directory.\n";
            return;
        }
        running = true;
        thread t(&WallpaperRotator::rotationLoop, this);
        t.detach();
        cout << "Started rotation every " << interval << " seconds.\n";
        saveState();
    }

    void stop() {
        running = false;
        cout << "Stopped rotation.\n";
    }

    void setDirectory(const string& path) {
        if (access(path.c_str(), F_OK) != 0) {
            cout << "Directory not found: " << path << "\n";
            return;
        }
        dir = path;
        scanImages();
        currentIndex = 0;
        saveState();
        cout << "Directory set to: " << path << " (" << images.size() << " images)\n";
    }
};

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"add-dir", required_argument, 0, 'a'},
        {"interval", required_argument, 0, 'i'},
        {"random", no_argument, 0, 'r'},
        {"start", no_argument, 0, 's'},
        {"stop", no_argument, 0, 't'},
        {"next", no_argument, 0, 'n'},
        {"current", no_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };
    int opt;
    string addDir, intervalStr;
    bool randomFlag = false, startFlag = false, stopFlag = false, nextFlag = false, currentFlag = false;
    while ((opt = getopt_long(argc, argv, "a:i:rstnch", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'a': addDir = optarg; break;
            case 'i': intervalStr = optarg; break;
            case 'r': randomFlag = true; break;
            case 's': startFlag = true; break;
            case 't': stopFlag = true; break;
            case 'n': nextFlag = true; break;
            case 'c': currentFlag = true; break;
            case 'h': default:
                cout << "Usage: wallpaper_rotator --add-dir <dir> [--interval <sec>] [--random] [--start|--stop|--next|--current]\n";
                return 0;
        }
    }

    WallpaperRotator rotator;
    if (!addDir.empty()) rotator.setDirectory(addDir);
    if (!intervalStr.empty()) {
        rotator.interval = stoi(intervalStr);
        rotator.saveState();
    }
    if (randomFlag) {
        rotator.randomOrder = true;
        rotator.saveState();
    }

    if (startFlag) {
        rotator.start();
        // keep running
        while (rotator.running) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    } else if (stopFlag) {
        rotator.stop();
    } else if (nextFlag) {
        rotator.nextWallpaper();
    } else if (currentFlag) {
        cout << "Current wallpaper: " << rotator.getCurrentWallpaper() << "\n";
    } else {
        cout << "No command given. Use --help.\n";
    }
    return 0;
}
