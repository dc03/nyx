#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COLORED_PRINT_HELPER_HPP
#define COLORED_PRINT_HELPER_HPP

#include <iostream>

struct ColoredPrintHelper {
    using StreamColorModifier = std::ostream &(*)(std::ostream &out);
    bool colors_enabled{};
    StreamColorModifier colorizer{};

    friend std::ostream &operator<<(std::ostream &out, const ColoredPrintHelper &helper) {
        if (helper.colors_enabled) {
            return out << helper.colorizer;
        } else {
            return out;
        }
    }
};

#endif