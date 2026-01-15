// map_format.h
// Header for Universal/Text-based Build map format support
// For an unnamed EDuke32-based source port fork
// 2025-2026

#ifndef MAP_FORMAT_H_
#define MAP_FORMAT_H_

#include <cstdint>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
//                           Core Build map structures
// -----------------------------------------------------------------------------

using int32 = int32_t;
using uint32 = uint32_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int8  = int8_t;
using uint8  = uint8_t;

// Classic Build sector (40 bytes)
struct sector_t
{
    int16  wallptr;           // starting wall index
    int16  wallnum;           // number of walls in this sector
    int32  ceilingz;
    int32  floorz;
    int16  ceilingstat;
    int16  floorstat;
    int16  ceilingpicnum;
    int16  ceilingheinum;
    int8   ceilingshade;
    uint8  ceilingpal;
    uint8  ceilingxpanning;
    uint8  ceilingypanning;
    int16  floorpicnum;
    int16  floorheinum;
    int8   floorshade;
    uint8  floorpal;
    uint8  floorxpanning;
    uint8  floorypanning;
    uint8  visibility;
    uint8  filler;            // historically used for alignment/padding
    int16  lotag;
    int16  hitag;
    int16  extra;
};

// Classic Build wall (32 bytes)
struct wall_t
{
    int32  x;
    int32  y;
    int16  point2;            // index of next point in loop
    int16  nextwall;          // index of the wall on the other side (-1 if none)
    int16  nextsector;        // sector on the other side (-1 if none)
    int16  cstat;
    int16  picnum;
    int16  overpicnum;
    int8   shade;
    uint8  pal;
    uint8  xrepeat;
    uint8  yrepeat;
    uint8  xpanning;
    uint8  ypanning;
    int16  lotag;
    int16  hitag;
    int16  extra;
};

// Classic Build sprite (44 bytes)
struct spritetype
{
    int32  x;
    int32  y;
    int32  z;
    int16  cstat;
    int16  picnum;
    int8   shade;
    uint8  pal;
    uint8  clipdist;
    uint8  filler;            // alignment/padding
    uint8  xrepeat;
    uint8  yrepeat;
    int8   xoffset;
    int8   yoffset;
    int16  sectnum;
    int16  statnum;
    int16  ang;
    int16  owner;
    int16  xvel;
    int16  yvel;
    int16  zvel;
    int16  lotag;
    int16  hitag;
    int16  extra;
};

// -----------------------------------------------------------------------------
//                  Universal/Text-based map container
// -----------------------------------------------------------------------------

struct UniversalMap
{
    // Header / player start information
    int32               version = 0;           // usually 7,8,9
    std::string         format_namespace = "Build";
    std::string         author;
    std::string         description;
    std::string         map_title;             // optional

    // Player 1 start position (classic header)
    int32               posx = 0;
    int32               posy = 0;
    int32               posz = 0;
    int16               ang = 0;
    int16               cursectnum = 0;

    // Core geometry data
    std::vector<sector_t>   sectors;
    std::vector<wall_t>     walls;
    std::vector<spritetype> sprites;

    // Optional / extension fields (for future-proofing / ports)
    std::map<std::string, std::string>  metadata;     // key-value metadata
    std::map<std::string, int32>        int_properties;
    std::map<std::string, double>       float_properties;

    // Helpers / state (not saved)
    bool                is_text_format = false;
    bool                dirty = false;
};

// -----------------------------------------------------------------------------
//                           Interface / Functions
// -----------------------------------------------------------------------------

bool load_text_map(const char* filename, UniversalMap& out_map);
bool save_text_map(const char* filename, const UniversalMap& map);

bool load_binary_map(const char* filename, UniversalMap& out_map, int expected_version = -1);
bool save_binary_map(const char* filename, const UniversalMap& map, int version = 9);

// Conversion utilities
bool convert_classic_to_universal(const sector_t* classic_sectors, int num_sectors,
                                 const wall_t* classic_walls, int num_walls,
                                 const spritetype* classic_sprites, int num_sprites,
                                 UniversalMap& out_map);

void clear_universal_map(UniversalMap& map);

// Optional: very basic validation
bool validate_universal_map(const UniversalMap& map, std::string* out_error = nullptr);

#endif // MAP_FORMAT_H_
