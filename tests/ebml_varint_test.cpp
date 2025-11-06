/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <cassert>
#include <cstdint>
#include <vector>

#include "lmmkv/ebml_reader.h"

using namespace lmshao::lmmkv;

int main()
{
    // Test ReadVintId with 1-byte ID 0x1A (leading 1 in bit7)
    {
        std::vector<uint8_t> bytes = {0x81};
        BufferCursor in(bytes.data(), bytes.size());
        uint64_t v = 0;
        size_t w = ReadVintId(in, v);
        assert(w == 1);
        assert(v == 0x81);
    }

    // Test ReadVintSize with 1-byte size: 0x81 -> 0x01
    {
        std::vector<uint8_t> bytes = {0x81};
        BufferCursor in(bytes.data(), bytes.size());
        uint64_t v = 0;
        size_t w = ReadVintSize(in, v);
        assert(w == 1);
        assert(v == 0x01);
    }

    // Test multi-byte size: 0x40 0x7F -> width=2, first byte masked to 0x00, value becomes 0x007F
    {
        std::vector<uint8_t> bytes = {0x40, 0x7F};
        BufferCursor in(bytes.data(), bytes.size());
        uint64_t v = 0;
        size_t w = ReadVintSize(in, v);
        assert(w == 2);
        assert(v == 0x007F);
    }

    // Test multi-byte size with non-zero masked high bits: 0x5F 0x01 -> masked first is 0x1F, result 0x1F01
    {
        std::vector<uint8_t> bytes = {0x5F, 0x01};
        BufferCursor in(bytes.data(), bytes.size());
        uint64_t v = 0;
        size_t w = ReadVintSize(in, v);
        assert(w == 2);
        assert(v == 0x1F01);
    }

    return 0;
}