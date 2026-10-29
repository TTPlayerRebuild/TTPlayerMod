// Bounded Unreal package extraction for the legacy ModPlug reader.
// Follows the pinned libmodplug UMX loader's first-export and type rules.
#include <libmodplug/stdafx.h>
#include <libmodplug/sndfile.h>
#include <cstdint>
#include <cstring>

namespace {
struct Cursor {
    const BYTE* data;
    DWORD size, position;
    bool available(DWORD count) const {
        return position <= size && count <= size - position;
    }
    bool skip(DWORD count) {
        if (!available(count)) return false;
        position += count;
        return true;
    }
    bool byte(BYTE& value) {
        if (!available(1)) return false;
        value = data[position++];
        return true;
    }
    bool integer(LONG& value) {
        if (!available(4)) return false;
        const BYTE* p = data + position;
        value = LONG(DWORD(p[0]) | DWORD(p[1]) << 8 | DWORD(p[2]) << 16 | DWORD(p[3]) << 24);
        position += 4;
        return true;
    }
    bool compact(LONG& value) {
        BYTE part;
        if (!byte(part)) return false;
        const bool negative = (part & 0x80) != 0;
        DWORD magnitude = part & 0x3f;
        bool more = (part & 0x40) != 0;
        for (unsigned index = 1; more; ++index) {
            if (!byte(part)) return false;
            if (index == 4) {
                if (part & 0xe0) return false;
                magnitude |= DWORD(part) << 27;
                more = false;
            } else {
                magnitude |= DWORD(part & 0x7f) << (6 + 7 * (index - 1));
                more = (part & 0x80) != 0;
            }
        }
        if (magnitude > (negative ? 0x80000000u : 0x7fffffffu)) return false;
        value = negative ? LONG(-std::int64_t(magnitude)) : LONG(magnitude);
        return true;
    }
};

bool version_supported(LONG version) {
    switch (version) {
    case 35: case 37: case 40: case 41: case 61: case 62: case 63:
    case 64: case 66: case 68: case 69: case 83: return true;
    default: return false;
    }
}

bool type_name(const BYTE* data, DWORD size, LONG version, LONG offset,
               LONG count, LONG index, char (&name)[64]) {
    if (offset < 0 || index < 0 || index >= count) return false;
    Cursor names{data, size, DWORD(offset)};
    for (LONG current = 0; current <= index; ++current) {
        if (version >= 64) {
            BYTE length;
            if (!names.byte(length) || !length || length > sizeof(name) ||
                !names.available(length) || data[names.position + length - 1] != 0)
                return false;
            memcpy(name, data + names.position, length);
            if (!names.skip(length)) return false;
        } else {
            unsigned length = 0;
            BYTE part;
            do {
                if (length == sizeof(name) || !names.byte(part)) return false;
                name[length++] = char(part);
            } while (part);
        }
        if (!names.skip(4)) return false; // name flags
    }
    return true;
}

bool signature(const BYTE* data, DWORD size, DWORD at, const char* text, DWORD length) {
    return at <= size && length <= size - at && !memcmp(data + at, text, length);
}
}

BOOL CSoundFile::ReadUMX(const BYTE* data, DWORD length) {
    if (!data || length < 0x800 || length > 0x7fffffff) return FALSE;
    Cursor header{data, length, 0};
    LONG tag, version, flags, name_count, name_offset, export_count, export_offset,
         import_count, import_offset;
    if (!header.integer(tag) || DWORD(tag) != 0x9e2a83c1 ||
        !header.integer(version) || !version_supported(version) ||
        !header.integer(flags) || !header.integer(name_count) ||
        !header.integer(name_offset) || !header.integer(export_count) ||
        !header.integer(export_offset) || !header.integer(import_count) ||
        !header.integer(import_offset) || name_count <= 0 || name_offset < 0 ||
        export_count <= 0 || export_offset < 0 || import_count < 0 || import_offset < 0)
        return FALSE;

    // As in the pinned loader, extract the first exported music object.
    Cursor exports{data, length, DWORD(export_offset)};
    LONG ignored, serialized_size, serialized_offset;
    if (!exports.compact(ignored) || !exports.compact(ignored) ||
        (version >= 60 && !exports.skip(4)) || !exports.compact(ignored) ||
        !exports.skip(4) || !exports.compact(serialized_size) || serialized_size <= 0 ||
        !exports.compact(serialized_offset) || serialized_offset < 0)
        return FALSE;
    Cursor object{data, length, DWORD(serialized_offset)};
    if (!object.available(DWORD(serialized_size))) return FALSE;
    object.size = object.position + DWORD(serialized_size);
    LONG type, sample_size;
    if ((version < 40 && !object.skip(8)) || (version < 60 && !object.skip(16)) ||
        !object.compact(ignored) || !object.compact(type) ||
        (version > 61 && !object.skip(4)) || !object.compact(sample_size) ||
        sample_size <= 0 || !object.available(DWORD(sample_size)))
        return FALSE;
    char name[64]{};
    if (!type_name(data, length, version, name_offset, name_count, type, name)) return FALSE;
    const BYTE* sample = data + object.position;
    const DWORD size = DWORD(sample_size);
    if (!_stricmp(name, "IT"))
        return signature(sample, size, 0, "IMPM", 4) && ReadIT(sample, size);
    if (!_stricmp(name, "S3M")) {
        if (signature(sample, size, 44, "SCRM", 4)) return ReadS3M(sample, size);
        // Some Return to NaPali packages label their IT music as S3M.
        return signature(sample, size, 0, "IMPM", 4) && ReadIT(sample, size);
    }
    if (!_stricmp(name, "XM"))
        return signature(sample, size, 0, "Extended Module: ", 17) && size > 37 &&
            sample[37] == 0x1a && ReadXM(sample, size);
    if (!_stricmp(name, "MOD"))
        return (signature(sample, size, 1080, "M.K.", 4) ||
                signature(sample, size, 1080, "M!K!", 4)) && ReadMod(sample, size);
    return FALSE;
}
