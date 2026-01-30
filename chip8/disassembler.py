import sys

# CHIP-8 programs start at memory address 0x200
START_ADDRESS = 0x200

def decode_opcode(opcode):
    nibbles = (
        (opcode & 0xF000) >> 12,
        (opcode & 0x0F00) >> 8,
        (opcode & 0x00F0) >> 4,
        (opcode & 0x000F)
    )

    nnn = opcode & 0x0FFF
    nn  = opcode & 0x00FF
    x   = nibbles[1]
    y   = nibbles[2]
    n   = nibbles[3]

    op = nibbles[0]

    match op:
        case 0x0:
            if opcode == 0x00E0: return "CLS"
            if opcode == 0x00EE: return "RET"
            return f"SYS {nnn:03X}"
        case 0x1: return f"JP {nnn:03X}"
        case 0x2: return f"CALL {nnn:03X}"
        case 0x3: return f"SE V{x:X}, {nn:02X}"
        case 0x4: return f"SNE V{x:X}, {nn:02X}"
        case 0x5: return f"SE V{x:X}, V{y:X}"
        case 0x6: return f"LD V{x:X}, {nn:02X}"
        case 0x7: return f"ADD V{x:X}, {nn:02X}"
        case 0x8:
            match n:
                case 0x0: return f"LD V{x:X}, V{y:X}"
                case 0x1: return f"OR V{x:X}, V{y:X}"
                case 0x2: return f"AND V{x:X}, V{y:X}"
                case 0x3: return f"XOR V{x:X}, V{y:X}"
                case 0x4: return f"ADD V{x:X}, V{y:X}"
                case 0x5: return f"SUB V{x:X}, V{y:X}"
                case 0x6: return f"SHR V{x:X}"
                case 0x7: return f"SUBN V{x:X}, V{y:X}"
                case 0xE: return f"SHL V{x:X}"
            return "UNKNOWN"
        case 0x9: return f"SNE V{x:X}, V{y:X}"
        case 0xA: return f"LD I, {nnn:03X}"
        case 0xB: return f"JP V0, {nnn:03X}"
        case 0xC: return f"RND V{x:X}, {nn:02X}"
        case 0xD: return f"DRW V{x:X}, V{y:X}, {n:X}"
        case 0xE:
            if nn == 0x9E: return f"SKP V{x:X}"
            if nn == 0xA1: return f"SKNP V{x:X}"
            return "UNKNOWN"
        case 0xF:
            match nn:
                case 0x07: return f"LD V{x:X}, DT"
                case 0x0A: return f"LD V{x:X}, K"
                case 0x15: return f"LD DT, V{x:X}"
                case 0x18: return f"LD ST, V{x:X}"
                case 0x1E: return f"ADD I, V{x:X}"
                case 0x29: return f"LD F, V{x:X}"
                case 0x33: return f"LD B, V{x:X}"
                case 0x55: return f"LD [I], V0~V{x:X}"
                case 0x65: return f"LD V0~V{x:X}, [I]"
            return "UNKNOWN"

    return "UNKNOWN"


def disassemble(filename):
    with open(filename, "rb") as f:
        rom = f.read()

    pc = START_ADDRESS

    print(f"Disassembling {filename}...\n")

    for i in range(0, len(rom), 2):
        opcode = rom[i] << 8 | rom[i+1]
        mnemonic = decode_opcode(opcode)
        print(f"{pc:03X}: {opcode:04X}  {mnemonic}")
        pc += 2


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python chip8_disassembler.py <romfile>")
        sys.exit(1)

    disassemble(sys.argv[1])
