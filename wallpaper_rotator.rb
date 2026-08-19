# wallpaper_rotator.rb
#!/usr/bin/env ruby
require 'optparse'
require 'fileutils'
require 'rbconfig'

class WallpaperRotator
  attr_reader :dir, :images, :interval, :random_order, :current_index, :running, :current_wall

  def initialize
    @dir = nil
    @images = []
    @interval = 60
    @random_order = false
    @current_index = 0
    @running = false
    @current_wall = nil
    @thread = nil
    load_state
  end

  def load_state
    state_file = File.expand_path("~/.wallpaper_rotator_state.txt")
    if File.exist?(state_file)
      lines = File.readlines(state_file).map(&:chomp)
      @dir = lines[0] if lines[0] && !lines[0].empty?
      @interval = lines[1].to_i if lines[1]
      @random_order = lines[2] == 'true' if lines[2]
      @current_wall = lines[3] if lines[3] && !lines[3].empty?
      scan_images
    end
  end

  def save_state
    state_file = File.expand_path("~/.wallpaper_rotator_state.txt")
    File.write(state_file, "#{@dir}\n#{@interval}\n#{@random_order}\n#{@current_wall}\n")
  end

  def scan_images
    return unless @dir && Dir.exist?(@dir)
    exts = %w[.jpg .jpeg .png .bmp .gif .tiff]
    @images = Dir.entries(@dir).select do |f|
      ext = File.extname(f).downcase
      exts.include?(ext)
    end.map { |f| File.join(@dir, f) }
  end

  def set_wallpaper(path)
    unless File.exist?(path)
      puts "File not found: #{path}"
      return false
    end
    host_os = RbConfig::CONFIG['host_os']
    begin
      case host_os
      when /mswin|mingw|windows/
        system("reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"#{path}\" /f")
        system("RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters 1 True")
      when /darwin|mac os/
        system("osascript -e 'tell application \"Finder\" to set desktop picture to POSIX file \"#{path}\"'")
      else # Linux
        if system("gsettings get org.gnome.desktop.background picture-uri 2>/dev/null")
          system("gsettings set org.gnome.desktop.background picture-uri \"file://#{path}\"")
        else
          system("feh --bg-scale \"#{path}\"")
        end
      end
      @current_wall = path
      save_state
      true
    rescue => e
      puts "Error setting wallpaper: #{e.message}"
      false
    end
  end

  def get_current_wallpaper
    return @current_wall if @current_wall
    host_os = RbConfig::CONFIG['host_os']
    begin
      case host_os
      when /darwin|mac os/
        out = `osascript -e 'tell application "Finder" to get desktop picture'`.strip
        return out unless out.empty?
      when /linux/
        out = `gsettings get org.gnome.desktop.background picture-uri`.strip
        if out.start_with?("'file://")
          return out[8..-2] if out.end_with?("'")
          return out[7..-1]
        end
      end
    rescue
    end
    "Unknown"
  end

  def next_wallpaper
    if @images.empty?
      puts "No images in directory. Please add images."
      return
    end
    idx = if @random_order
            rand(@images.length)
          else
            @current_index = (@current_index + 1) % @images.length
            @current_index
          end
    path = @images[idx]
    if set_wallpaper(path)
      puts "Wallpaper changed to: #{path}"
    end
  end

  def rotation_loop
    while @running
      if @images.empty?
        puts "No images to rotate. Waiting..."
      else
        next_wallpaper
      end
      sleep(@interval)
    end
  end

  def start
    if @running
      puts "Already running."
      return
    end
    unless @dir && Dir.exist?(@dir)
      puts "Please set a directory with --add-dir"
      return
    end
    scan_images
    if @images.empty?
      puts "No images found in directory."
      return
    end
    @running = true
    @thread = Thread.new { rotation_loop }
    puts "Started rotation every #{@interval} seconds."
    save_state
  end

  def stop
    @running = false
    @thread.join if @thread
    puts "Stopped rotation."
  end

  def set_directory(path)
    unless Dir.exist?(path)
      puts "Directory not found: #{path}"
      return
    end
    @dir = path
    scan_images
    @current_index = 0
    save_state
    puts "Directory set to: #{path} (#{@images.length} images)"
  end
end

options = {}
OptionParser.new do |opts|
  opts.banner = "Usage: ruby wallpaper_rotator.rb [options]"
  opts.on("--add-dir DIR", "Directory with wallpapers") { |v| options[:dir] = v }
  opts.on("--interval SECONDS", Integer, "Rotation interval in seconds") { |v| options[:interval] = v }
  opts.on("--random", "Random order") { options[:random] = true }
  opts.on("--start", "Start rotation") { options[:start] = true }
  opts.on("--stop", "Stop rotation") { options[:stop] = true }
  opts.on("--next", "Force next wallpaper") { options[:next] = true }
  opts.on("--current", "Show current wallpaper") { options[:current] = true }
  opts.on_tail("--help", "Show this message") { puts opts; exit }
end.parse!

rotator = WallpaperRotator.new
if options[:dir]
  rotator.set_directory(options[:dir])
end
if options[:interval]
  rotator.interval = options[:interval]
  rotator.save_state
end
if options[:random]
  rotator.random_order = true
  rotator.save_state
end

if options[:start]
  rotator.start
elsif options[:stop]
  rotator.stop
elsif options[:next]
  rotator.next_wallpaper
elsif options[:current]
  puts "Current wallpaper: #{rotator.get_current_wallpaper}"
else
  puts "No command given. Use --help for usage."
end

if options[:start]
  # keep thread alive
  begin
    sleep 1 while rotator.running
  rescue Interrupt
    rotator.stop
  end
end
