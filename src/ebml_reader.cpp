/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "ebml_reader.h"

#include <cstring>

#include "internal_logger.h"

namespace lmshao::lmmkv {

// EBML varint width detection: leading 1 bit mask across first byte
static inline int DetectVintWidth(uint8_t first)
{
    for (int i = 0; i < 8; ++i) {
        if (first & (0x80 >> i)) {
            return i + 1; // width in bytes
        }
    }
    return -1; // invalid
}

// BufferCursor versions
static inline size_t ReadBytes(BufferCursor &cur, uint8_t *dst, size_t n)
{
    size_t r = cur.Read(dst, n);
    if (r != n) {
        LMMKV_LOGE("Failed to read %zu bytes, got %zu", n, r);
    }
    return r;
}

size_t ReadVintId(BufferCursor &cur, uint64_t &value)
{
    uint8_t b0 = 0;
    if (ReadBytes(cur, &b0, 1) != 1) {
        return 0;
    }
    int width = DetectVintWidth(b0);
    if (width <= 0) {
        LMMKV_LOGE("Invalid EBML ID leading byte: 0x%02X", b0);
        return 0;
    }
    uint64_t v = b0;
    for (int i = 1; i < width; ++i) {
        uint8_t bi = 0;
        if (ReadBytes(cur, &bi, 1) != 1) {
            return 0;
        }
        v = (v << 8) | bi;
    }
    value = v;
    return static_cast<size_t>(width);
}

size_t ReadVintSize(BufferCursor &cur, uint64_t &value)
{
    uint8_t b0 = 0;
    if (ReadBytes(cur, &b0, 1) != 1) {
        return 0;
    }
    int width = DetectVintWidth(b0);
    if (width <= 0) {
        LMMKV_LOGE("Invalid EBML size leading byte: 0x%02X", b0);
        return 0;
    }
    // strip leading 1-bit
    uint64_t v = static_cast<uint64_t>(b0 & (0xFF >> width));
    for (int i = 1; i < width; ++i) {
        uint8_t bi = 0;
        if (ReadBytes(cur, &bi, 1) != 1) {
            return 0;
        }
        v = (v << 8) | bi;
    }
    value = v;
    return static_cast<size_t>(width);
}

bool NextElement(BufferCursor &cur, EbmlElementHeader &out)
{
    uint64_t id = 0;
    size_t id_len = ReadVintId(cur, id);
    if (id_len == 0) {
        return false;
    }
    uint64_t size = 0;
    size_t size_len = ReadVintSize(cur, size);
    if (size_len == 0) {
        return false;
    }
    out.id = id;
    out.size = size;
    return true;
}

} // namespace lmshao::lmmkv