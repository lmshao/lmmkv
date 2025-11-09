/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_EBML_READER_H
#define LMSHAO_LMMKV_EBML_READER_H

#include <cstddef>
#include <cstdint>

namespace lmshao::lmmkv {

struct EbmlElementHeader {
    uint64_t id;
    uint64_t size;
};

// Buffer-only cursor for sequential reading over memory
struct BufferCursor {
    const uint8_t *data_;
    size_t size_;
    size_t pos_;
    BufferCursor(const uint8_t *d, size_t s) : data_(d), size_(s), pos_(0) {}
    size_t Read(uint8_t *dst, size_t n)
    {
        size_t remain = (pos_ < size_) ? (size_ - pos_) : 0;
        size_t to_read = n < remain ? n : remain;
        if (to_read > 0) {
            for (size_t i = 0; i < to_read; ++i)
                dst[i] = data_[pos_ + i];
            pos_ += to_read;
        }
        return to_read;
    }
    bool Seek(size_t offset)
    {
        if (offset > size_)
            return false;
        pos_ = offset;
        return true;
    }
    size_t Tell() const { return pos_; }
};

// Read EBML varint for element ID; keeps leading 1-bit.
size_t ReadVintId(BufferCursor &cur, uint64_t &value);

// Read EBML varint for element size; strips leading 1-bit.
size_t ReadVintSize(BufferCursor &cur, uint64_t &value);

// Parse next element header from current position.
bool NextElement(BufferCursor &cur, EbmlElementHeader &out);

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_EBML_READER_H