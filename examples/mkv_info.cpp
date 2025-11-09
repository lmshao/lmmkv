/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <cstring>

#include "lmcore/mapped_file.h"
#include "lmmkv/matroska_parser.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <input.mkv>\n", argv[0]);
        return 1;
    }

    const std::string path = argv[1];
    auto mf = lmshao::lmcore::MappedFile::Open(path);
    if (!mf || !mf->IsValid()) {
        printf("Cannot open input file: %s", path.c_str());
        return 2;
    }

    lmshao::lmmkv::MatroskaParser parser;
    lmshao::lmmkv::MatroskaInfo info;
    const uint8_t *data = mf->Data();
    size_t size = mf->Size();
    if (!parser.ParseBuffer(data, size, info)) {
        printf("Parse failed for: %s", path.c_str());
        return 3;
    }

    printf("TimecodeScale(ns): %llu\n", (unsigned long long)info.timecode_scale_ns);
    printf("Duration(s): %.3f\n", info.duration_seconds);
    return 0;
}