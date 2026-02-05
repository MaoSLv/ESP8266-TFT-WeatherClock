from PIL import Image, ImageFont, ImageDraw

# size 的参数要与 tft.drawBitmap() 一致，否则会错位
def convert_transparent_png_to_bitmap(img_path, size=(32, 32)):
    img = Image.open(img_path).convert("RGBA")
    img = img.resize(size, Image.Resampling.LANCZOS)

    width, height = img.size
    print(f"// Bitmap data for {img_path} ({width}x{height})")
    print(f"const unsigned char icon_custom[] PROGMEM = {{")

    for y in range(height):
        line = "  "
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                if x + bit < width:
                    r, g, b, a = img.getpixel((x + bit, y))
                    # 关键逻辑：如果 Alpha 通道 > 128（即不透明度超过一半），则认为该点有像素
                    if a > 240:
                        byte |= (1 << (7 - bit))
            line += f"0x{byte:02x}, "
        print(line)

    print("};")

def generate_font_bitmap(text, font_path="C:/Windows/Fonts/simhei.ttf", font_size=24):
    # 加载字体（Windows自带黑体，Linux/Mac请换成对应的ttf路径）
    try:
        font = ImageFont.truetype(font_path, font_size)
    except:
        print("未找到字体文件，请检查路径")
        return

    for char in text:
        img = Image.new('1', (font_size, font_size), 0)
        draw = ImageDraw.Draw(img)

        draw.text((0, 0), char, font=font, fill=1)

        data = []
        for y in range(font_size):
            byte = 0
            for x in range(font_size):
                if img.getpixel((x, y)):
                    byte |= (1 << (7 - (x % 8)))
                if (x % 8 == 7):
                    data.append(f"0x{byte:02X}")
                    byte = 0

        print(f"// 汉字: {char} ({font_size}x{font_size})")
        print(f"const unsigned char hex_{char}[] PROGMEM = {{")
        print("  " + ", ".join(data))
        print("};\n")


def to_rgb565(r, g, b):
    # 将 0-255 映射到 5-6-5 位
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F

    # 组合成 16 位整数
    rgb565 = (r5 << 11) | (g6 << 5) | b5
    return f"0x{rgb565:04X}"


def hex_to_rgb565(hex_str):
    hex_str = hex_str.lstrip('#')
    r = int(hex_str[0:2], 16)
    g = int(hex_str[2:4], 16)
    b = int(hex_str[4:6], 16)
    return to_rgb565(r, g, b)

if __name__ == "__main__":
    # Hex值转
    print(f"RGB565: {hex_to_rgb565('#FF8025')}")
    # RGB转
    print(f"RGB565: {to_rgb565(16, 136, 255)}")

    # 生成“星期一二三四五六日”
    generate_font_bitmap("星期一二三四五六日")

    # 使用：把你的文件名填在这里
    # convert_transparent_png_to_bitmap("龙卷风.png")