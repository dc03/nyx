#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef FEATURE_FLAG_ERROR_HPP
#define FEATURE_FLAG_ERROR_HPP

#define FEATURE_FLAG(name, warn_msg, error_msg, where)                                                                 \
    if (ctx->config->contains(name)) {                                                                                 \
        std::string_view config = ctx->config->get<std::string>(name);                                                 \
                                                                                                                       \
        if (config == "warn") {                                                                                        \
            warning({warn_msg " [[ " name " = warn ]]"}, where);                                                       \
        } else if (config == "error") {                                                                                \
            error({error_msg " [[ " name " = error ]]"}, where);                                                       \
        }                                                                                                              \
    }

#define FEATURE_FLAG_DEFAULT_ERROR(name, warn_msg, error_msg, default_msg, where)                                      \
    FEATURE_FLAG(name, warn_msg, error_msg, where)                                                                     \
    else {                                                                                                             \
        error({default_msg, " [[ " name " = error (default) ]]"}, where);                                              \
    }

#define FEATURE_FLAG_DEFAULT_WARN(name, warn_msg, error_msg, default_msg, where)                                       \
    FEATURE_FLAG(name, warn_msg, error_msg, where)                                                                     \
    else {                                                                                                             \
        warning({default_msg, " [[ " name " = warn (default) ]]"}, where);                                             \
    }

#define FEATURE_FLAG_SINGLE_MSG(name, msg, where)               FEATURE_FLAG(name, msg, msg, where)
#define FEATURE_FLAG_DEFAULT_ERROR_SINGLE_MSG(name, msg, where) FEATURE_FLAG_DEFAULT_ERROR(name, msg, msg, msg, where)
#define FEATURE_FLAG_DEFAULT_WARN_SINGLE_MSG(name, msg, where)  FEATURE_FLAG_DEFAULT_WARN(name, msg, msg, msg, where)

#endif