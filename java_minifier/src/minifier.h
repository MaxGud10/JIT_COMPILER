#pragma once

#include <istream>
#include <ostream>

struct MinifyOptions {
    bool aggressive = false;
};

void minify(std::istream& in,
            std::ostream& out,
            const MinifyOptions& opts = {});