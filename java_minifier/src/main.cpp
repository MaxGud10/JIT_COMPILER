#include "minifier.h"

#include <fstream>
#include <iostream>
#include <string>

static void print_usage(const char* prog)
{
    std::cerr
        << "Usage:\n"
        << "  " << prog << " [--aggressive] <input.java> [output.java]\n"
        << "  " << prog << " [--aggressive] < input.java > output.java\n";
}

int main(int argc, char** argv)
{
    MinifyOptions opts;

    int idx = 1;
    if (idx < argc && std::string(argv[idx]) == "--aggressive")
    {
        opts.aggressive = true;
        idx++;
    }

    if (idx >= argc)
    {
        minify(std::cin, std::cout, opts);
        return 0;
    }

    std::string inPath = argv[idx++];
    std::string outPath;
    bool hasOut = (idx < argc);
    if (hasOut) outPath = argv[idx++];

    if (idx < argc)
    {
        print_usage(argv[0]);
        return 2;
    }

    std::ifstream fin(inPath, std::ios::binary);
    if (!fin)
    {
        std::cerr << "Error: cannot open input file: " << inPath << "\n";
        return 1;
    }

    if (!hasOut)
    {
        minify(fin, std::cout, opts);
        return 0;
    }

    std::ofstream fout(outPath, std::ios::binary);
    if (!fout)
    {
        std::cerr << "Error: cannot open output file: " << outPath << "\n";
        return 1;
    }

    minify(fin, fout, opts);
    return 0;
}