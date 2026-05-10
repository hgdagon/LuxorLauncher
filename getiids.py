import sys
import pefile
import xxhash
from pathlib import Path

NAMES = {
    0x581E9069: "Luxor",
    0xC7FFCD99: "Luxor (French)",
    0x1E1267D4: "Luxor (German)",
    0x8BC5255E: "Luxor (Swedish)",
    0x2C96A170: "Luxor 2",
    0x21C5A0C7: "Luxor 2 (French)",
    0x9CBF6C9E: "Luxor 2 (German)",
    0xADACD3ED: "Luxor 3",
    0x50E7AE4D: "Luxor 4",
    0x144CE8BA: "Luxor 5",
    0x044E7C8C: "Luxor Amun Rising",
    0x001D343A: "Luxor Amun Rising (French)",
    0xDEA924D6: "Luxor Amun Rising (German)",
    0x0A0EFEAB: "Luxor Amun Rising (Swedish)",
    0x6393E8A9: "Luxor Mahjong",
    0x0FE73EA3: "Luxor Mahjong (French)",
    0x7D950903: "Luxor Mahjong (German)",
    0x9E5E428D: "Luxor Mahjong (Italian)",
    0x58A3AA74: "Luxor Mahjong (Spanish)",
    0x1F0A77D7: "Luxor Mahjong (Swedish)"
}

def get_iid(file_path):
    pe = pefile.PE(file_path)
    xxh32 = xxhash.xxh32((file_path.parent /'game.dmg').read_bytes()).intdigest()

    global seen
    if xxh32 in seen:
        return
    seen.add(xxh32)

    for resource_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if resource_type.id != 6: # 'RT_STRING'
            continue

        for resource_id in resource_type.directory.entries:
                data_entry = resource_id.directory.entries[-1].data
                if data_entry.struct.Size < 105:
                    continue
                data = pe.get_data(data_entry.struct.OffsetToData, data_entry.struct.Size)

                print(f'{NAMES.get(xxh32)} 0x{xxh32:08X} {data.decode('utf16').strip('\0')[1:]}')

if __name__ == "__main__":
    test_path = Path(r"C:\Program Files (x86)\Steam\steamapps\content")
    in_path = Path(sys.argv[1]) if len(sys.argv) > 1 else test_path

    seen = set()

    if in_path.is_dir():
        for exe in test_path.rglob('*.exe'):
            if exe.stem.endswith('_unpacked'): # No (good) UPX in Python, must unpack manually
                get_iid(exe)
    elif in_path.is_file():
        get_iid(in_path)
