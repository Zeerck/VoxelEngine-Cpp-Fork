#include "coders/lua_parsing.hpp"

#include <gtest/gtest.h>

#include "coders/commons.hpp"
#include "io/io.hpp"
#include "io/devices/StdfsDevice.hpp"
#include "util/stringutil.hpp"

namespace fs = std::filesystem;

TEST(lua_parsing, Tokenizer) {
    io::set_device("res", std::make_shared<io::StdfsDevice>(fs::u8path("../../res")));
    auto filename = "res:scripts/stdlib.lua";
    auto source = io::read_string(filename);
    try {
        auto tokens = lua::tokenize(filename, util::str2wstr_utf8(source));
        for (const auto& token : tokens) {
            std::cout << (int)token.tag << " "
                      << util::quote(util::wstr2str_utf8(token.text))
                      << std::endl;
        }
    } catch (const parsing_error& err) {
        std::cerr << err.errorLog() << std::endl;
        throw err;
    }
}
