import sys
import os

def bin_to_h(bin_filepath, h_filepath, var_name, width, height):
    try:
        with open(bin_filepath, 'rb') as f_bin:
            data = f_bin.read()

        if len(data) != width * height * 2: # 2 bytes per pixel for RGB565
            print(f"Warning: Binary file size ({len(data)} bytes) does not match expected size ({width * height * 2} bytes) for {width}x{height} image.", file=sys.stderr)

        with open(h_filepath, 'w') as f_h:
            f_h.write(f"#ifndef {var_name.upper()}_H\n")
            f_h.write(f"#define {var_name.upper()}_H\n\n")
            f_h.write(f"#include <stdint.h>\n") # Include stdint for uint16_t
            f_h.write(f"#include <pgmspace.h>\n\n") # Include pgmspace for PROGMEM
            f_h.write(f"const unsigned int {var_name}_width = {width};\n")
            f_h.write(f"const unsigned int {var_name}_height = {height};\n")
            f_h.write(f"const uint16_t {var_name}_pixels[] PROGMEM = {{\n")

            for i in range(0, len(data), 2):
                # Read two bytes, combine into uint16_t
                # Assuming little-endian for the 16-bit word as per previous magick convert
                pixel_value = int.from_bytes(data[i:i+2], byteorder='little')
                f_h.write(f"0x{pixel_value:04X}, ")
                if (i // 2 + 1) % 16 == 0: # 16 pixels per line for readability
                    f_h.write("\n")
            f_h.write("\n};

")
            f_h.write(f"#endif // {var_name.upper()}_H\n")
        print(f"Successfully converted '{bin_filepath}' to '{h_filepath}'")
    except Exception as e:
        print(f"Error converting {bin_filepath}: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 6:
        print("Usage: python convert_bin_to_h.py <input_bin_file> <output_h_file> <variable_name> <width> <height>")
        sys.exit(1)

    input_bin = sys.argv[1]
    output_h = sys.argv[2]
    var_name = sys.argv[3]
    width = int(sys.argv[4])
    height = int(sys.argv[5])

    bin_to_h(input_bin, output_h, var_name, width, height)
