/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_INTERNAL_LOGGER_H
#define LMSHAO_LMMKV_INTERNAL_LOGGER_H

#include <mutex>

#include "lmmkv/lmmkv_logger.h"

namespace lmshao::lmmkv {

inline lmshao::lmcore::Logger &GetLmmkvLoggerWithAutoInit()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        lmshao::lmcore::LoggerRegistry::RegisterModule<LmmkvModuleTag>("LMMKV");
        InitLmmkvLogger();
    });
    return lmshao::lmcore::LoggerRegistry::GetLogger<LmmkvModuleTag>();
}

#define LMMKV_LOGD(fmt, ...)                                                                                           \
    do {                                                                                                               \
        auto &logger = lmshao::lmmkv::GetLmmkvLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kDebug)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmmkv::LmmkvModuleTag>(lmshao::lmcore::LogLevel::kDebug, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define LMMKV_LOGI(fmt, ...)                                                                                           \
    do {                                                                                                               \
        auto &logger = lmshao::lmmkv::GetLmmkvLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kInfo)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmmkv::LmmkvModuleTag>(lmshao::lmcore::LogLevel::kInfo, __FILE__,          \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define LMMKV_LOGW(fmt, ...)                                                                                           \
    do {                                                                                                               \
        auto &logger = lmshao::lmmkv::GetLmmkvLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kWarn)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmmkv::LmmkvModuleTag>(lmshao::lmcore::LogLevel::kWarn, __FILE__,          \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define LMMKV_LOGE(fmt, ...)                                                                                           \
    do {                                                                                                               \
        auto &logger = lmshao::lmmkv::GetLmmkvLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kError)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmmkv::LmmkvModuleTag>(lmshao::lmcore::LogLevel::kError, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define LMMKV_LOGF(fmt, ...)                                                                                           \
    do {                                                                                                               \
        auto &logger = lmshao::lmmkv::GetLmmkvLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kFatal)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmmkv::LmmkvModuleTag>(lmshao::lmcore::LogLevel::kFatal, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_INTERNAL_LOGGER_H