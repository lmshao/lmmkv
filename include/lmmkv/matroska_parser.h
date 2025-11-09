/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_MATROSKA_PARSER_H
#define LMSHAO_LMMKV_MATROSKA_PARSER_H

#include <cstddef>
#include <cstdint>

namespace lmshao::lmmkv {

struct MatroskaInfo {
    uint64_t timecode_scale_ns;
    double duration_seconds;
    MatroskaInfo() : timecode_scale_ns(1000000), duration_seconds(0.0) {}
};

class MatroskaParser {
public:
    MatroskaParser() = default;
    // Parse from memory buffer without IO
    bool ParseBuffer(const uint8_t *data, size_t size, MatroskaInfo &info);
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_MATROSKA_PARSER_H