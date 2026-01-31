import os

def gen_header():
    roms_dir = 'chip8/roms'
    header_file = 'chip8/Roms.h'
    
    if not os.path.exists(roms_dir):
        os.makedirs(roms_dir)
        
    files = [f for f in os.listdir(roms_dir) if f.endswith('.ch8')]
    
    import re

    with open(header_file, 'w') as f:
        f.write('#pragma once\n')
        f.write('#include <cstdint>\n\n')
        
        rom_names = []
        for filename in files:
            # Sanitize name: remove non-alphanumeric characters for C++ variable compatibility
            clean_name = re.sub(r'[^a-zA-Z0-9]', '_', os.path.splitext(filename)[0]).upper()
            rom_names.append((clean_name, filename))
            with open(os.path.join(roms_dir, filename), 'rb') as rf:
                data = rf.read()
                f.write(f'static const uint8_t ROM_{clean_name}[] = {{\n    ')
                f.write(', '.join(f'0x{b:02X}' for b in data))
                f.write('\n};\n\n')
        
        # Add fallback ROMs if directory was empty or to ensure IBM is there
        if not files:
            # Add IBM manually as fallback
            f.write('static const uint8_t ROM_IBM[] = {\n')
            f.write('    0x00, 0xE0, 0xA2, 0x22, 0x60, 0x0C, 0x61, 0x08, 0xD0, 0x1F, 0x70, 0x09, 0xA2, 0x2D, 0xD0, 0x1F,\n')
            f.write('    0xA2, 0x38, 0x70, 0x08, 0xD0, 0x1F, 0x70, 0x04, 0xA2, 0x43, 0xD0, 0x1F, 0x70, 0x08, 0xA2, 0x4E,\n')
            f.write('    0xD0, 0x1F, 0xA2, 0x59, 0x70, 0x08, 0xD0, 0x1F, 0x12, 0x10, 0xFF, 0x00, 0xFF, 0x00, 0x3C, 0x00,\n')
            f.write('    0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x3C, 0x00,\n')
            f.write('    0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x33, 0x00, 0x33, 0x00, 0x33, 0x00, 0x33, 0x00, 0x33, 0x00,\n')
            f.write('    0x33, 0x00, 0x33, 0x00, 0x33, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x33, 0x00, 0x33, 0x00, 0x33, 0x00,\n')
            f.write('    0x33, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x33, 0x00, 0x33, 0x00,\n')
            f.write('    0x33, 0x00, 0x33, 0x00, 0xFF, 0x00, 0xFF, 0x00\n};\n\n')
            rom_names.append(('IBM', 'ibm.ch8'))

        f.write('struct RomEntry { const char* name; const uint8_t* data; size_t size; };\n\n')
        f.write('static const RomEntry ROM_LIST[] = {\n')
        for name, filename in rom_names:
            f.write(f'    {{"{name}", ROM_{name}, sizeof(ROM_{name})}},\n')
        f.write('};\n')

if __name__ == "__main__":
    gen_header()
