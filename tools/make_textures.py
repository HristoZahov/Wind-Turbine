import os
import random
import struct
import zlib

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "resources", "textures")


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    return t * t * (3.0 - 2.0 * t)


def clamp01(t):
    return max(0.0, min(1.0, t))


def mix(c1, c2, t):
    return tuple(int(round(lerp(a, b, t))) for a, b in zip(c1, c2))


def shade(color, k):
    return tuple(max(0, min(255, int(round(c * k)))) for c in color)


def write_png(path, width, height, pixels):
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for r, g, b in row:
            raw += bytes((r, g, b))

    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", header))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def make_noise(size, cells, rng):
    grid = [[rng.random() for _ in range(cells)] for _ in range(cells)]
    step = size / cells
    out = []
    for y in range(size):
        gy = y / step
        y0 = int(gy)
        ty = smooth(gy - y0)
        y0 %= cells
        y1 = (y0 + 1) % cells
        row = []
        for x in range(size):
            gx = x / step
            x0 = int(gx)
            tx = smooth(gx - x0)
            x0 %= cells
            x1 = (x0 + 1) % cells
            top = lerp(grid[y0][x0], grid[y0][x1], tx)
            bottom = lerp(grid[y1][x0], grid[y1][x1], tx)
            row.append(lerp(top, bottom, ty))
        out.append(row)
    return out


def make_grass(size):
    rng = random.Random(1)

    big = make_noise(size, 4, rng)
    mid = make_noise(size, 16, rng)
    fine = make_noise(size, 64, rng)

    dark = (52, 92, 30)
    light = (112, 158, 58)

    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            n = 0.5 * big[y][x] + 0.3 * mid[y][x] + 0.2 * fine[y][x]
            n += rng.uniform(-0.08, 0.08)
            row.append(mix(dark, light, clamp01(n)))
        pixels.append(row)

    shades = [(130, 175, 70), (40, 75, 25), (95, 140, 50)]
    for _ in range(2500):
        x = rng.randrange(size)
        y = rng.randrange(size)
        length = rng.randint(4, 10)
        slant = rng.uniform(-0.4, 0.4)
        shade = rng.choice(shades)
        for i in range(length):
            px = int(x + slant * i) % size
            py = (y - i) % size
            pixels[py][px] = mix(pixels[py][px], shade, 0.6)

    return pixels


def make_metal(size):
    rng = random.Random(2)
    big = make_noise(size, 8, rng)
    fine = make_noise(size, 64, rng)
    paint = (222, 225, 228)

    pixels = []
    for y in range(size):
        t = y / (size - 1)
        row = []
        for x in range(size):
            n = 0.6 * big[y][x] + 0.4 * fine[y][x]
            k = 0.94 + 0.06 * n + rng.uniform(-0.015, 0.015)
            if t > 0.85:
                k -= 0.12 * (t - 0.85) / 0.15
            row.append(shade(paint, k))
        pixels.append(row)

    for seam in (size // 3, 2 * size // 3):
        for x in range(size):
            pixels[seam][x] = shade(pixels[seam][x], 0.72)
            pixels[seam + 1][x] = shade(pixels[seam + 1][x], 1.04)

    return pixels


def make_blade(width, height):
    rng = random.Random(3)
    white = (236, 238, 240)
    red = (200, 40, 35)

    pixels = []
    for y in range(height):
        t = y / height
        color = red if (t < 0.07 or 0.11 <= t < 0.18) else white
        row = []
        for x in range(width):
            row.append(shade(color, 1.0 + rng.uniform(-0.01, 0.01)))
        pixels.append(row)
    return pixels


def make_windsock(width, height):
    rng = random.Random(4)
    orange = (235, 105, 30)
    white = (240, 240, 240)

    pixels = []
    for y in range(height):
        band = y * 5 // height
        color = orange if band % 2 == 0 else white
        row = [shade(color, 1.0 + rng.uniform(-0.02, 0.02)) for _ in range(width)]
        pixels.append(row)
    return pixels


def make_lamp(size):
    return [[(110, 20, 20)] * size for _ in range(size)]


def save(name, width, height, pixels):
    path = os.path.join(OUT_DIR, name)
    write_png(path, width, height, pixels)
    print("Saved:", os.path.normpath(path))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    save("grass.png", 256, 256, make_grass(256))
    save("metal.png", 256, 256, make_metal(256))
    save("blade.png", 64, 256, make_blade(64, 256))
    save("windsock.png", 64, 160, make_windsock(64, 160))
    save("lamp.png", 8, 8, make_lamp(8))


if __name__ == "__main__":
    main()
