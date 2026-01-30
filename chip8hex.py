# Your Chip-8 hex data (0x prefix will be removed)
hex_data = "0x02 0x07 0x08 0x0F 0x43 0x3C 0xF9 0x08 0x0D 0xF9 0x07 0x04 0x17 0xFF"

# Remove 0x prefixes and convert to bytes
hex_cleaned = hex_data.replace("0x", "").replace(",", "").replace("\n", " ")
byte_values = bytes.fromhex(hex_cleaned)

# Write binary Chip-8 ROM
with open("my_chip8_program.ch8", "wb") as f:
    f.write(byte_values)

print("✅ Chip-8 ROM created: my_chip8_program.ch8")
