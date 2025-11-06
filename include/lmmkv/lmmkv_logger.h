/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_LMMKV_LOGGER_H
#define LMSHAO_LMMKV_LMMKV_LOGGER_H

#include <lmcore/logger.h>

namespace lmshao::lmmkv {

// Module tag for Lmmkv
struct LmmkvModuleTag {};

/**
 * @brief Initialize Lmmkv logger with specified settings
 */
inline void InitLmmkvLogger(lmcore::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                                lmcore::LogLevel::kDebug,
#else
                                lmcore::LogLevel::kWarn,
#endif
                            lmcore::LogOutput output = lmcore::LogOutput::CONSOLE, const std::string &filename = "")
{
    lmcore::LoggerRegistry::RegisterModule<LmmkvModuleTag>("LMMKV");
    lmcore::LoggerRegistry::InitLogger<LmmkvModuleTag>(level, output, filename);
}

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_LMMKV_LOGGER_H