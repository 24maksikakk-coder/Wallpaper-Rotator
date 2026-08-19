🖼️ Wallpaper Rotator — Multi‑Language Automatic Wallpaper Changer
8 languages, one smart wallpaper rotator – automatically change your desktop background from a folder of images, with random or sequential mode, interval control, and cross‑platform support.

✨ Features
📂 Watch a folder – pick any directory with images (JPG, PNG, BMP, etc.)

⏱️ Interval timer – set how often to change (seconds, minutes, or hours)

🎲 Random or sequential – shuffle or go in order

💻 Cross‑platform – works on Windows, macOS, and Linux (GNOME/KDE)

🔍 Preview current – show the path of the current wallpaper

⏯️ Start/Stop – control the rotation daemon

🔄 Next wallpaper – manually trigger a change

🚀 Common Usage
All implementations follow the same CLI pattern:

bash
# Add a folder with images and start rotating every 30 seconds
<command> --add-dir ~/Pictures/Wallpapers --interval 30 --start

# Show the current wallpaper path
<command> --current

# Manually switch to the next wallpaper
<command> --next

# Stop rotation
<command> --stop
Arguments:

--add-dir <path> – set the wallpaper directory

--interval <seconds> – rotation interval (default: 60)

--random – enable random order (default: sequential)

--start – start the rotation daemon

--stop – stop the rotation daemon

--next – force change to next wallpaper

--current – show current wallpaper path

--help – show help

📸 Example Output
text
🖼️ Wallpaper Rotator
📂 Directory: /home/user/Pictures/Wallpapers (12 images)
⏱️ Interval: 60 seconds
🎲 Mode: random
▶️ Running
Current: /home/user/Pictures/Wallpapers/beach.jpg
📁 Repository Structure
text
.
├── README.md
├── python/
│   └── wallpaper_rotator.py
├── go/
│   └── wallpaper_rotator.go
├── javascript/
│   └── wallpaper_rotator.js
├── ruby/
│   └── wallpaper_rotator.rb
├── php/
│   └── wallpaper_rotator.php
├── java/
│   └── WallpaperRotator.java
├── csharp/
│   └── WallpaperRotator.cs
└── cpp/
    └── wallpaper_rotator.cpp
