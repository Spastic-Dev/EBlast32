// map_format.cpp
// Implementation of a universal text-based map format for an EDuke32 fork.
// This supports loading and saving Build engine maps in a text-based format inspired by UDMF.
// Universal means it handles versions 7,8,9 with extensions.
// Additional feature: Support for custom "author" and "description" fields in header.
// Text format example:
// namespace = "Build";
// version = 7;
// author = "Example Author";
// description = "Test map";
// posx = 0;
// posy = 0;
// posz = 0;
// ang = 0;
// cursectnum = 0;
// sector
// {
//   wallptr = 0;
//   wallnum = 4;
//   ceilingz = 0;
//   // ... all sector fields
// }
// // Similar for wall and sprite blocks.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

// Define data types for clarity
using int32 = int32_t;
using uint32 = uint32_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int8 = int8_t;
using uint8 = uint8_t;

// Sector structure (40 bytes in binary)
struct Sector {
    int16 wallptr;
    int16 wallnum;
    int32 ceilingz;
    int32 floorz;
    int16 ceilingstat;
    int16 floorstat;
    int16 ceilingpicnum;
    int16 ceilingheinum;
    int8 ceilingshade;
    uint8 ceilingpal;
    uint8 ceilingxpanning;
    uint8 ceilingypanning;
    int16 floorpicnum;
    int16 floorheinum;
    int8 floorshade;
    uint8 floorpal;
    uint8 floorxpanning;
    uint8 floorypanning;
    uint8 visibility;
    uint8 filler; // padding
    int16 lotag;
    int16 hitag;
    int16 extra;
};

// Wall structure (32 bytes)
struct Wall {
    int32 x;
    int32 y;
    int16 point2;
    int16 nextwall;
    int16 nextsector;
    int16 cstat;
    int16 picnum;
    int16 overpicnum;
    int8 shade;
    uint8 pal;
    uint8 xrepeat;
    uint8 yrepeat;
    uint8 xpanning;
    uint8 ypanning;
    int16 lotag;
    int16 hitag;
    int16 extra;
};

// Sprite structure (44 bytes)
struct Sprite {
    int32 x;
    int32 y;
    int32 z;
    int16 cstat;
    int16 picnum;
    int8 shade;
    uint8 pal;
    uint8 clipdist;
    uint8 filler; // padding
    uint8 xrepeat;
    uint8 yrepeat;
    int8 xoffset;
    int8 yoffset;
    int16 sectnum;
    int16 statnum;
    int16 ang;
    int16 owner;
    int16 xvel;
    int16 yvel;
    int16 zvel;
    int16 lotag;
    int16 hitag;
    int16 extra;
};

// Map data container
struct BuildMap {
    int32 version;
    int32 posx;
    int32 posy;
    int32 posz;
    int16 ang;
    int16 cursectnum;
    std::vector<Sector> sectors;
    std::vector<Wall> walls;
    std::vector<Sprite> sprites;
    // Additional features
    std::string author;
    std::string description;
    std::string namespace_ = "Build"; // Default
};

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

// Parse a line: key = value;
std::pair<std::string, std::string> parse_line(const std::string& line) {
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) return {"", ""};
    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));
    // Remove trailing semicolon
    if (!value.empty() && value.back() == ';') value.pop_back();
    value = trim(value);
    return {key, value};
}

// Load map from text file
bool load_text_map(const std::string& filename, BuildMap& map) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }

    std::string line;
    std::string current_block;
    Sector current_sector;
    Wall current_wall;
    Sprite current_sprite;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line == "{") continue;
        if (line == "}") {
            if (current_block == "sector") {
                map.sectors.push_back(current_sector);
            } else if (current_block == "wall") {
                map.walls.push_back(current_wall);
            } else if (current_block == "sprite") {
                map.sprites.push_back(current_sprite);
            }
            current_block.clear();
            continue;
        }

        if (line.find('=') == std::string::npos) {
            // New block
            current_block = line;
            continue;
        }

        auto [key, value] = parse_line(line);
        if (key.empty()) continue;

        std::istringstream iss(value);

        if (current_block.empty()) {
            // Global fields
            if (key == "namespace") map.namespace_ = value;
            else if (key == "version") iss >> map.version;
            else if (key == "posx") iss >> map.posx;
            else if (key == "posy") iss >> map.posy;
            else if (key == "posz") iss >> map.posz;
            else if (key == "ang") iss >> map.ang;
            else if (key == "cursectnum") iss >> map.cursectnum;
            else if (key == "author") map.author = value;
            else if (key == "description") map.description = value;
            // Ignore unknown for universality
        } else if (current_block == "sector") {
            if (key == "wallptr") iss >> current_sector.wallptr;
            else if (key == "wallnum") iss >> current_sector.wallnum;
            else if (key == "ceilingz") iss >> current_sector.ceilingz;
            else if (key == "floorz") iss >> current_sector.floorz;
            else if (key == "ceilingstat") iss >> current_sector.ceilingstat;
            else if (key == "floorstat") iss >> current_sector.floorstat;
            else if (key == "ceilingpicnum") iss >> current_sector.ceilingpicnum;
            else if (key == "ceilingheinum") iss >> current_sector.ceilingheinum;
            else if (key == "ceilingshade") iss >> current_sector.ceilingshade;
            else if (key == "ceilingpal") iss >> current_sector.ceilingpal;
            else if (key == "ceilingxpanning") iss >> current_sector.ceilingxpanning;
            else if (key == "ceilingypanning") iss >> current_sector.ceilingypanning;
            else if (key == "floorpicnum") iss >> current_sector.floorpicnum;
            else if (key == "floorheinum") iss >> current_sector.floorheinum;
            else if (key == "floorshade") iss >> current_sector.floorshade;
            else if (key == "floorpal") iss >> current_sector.floorpal;
            else if (key == "floorxpanning") iss >> current_sector.floorxpanning;
            else if (key == "floorypanning") iss >> current_sector.floorypanning;
            else if (key == "visibility") iss >> current_sector.visibility;
            else if (key == "filler") iss >> current_sector.filler;
            else if (key == "lotag") iss >> current_sector.lotag;
            else if (key == "hitag") iss >> current_sector.hitag;
            else if (key == "extra") iss >> current_sector.extra;
            // Ignore unknown
        } else if (current_block == "wall") {
            if (key == "x") iss >> current_wall.x;
            else if (key == "y") iss >> current_wall.y;
            else if (key == "point2") iss >> current_wall.point2;
            else if (key == "nextwall") iss >> current_wall.nextwall;
            else if (key == "nextsector") iss >> current_wall.nextsector;
            else if (key == "cstat") iss >> current_wall.cstat;
            else if (key == "picnum") iss >> current_wall.picnum;
            else if (key == "overpicnum") iss >> current_wall.overpicnum;
            else if (key == "shade") iss >> current_wall.shade;
            else if (key == "pal") iss >> current_wall.pal;
            else if (key == "xrepeat") iss >> current_wall.xrepeat;
            else if (key == "yrepeat") iss >> current_wall.yrepeat;
            else if (key == "xpanning") iss >> current_wall.xpanning;
            else if (key == "ypanning") iss >> current_wall.ypanning;
            else if (key == "lotag") iss >> current_wall.lotag;
            else if (key == "hitag") iss >> current_wall.hitag;
            else if (key == "extra") iss >> current_wall.extra;
            // Ignore unknown
        } else if (current_block == "sprite") {
            if (key == "x") iss >> current_sprite.x;
            else if (key == "y") iss >> current_sprite.y;
            else if (key == "z") iss >> current_sprite.z;
            else if (key == "cstat") iss >> current_sprite.cstat;
            else if (key == "picnum") iss >> current_sprite.picnum;
            else if (key == "shade") iss >> current_sprite.shade;
            else if (key == "pal") iss >> current_sprite.pal;
            else if (key == "clipdist") iss >> current_sprite.clipdist;
            else if (key == "filler") iss >> current_sprite.filler;
            else if (key == "xrepeat") iss >> current_sprite.xrepeat;
            else if (key == "yrepeat") iss >> current_sprite.yrepeat;
            else if (key == "xoffset") iss >> current_sprite.xoffset;
            else if (key == "yoffset") iss >> current_sprite.yoffset;
            else if (key == "sectnum") iss >> current_sprite.sectnum;
            else if (key == "statnum") iss >> current_sprite.statnum;
            else if (key == "ang") iss >> current_sprite.ang;
            else if (key == "owner") iss >> current_sprite.owner;
            else if (key == "xvel") iss >> current_sprite.xvel;
            else if (key == "yvel") iss >> current_sprite.yvel;
            else if (key == "zvel") iss >> current_sprite.zvel;
            else if (key == "lotag") iss >> current_sprite.lotag;
            else if (key == "hitag") iss >> current_sprite.hitag;
            else if (key == "extra") iss >> current_sprite.extra;
            // Ignore unknown
        }
    }

    return true;
}

// Save map to text file
bool save_text_map(const std::string& filename, const BuildMap& map) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }

    file << "namespace = \"" << map.namespace_ << "\";\n";
    file << "version = " << map.version << ";\n";
    if (!map.author.empty()) file << "author = \"" << map.author << "\";\n";
    if (!map.description.empty()) file << "description = \"" << map.description << "\";\n";
    file << "posx = " << map.posx << ";\n";
    file << "posy = " << map.posy << ";\n";
    file << "posz = " << map.posz << ";\n";
    file << "ang = " << map.ang << ";\n";
    file << "cursectnum = " << map.cursectnum << ";\n\n";

    for (const auto& sec : map.sectors) {
        file << "sector\n{\n";
        file << "  wallptr = " << sec.wallptr << ";\n";
        file << "  wallnum = " << sec.wallnum << ";\n";
        file << "  ceilingz = " << sec.ceilingz << ";\n";
        file << "  floorz = " << sec.floorz << ";\n";
        file << "  ceilingstat = " << sec.ceilingstat << ";\n";
        file << "  floorstat = " << sec.floorstat << ";\n";
        file << "  ceilingpicnum = " << sec.ceilingpicnum << ";\n";
        file << "  ceilingheinum = " << sec.ceilingheinum << ";\n";
        file << "  ceilingshade = " << static_cast<int>(sec.ceilingshade) << ";\n";
        file << "  ceilingpal = " << static_cast<unsigned>(sec.ceilingpal) << ";\n";
        file << "  ceilingxpanning = " << static_cast<unsigned>(sec.ceilingxpanning) << ";\n";
        file << "  ceilingypanning = " << static_cast<unsigned>(sec.ceilingypanning) << ";\n";
        file << "  floorpicnum = " << sec.floorpicnum << ";\n";
        file << "  floorheinum = " << sec.floorheinum << ";\n";
        file << "  floorshade = " << static_cast<int>(sec.floorshade) << ";\n";
        file << "  floorpal = " << static_cast<unsigned>(sec.floorpal) << ";\n";
        file << "  floorxpanning = " << static_cast<unsigned>(sec.floorxpanning) << ";\n";
        file << "  floorypanning = " << static_cast<unsigned>(sec.floorypanning) << ";\n";
        file << "  visibility = " << static_cast<unsigned>(sec.visibility) << ";\n";
        file << "  filler = " << static_cast<unsigned>(sec.filler) << ";\n";
        file << "  lotag = " << sec.lotag << ";\n";
        file << "  hitag = " << sec.hitag << ";\n";
        file << "  extra = " << sec.extra << ";\n";
        file << "}\n\n";
    }

    for (const auto& w : map.walls) {
        file << "wall\n{\n";
        file << "  x = " << w.x << ";\n";
        file << "  y = " << w.y << ";\n";
        file << "  point2 = " << w.point2 << ";\n";
        file << "  nextwall = " << w.nextwall << ";\n";
        file << "  nextsector = " << w.nextsector << ";\n";
        file << "  cstat = " << w.cstat << ";\n";
        file << "  picnum = " << w.picnum << ";\n";
        file << "  overpicnum = " << w.overpicnum << ";\n";
        file << "  shade = " << static_cast<int>(w.shade) << ";\n";
        file << "  pal = " << static_cast<unsigned>(w.pal) << ";\n";
        file << "  xrepeat = " << static_cast<unsigned>(w.xrepeat) << ";\n";
        file << "  yrepeat = " << static_cast<unsigned>(w.yrepeat) << ";\n";
        file << "  xpanning = " << static_cast<unsigned>(w.xpanning) << ";\n";
        file << "  ypanning = " << static_cast<unsigned>(w.ypanning) << ";\n";
        file << "  lotag = " << w.lotag << ";\n";
        file << "  hitag = " << w.hitag << ";\n";
        file << "  extra = " << w.extra << ";\n";
        file << "}\n\n";
    }

    for (const auto& spr : map.sprites) {
        file << "sprite\n{\n";
        file << "  x = " << spr.x << ";\n";
        file << "  y = " << spr.y << ";\n";
        file << "  z = " << spr.z << ";\n";
        file << "  cstat = " << spr.cstat << ";\n";
        file << "  picnum = " << spr.picnum << ";\n";
        file << "  shade = " << static_cast<int>(spr.shade) << ";\n";
        file << "  pal = " << static_cast<unsigned>(spr.pal) << ";\n";
        file << "  clipdist = " << static_cast<unsigned>(spr.clipdist) << ";\n";
        file << "  filler = " << static_cast<unsigned>(spr.filler) << ";\n";
        file << "  xrepeat = " << static_cast<unsigned>(spr.xrepeat) << ";\n";
        file << "  yrepeat = " << static_cast<unsigned>(spr.yrepeat) << ";\n";
        file << "  xoffset = " << static_cast<int>(spr.xoffset) << ";\n";
        file << "  yoffset = " << static_cast<int>(spr.yoffset) << ";\n";
        file << "  sectnum = " << spr.sectnum << ";\n";
        file << "  statnum = " << spr.statnum << ";\n";
        file << "  ang = " << spr.ang << ";\n";
        file << "  owner = " << spr.owner << ";\n";
        file << "  xvel = " << spr.xvel << ";\n";
        file << "  yvel = " << spr.yvel << ";\n";
        file << "  zvel = " << spr.zvel << ";\n";
        file << "  lotag = " << spr.lotag << ";\n";
        file << "  hitag = " << spr.hitag << ";\n";
        file << "  extra = " << spr.extra << ";\n";
        file << "}\n\n";
    }

    return true;
}

// Example usage (integrate into EDuke32 fork as needed)
int main() {
    BuildMap map;
    // For testing: load and save
    if (load_text_map("test.tmap", map)) {
        save_text_map("output.tmap", map);
    }
    return 0;
}
