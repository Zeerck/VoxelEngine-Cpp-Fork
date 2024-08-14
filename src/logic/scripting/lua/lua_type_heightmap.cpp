#include "lua_custom_types.hpp"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include "util/functional_util.hpp"
#define FNL_IMPL
#include "maths/FastNoiseLite.h"
#include "coders/png.hpp"
#include "files/util.hpp"
#include "graphics/core/ImageData.hpp"
#include "lua_util.hpp"

using namespace lua;

static fnl_state noise = fnlCreateState();

LuaHeightmap::~LuaHeightmap() {;
}

static int l_dump(lua::State* L) {
    if (auto heightmap = touserdata<LuaHeightmap>(L, 1)) {
        auto filename = tostring(L, 2);
        if (!files::is_valid_name(filename)) {
            throw std::runtime_error("invalid file name");
        }
        uint w = heightmap->getWidth();
        uint h = heightmap->getHeight();
        ImageData image(ImageFormat::rgb888, w, h);
        auto heights = heightmap->getValues();
        auto raster = image.getData();
        for (uint y = 0; y < h; y++) {
            for (uint x = 0; x < w; x++) {
                uint i = y * w + x;
                int val = heights[i] * 127 + 128;
                val = std::max(std::min(val, 255), 0);
                raster[i*3] = val;
                raster[i*3 + 1] = val;
                raster[i*3 + 2] = val;
            }
        }
        png::write_image(filename, &image);
    }
    return 0;
}

static int l_noise(lua::State* L) {
    if (auto heightmap = touserdata<LuaHeightmap>(L, 1)) {
        uint w = heightmap->getWidth();
        uint h = heightmap->getHeight();
        auto heights = heightmap->getValues();

        auto offset = tovec<2>(L, 2);

        float s = tonumber(L, 3);
        int octaves = 1;
        float multiplier = 1.0f;
        if (gettop(L) > 3) {
            octaves = tointeger(L, 4);
        }
        if (gettop(L) > 4) {
            multiplier = tonumber(L, 5);
        }
        const LuaHeightmap* shiftMapX = nullptr;
        const LuaHeightmap* shiftMapY = nullptr;
        if (gettop(L) > 5) {
            shiftMapX = touserdata<LuaHeightmap>(L, 6);
        }
        if (gettop(L) > 6) {
            shiftMapY = touserdata<LuaHeightmap>(L, 7);
        }
        for (uint y = 0; y < h; y++) {
            for (uint x = 0; x < w; x++) {
                uint i = y * w + x;
                for (uint c = 0; c < octaves; c++) {
                    float m = s * (1 << c);
                    float value = heights[i];
                    float u = (x + offset.x) * m;
                    float v = (y + offset.y) * m;
                    if (shiftMapX) {
                        u += shiftMapX->getValues()[i];
                    }
                    if (shiftMapY) {
                        v += shiftMapY->getValues()[i];
                    }

                    value += fnlGetNoise2D(&noise, u, v) /
                            static_cast<float>(1 << c) * multiplier;
                    heights[i] = value;
                }
            }
        }
    }
    return 0;
}

template<template<class> class Op>
static int l_binop_func(lua::State* L) {
    Op<float> op;
    if (auto heightmap = touserdata<LuaHeightmap>(L, 1)) {
        uint w = heightmap->getWidth();
        uint h = heightmap->getHeight();
        auto heights = heightmap->getValues();

        if (isnumber(L, 2)) {
            float scalar = tonumber(L, 2);
            for (uint y = 0; y < h; y++) {
                for (uint x = 0; x < w; x++) {
                    uint i = y * w + x;
                    heights[i] = op(heights[i], scalar);
                }
            }
        } else {
            auto map = touserdata<Heightmap>(L, 2);
            for (uint y = 0; y < h; y++) {
                for (uint x = 0; x < w; x++) {
                    uint i = y * w + x;
                    heights[i] = op(heights[i], map->getValues()[i]);
                }
            }
        }
    }
    return 0;
}

template<template<class> class Op>
static int l_unaryop_func(lua::State* L) {
    Op<float> op;
    if (auto heightmap = touserdata<LuaHeightmap>(L, 1)) {
        uint w = heightmap->getWidth();
        uint h = heightmap->getHeight();
        auto heights = heightmap->getValues();
        float power = tonumber(L, 2);
        for (uint y = 0; y < h; y++) {
            for (uint x = 0; x < w; x++) {
                uint i = y * w + x;
                heights[i] = op(heights[i]);
            }
        }
    }
    return 0;
}

static std::unordered_map<std::string, lua_CFunction> methods {
    {"dump", lua::wrap<l_dump>},
    {"noise", lua::wrap<l_noise>},
    {"pow", lua::wrap<l_binop_func<util::pow>>},
    {"add", lua::wrap<l_binop_func<std::plus>>},
    {"mul", lua::wrap<l_binop_func<std::multiplies>>},
    {"min", lua::wrap<l_binop_func<util::min>>},
    {"max", lua::wrap<l_binop_func<util::max>>},
    {"abs", lua::wrap<l_unaryop_func<util::abs>>},
};

static int l_meta_meta_call(lua::State* L) {
    auto width = tointeger(L, 2);
    auto height = tointeger(L, 3);
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("width and height must be greather than 0");
    }
    return newuserdata<LuaHeightmap>(
        L, static_cast<uint>(width), static_cast<uint>(height)
    );
}

static int l_meta_index(lua::State* L) {
    auto map = touserdata<LuaHeightmap>(L, 1);
    if (map == nullptr) {
        return 0;
    }
    if (isstring(L, 2)) {
        auto fieldname = tostring(L, 2);
        if (!std::strcmp(fieldname, "width")) {
            return pushinteger(L, map->getWidth());
        } else if (!std::strcmp(fieldname, "height")) {
            return pushinteger(L, map->getHeight());
        } else {
            auto found = methods.find(tostring(L, 2));
            if (found != methods.end()) {
                return pushcfunction(L, found->second);
            }
        }
    }
    return 0;
}

static int l_meta_tostring(lua::State* L) {
    auto map = touserdata<LuaHeightmap>(L, 1);
    if (map == nullptr) {
        return 0;
    }

    std::stringstream stream;
    stream << std::hex << reinterpret_cast<ptrdiff_t>(map);
    auto ptrstr = stream.str();

    return pushstring(
        L, "Heightmap(" + std::to_string(map->getWidth()) + 
           "*" + std::to_string(map->getHeight()) + " at 0x" + ptrstr + ")"
    );
}

int LuaHeightmap::createMetatable(lua::State* L) {
    createtable(L, 0, 2);
    pushcfunction(L, lua::wrap<l_meta_tostring>);
    setfield(L, "__tostring");
    pushcfunction(L, lua::wrap<l_meta_index>);
    setfield(L, "__index");

    createtable(L, 0, 1);
    pushcfunction(L, lua::wrap<l_meta_meta_call>);
    setfield(L, "__call");
    setmetatable(L);
    return 1;
}
