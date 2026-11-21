#!/usr/bin/env python3
"""
Crops a desktop screenshot to include only the active application window,
trimming menu bars and uniform desktop backgrounds.
"""
import sys
import os

def crop_window(input_path, output_path):
    try:
        from PIL import Image
    except ImportError:
        # Fallback to copy if PIL not installed
        import shutil
        shutil.copyfile(input_path, output_path)
        return

    if not os.path.exists(input_path):
        return

    im = Image.open(input_path)
    w, h = im.size
    
    # On macOS, skip the top menu bar (32px) if present
    start_y = 32 if sys.platform == 'darwin' or h >= 768 else 0
    
    # Find bounding box of non-black / non-background content
    min_x, min_y, max_x, max_y = w, h, 0, 0
    bg_color = im.getpixel((0, h - 1))[:3]
    
    for y in range(start_y, h):
        for x in range(w):
            pix = im.getpixel((x, y))[:3]
            if pix != bg_color and pix != (0, 0, 0):
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
                
    if max_x > min_x and max_y > min_y:
        cropped = im.crop((min_x, min_y, max_x + 1, max_y + 1))
        cropped.save(output_path)
    else:
        im.save(output_path)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: crop_window.py <input.png> <output.png>")
        sys.exit(1)
    crop_window(sys.argv[1], sys.argv[2])
