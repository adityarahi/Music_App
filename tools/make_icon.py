"""Generates assets/music_app.ico: a small circular spectrum on a dark rounded square.

Standard library only. Run from anywhere:  python3 tools/make_icon.py
"""
import colorsys
import math
import os
import struct
import zlib

MASTER = 512                      # drawn at this size, then scaled down
SIZES = [256, 128, 64, 48, 32, 24, 16]
BARS = 40


def bar_length(k):
    """Pleasant fixed 'spectrum' shape, mirrored left/right like the visualizer."""
    band = k if k < BARS // 2 else BARS - 1 - k
    t = band / (BARS // 2 - 1)
    return 0.07 + 0.11 * (0.55 + 0.45 * math.sin(t * 7.0 + 0.6)) * (1.0 - 0.45 * t)


def shade(x, y):
    """Colour of one point; x, y in 0..1 with y down. Returns (r, g, b, a) floats."""
    # Rounded-square background with a vertical gradient.
    cx, cy = x - 0.5, y - 0.5
    corner = 0.21
    qx, qy = max(abs(cx) - (0.5 - corner), 0.0), max(abs(cy) - (0.5 - corner), 0.0)
    if math.hypot(qx, qy) > corner:
        return (0.0, 0.0, 0.0, 0.0)
    top, bottom = (0.11, 0.08, 0.25), (0.03, 0.03, 0.08)
    col = [top[i] + (bottom[i] - top[i]) * y for i in range(3)]

    r = math.hypot(cx, cy)
    angle = math.atan2(cx, -cy) % (2 * math.pi)          # 0 at the top, clockwise

    # Radial bars.
    inner = 0.235
    pos = angle / (2 * math.pi) * BARS
    k = int(pos) % BARS
    within = pos - int(pos)
    if 0.2 < within < 0.8 and inner < r < inner + bar_length(k):
        band = k if k < BARS // 2 else BARS - 1 - k
        hue = 0.50 + 0.42 * band / (BARS // 2 - 1)        # cyan -> violet -> pink
        along = (r - inner) / bar_length(k)
        rgb = colorsys.hsv_to_rgb(hue % 1.0, 0.75 - 0.35 * along, 0.65 + 0.35 * along)
        return (*rgb, 1.0)

    # Glowing white ring and a soft core.
    ring = math.exp(-((r - 0.19) / 0.018) ** 2)
    core = 0.35 * math.exp(-(r / 0.13) ** 2)
    glow = min(ring + core, 1.0)
    col = [c + (g - c) * glow for c, g in zip(col, (0.85, 0.9, 1.0))]
    return (*col, 1.0)


def render(size, samples=2):
    """Renders at `size` with samples x samples supersampling. Returns RGBA bytes."""
    out = bytearray()
    step = 1.0 / (size * samples)
    for py in range(size):
        for px in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(samples):
                for sx in range(samples):
                    r, g, b, a = shade((px * samples + sx + 0.5) * step, (py * samples + sy + 0.5) * step)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            n = samples * samples
            alpha = acc[3] / n
            if alpha > 0:
                out += bytes(int(min(acc[i] / acc[3], 1.0) * 255 + 0.5) for i in range(3))
            else:
                out += b"\0\0\0"
            out.append(int(alpha * 255 + 0.5))
    return bytes(out)


def downscale(rgba, src, dst):
    """Area-average resize (premultiplied) from src x src to dst x dst."""
    ratio = src / dst
    out = bytearray()
    for y in range(dst):
        y0, y1 = int(y * ratio), max(int((y + 1) * ratio), int(y * ratio) + 1)
        for x in range(dst):
            x0, x1 = int(x * ratio), max(int((x + 1) * ratio), int(x * ratio) + 1)
            acc = [0, 0, 0, 0]
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    i = (yy * src + xx) * 4
                    a = rgba[i + 3]
                    acc[0] += rgba[i] * a
                    acc[1] += rgba[i + 1] * a
                    acc[2] += rgba[i + 2] * a
                    acc[3] += a
            n = (y1 - y0) * (x1 - x0)
            if acc[3]:
                out += bytes(min(255, acc[c] // acc[3]) for c in range(3))
            else:
                out += b"\0\0\0"
            out.append(acc[3] // n)
    return bytes(out)


def png(rgba, size):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    stride = size * 4
    raw = b"".join(b"\0" + rgba[y * stride:(y + 1) * stride] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def ico(images):
    """images: list of (size, png_bytes). PNG-compressed ICO entries (Windows Vista+)."""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, data = b"", b""
    for size, blob in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    master = render(MASTER)
    images = [(s, png(downscale(master, MASTER, s), s)) for s in SIZES]

    os.makedirs(os.path.join(root, "assets"), exist_ok=True)
    with open(os.path.join(root, "assets", "music_app.ico"), "wb") as f:
        f.write(ico(images))
    with open(os.path.join(root, "assets", "music_app_256.png"), "wb") as f:
        f.write(images[0][1])
    print("wrote assets/music_app.ico and assets/music_app_256.png")


if __name__ == "__main__":
    main()
