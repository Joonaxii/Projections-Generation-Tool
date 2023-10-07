#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <functional>
#include <glm.hpp>
#include <string>
#include <nlohmann/json.hpp>
#include <J-Core/IO/IOUtils.h>
#include <J-Core/IO/ImageUtils.h>
#include <J-Core/IO/Image.h>
#include <J-Core/IO/Stream.h>
#include <J-Core/Util/DataUtilities.h>
#include <J-Core/Util/StringHelpers.h>
#include <J-Core/Util/EnumUtils.h>
#include <J-Core/Util/Span.h>
#include <J-Core/Rendering/Atlas.h>
#include <J-Core/Util/DataFormatUtils.h>
#include <J-Core/Util/PoolAllocator.h>

#include <smmintrin.h>
#include <J-Core/Util/AlignmentAllocator.h>

namespace Projections {
    static constexpr uint32_t PROJ_GEN_VERSION = 1;

    namespace detail {
        static inline void maskSimdIndices(const __m128i& simd, __m128i& buffer) {
            buffer.m128i_u64[0] &= simd.m128i_u64[0];
            buffer.m128i_u64[1] &= simd.m128i_u64[1];
        }

        static inline uint32_t getSimdMask(const __m128i& simd) {
            return simd.m128i_u32[0] & simd.m128i_u32[1] & simd.m128i_u32[2] & simd.m128i_u32[3];
        }

        static inline uint32_t getSimdORMask(const __m128i& simd) {
            return simd.m128i_u32[0] | simd.m128i_u32[1] | simd.m128i_u32[2] | simd.m128i_u32[3];
        }

        static inline bool compare(const __m128i* ptr, const __m128i* input) {
            for (size_t i = 0; i < 4; i++) {
                if ((
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++)) &
                    getSimdMask(_mm_cmpeq_epi32(*ptr++, *input++))
                    ) != 0xFFFFFFFFU) {
                    return false;
                }
            }
            return true;
        }
    }

    enum AtlasExportType {
        A_EXP_PNG,
        A_EXP_DDS,
        A_EXP_JTEX,
    };

    enum AtlasExportSize {
        A_EXP_S_RGBA32,
        A_EXP_S_RGBA4444,
    };

    struct OutputFormat {
        AtlasExportType type{ A_EXP_PNG };
        AtlasExportSize size{ A_EXP_S_RGBA32 };

        void reset() {
            type = A_EXP_PNG;
            size = A_EXP_S_RGBA32;
        }

        const char* getExtension() const {
            switch (type) {
                default: return ".png";
                case A_EXP_DDS: return ".dds";
                case A_EXP_JTEX: return ".jtex";
            }
        }

        AtlasExportSize getSize() const {
            return type == A_EXP_PNG ? A_EXP_S_RGBA32 : size;
        }

        void write(json& jsonF) const {
            jsonF["type"] = int32_t(type);
            jsonF["size"] = int32_t(size);
        }

        void read(json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                type = AtlasExportType(jsonF.value<int32_t>("type", int32_t(A_EXP_PNG)));
                size = AtlasExportSize(jsonF.value<int32_t>("size", int32_t(A_EXP_S_RGBA32)));
            }
        }
    };

    enum AnimSpeedType : uint8_t {
        ANIM_S_Ticks,
        ANIM_S_Duration,
        ANIM_S_COUNT,
    };

    enum DropType : uint8_t {
        DROP_Null,
        DROP_Enemy,
        DROP_Chest,
        DROP_TravelingMerchant,

        DROP_COUNT,
    };

    enum TileDrawLayer : uint8_t {
        TDRAW_BehindWalls,
        TDRAW_BehindTiles,
        TDRAW_AfterTiles,

        TDRAW_COUNT
    };

    enum RarityType : uint8_t {
        RARITY_Basic,
        RARITY_Intermediate,
        RARITY_Advanced,
        RARITY_Expert,
        RARITY_Master,
    };

    enum NPCID : int32_t {
        NPC_Big_Hornet_Stingy = -65,
        NPC_Little_Hornet_Stingy = -64,
        NPC_Big_Hornet_Spikey = -63,
        NPC_Little_Hornet_Spikey = -62,
        NPC_Big_Hornet_Leafy = -61,
        NPC_Little_Hornet_Leafy = -60,
        NPC_Big_Hornet_Honey = -59,
        NPC_Little_Hornet_Honey = -58,
        NPC_Big_Hornet_Fatty = -57,
        NPC_Little_Hornet_Fatty = -56,
        NPC_Big_Rain_Zombie = -55,
        NPC_Small_Rain_Zombie = -54,
        NPC_Big_Pantless_Skeleton = -53,
        NPC_Small_Pantless_Skeleton = -52,
        NPC_Big_Misassembled_Skeleton = -51,
        NPC_Small_Misassembled_Skeleton = -50,
        NPC_Big_Headache_Skeleton = -49,
        NPC_Small_Headache_Skeleton = -48,
        NPC_Big_Skeleton = -47,
        NPC_Small_Skeleton = -46,
        NPC_Big_Female_Zombie = -45,
        NPC_Small_Female_Zombie = -44,
        NPC_Demon_Eye_2 = -43,
        NPC_Purple_Eye_24 = -2,
        NPC_Green_Eye_2 = -41,
        NPC_Dialated_Eye_2 = -40,
        NPC_Sleepy_Eye_2 = -39,
        NPC_Cataract_Eye_2 = -38,
        NPC_Big_Twiggy_Zombie = -37,
        NPC_Small_Twiggy_Zombie = -36,
        NPC_Big_Swamp_Zombie = -35,
        NPC_Small_Swamp_Zombie = -34,
        NPC_Big_Slimed_Zombie = -33,
        NPC_Small_Slimed_Zombie = -32,
        NPC_Big_Pincushion_Zombie = -31,
        NPC_Small_Pincushion_Zombie = -30,
        NPC_Big_Bald_Zombie = -29,
        NPC_Small_Bald_Zombie = -28,
        NPC_Big_Zombie = -27,
        NPC_Small_Zombie = -26,
        NPC_Big_Crimslime = -25,
        NPC_Little_Crimslime = -24,
        NPC_Big_Crimera = -23,
        NPC_Little_Crimera = -22,
        NPC_Giant_Moss_Hornet = -21,
        NPC_Big_Moss_Hornet = -20,
        NPC_Little_Moss_Hornet = -19,
        NPC_Tiny_Moss_Hornet = -18,
        NPC_Big_Stinger = -17,
        NPC_Little_Stinger = -16,
        NPC_Heavy_Skeleton = -15,
        NPC_Big_Boned = -14,
        NPC_Short_Bones = -13,
        NPC_Big_Eater = -12,
        NPC_Little_Eater = -11,
        NPC_Jungle_Slime = -10,
        NPC_Yellow_Slime = -9,
        NPC_Red_Slime = -8,
        NPC_Purple_Slime = -7,
        NPC_Black_Slime = -6,
        NPC_Baby_Slime = -5,
        NPC_Pinky = -4,
        NPC_Green_Slime = -3,
        NPC_Slimer2 = -2,
        NPC_Slimeling = -1,
        NPC_Any,
        NPC_Blue_Slime,
        NPC_Demon_Eye,
        NPC_Zombie,
        NPC_Eye_of_Cthulhu,
        NPC_Servant_of_Cthulhu,
        NPC_Eater_of_Souls,
        NPC_Devourer,
        NPC_Devourer_Body,
        NPC_Devourer_Tail,
        NPC_Giant_Worm,
        NPC_Giant_Worm_Body,
        NPC_Giant_Worm_Tail,
        NPC_Eater_of_Worlds,
        NPC_Eater_of_Worlds_Body,
        NPC_Eater_of_Worlds_Tail,
        NPC_Mother_Slime,
        NPC_Merchant,
        NPC_Nurse,
        NPC_Arms_Dealer,
        NPC_Dryad,
        NPC_Skeleton,
        NPC_Guide,
        NPC_Meteor_Head,
        NPC_Fire_Imp,
        NPC_Burning_Sphere,
        NPC_Goblin_Peon,
        NPC_Goblin_Thief,
        NPC_Goblin_Warrior,
        NPC_Goblin_Sorcerer,
        NPC_Chaos_Ball,
        NPC_Angry_Bones,
        NPC_Dark_Caster,
        NPC_Water_Sphere,
        NPC_Cursed_Skull,
        NPC_Skeletron,
        NPC_Skeletron_Hand,
        NPC_Old_Man,
        NPC_Demolitionist,
        NPC_Bone_Serpent,
        NPC_Bone_Serpent_Body,
        NPC_Bone_Serpent_Tail,
        NPC_Hornet,
        NPC_Man_Eater,
        NPC_Undead_Miner,
        NPC_Tim,
        NPC_Bunny,
        NPC_Corrupt_Bunny,
        NPC_Harpy,
        NPC_Cave_Bat,
        NPC_King_Slime,
        NPC_Jungle_Bat,
        NPC_Doctor_Bones,
        NPC_The_Groom,
        NPC_Clothier,
        NPC_Goldfish,
        NPC_Snatcher,
        NPC_Corrupt_Goldfish,
        NPC_Piranha,
        NPC_Lava_Slime,
        NPC_Hellbat,
        NPC_Vulture,
        NPC_Demon,
        NPC_Blue_Jellyfish,
        NPC_Pink_Jellyfish,
        NPC_Shark,
        NPC_Voodoo_Demon,
        NPC_Crab,
        NPC_Dungeon_Guardian,
        NPC_Antlion,
        NPC_Spike_Ball,
        NPC_Dungeon_Slime,
        NPC_Blazing_Wheel,
        NPC_Goblin_Scout,
        NPC_Bird,
        NPC_Pixie,
        NPC_BUFFER_0,
        NPC_Armored_Skeleton,
        NPC_Mummy,
        NPC_Dark_Mummy,
        NPC_Light_Mummy,
        NPC_Corrupt_Slime,
        NPC_Wraith,
        NPC_Cursed_Hammer,
        NPC_Enchanted_Sword,
        NPC_Mimic,
        NPC_Unicorn,
        NPC_Wyvern,
        NPC_Wyvern_Legs,
        NPC_Wyvern_Body_0,
        NPC_Wyvern_Body_1,
        NPC_Wyvern_Body_2,
        NPC_Wyvern_Tail,
        NPC_Giant_Bat,
        NPC_Corruptor,
        NPC_Digger,
        NPC_Digger_Body,
        NPC_Digger_Tail,
        NPC_World_Feeder,
        NPC_World_Feeder_Body,
        NPC_World_Feeder_Tail,
        NPC_Clinger,
        NPC_Angler_Fish,
        NPC_Green_Jellyfish,
        NPC_Werewolf,
        NPC_Bound_Goblin,
        NPC_Bound_Wizard,
        NPC_Goblin_Tinkerer,
        NPC_Wizard,
        NPC_Clown,
        NPC_Skeleton_Archer,
        NPC_Goblin_Archer,
        NPC_Vile_Spit,
        NPC_Wall_of_Flesh,
        NPC_Wall_of_Flesh_Eye,
        NPC_The_Hungry,
        NPC_The_Hungry_II,
        NPC_Leech,
        NPC_Leech_Body,
        NPC_Leech_Tail,
        NPC_Chaos_Elemental,
        NPC_Slimer,
        NPC_Gastropod,
        NPC_Bound_Mechanic,
        NPC_Mechanic,
        NPC_Retinazer,
        NPC_Spazmatism,
        NPC_Skeletron_Prime,
        NPC_Prime_Cannon,
        NPC_Prime_Saw,
        NPC_Prime_Vice,
        NPC_Prime_Laser,
        NPC_Zombie_Bald,
        NPC_Wandering_Eye,
        NPC_The_Destroyer,
        NPC_The_Destroyer_Body,
        NPC_The_Destroyer_Tail,
        NPC_Illuminant_Bat,
        NPC_Illuminant_Slime,
        NPC_Probe,
        NPC_Possessed_Armor,
        NPC_Toxic_Sludge,
        NPC_Santa_Claus,
        NPC_Snowman_Gangsta,
        NPC_Mister_Stabby,
        NPC_Snow_Balla,
        BUFFER_1,
        NPC_Ice_Slime,
        NPC_Penguin,
        NPC_Penguin_Black,
        NPC_Ice_Bat,
        NPC_Lava_Bat,
        NPC_Giant_Flying_Fox,
        NPC_Giant_Tortoise,
        NPC_Ice_Tortoise,
        NPC_Wolf,
        NPC_Red_Devil,
        NPC_Arapaima,
        NPC_Vampire_Bat,
        NPC_Vampire,
        NPC_Truffle,
        NPC_Zombie_Eskimo,
        NPC_Frankenstein,
        NPC_Black_Recluse,
        NPC_Wall_Creeper,
        NPC_Wall_Creeper_Wall,
        NPC_Swamp_Thing,
        NPC_Undead_Viking,
        NPC_Corrupt_Penguin,
        NPC_Ice_Elemental,
        NPC_Pigron_Corrupt,
        NPC_Pigron_Hallow,
        NPC_Rune_Wizard,
        NPC_Crimera,
        NPC_Herpling,
        NPC_Angry_Trapper,
        NPC_Moss_Hornet,
        NPC_Derpling,
        NPC_Steampunker,
        NPC_Crimson_Axe,
        NPC_Pigron_Crimson,
        NPC_Face_Monster,
        NPC_Floaty_Gross,
        NPC_Crimslime,
        NPC_Spiked_Ice_Slime,
        NPC_Snow_Flinx,
        NPC_Pincushion_Zombie,
        NPC_Slimed_Zombie,
        NPC_Swamp_Zombie,
        NPC_Twiggy_Zombie,
        NPC_Cataract_Eye,
        NPC_Sleepy_Eye,
        NPC_Dialated_Eye,
        NPC_Green_Eye,
        NPC_Purple_Eye,
        NPC_Lost_Girl,
        NPC_Nymph,
        NPC_Armored_Viking,
        NPC_Lihzahrd,
        NPC_Lihzahrd_Crawler,
        NPC_Female_Zombie,
        NPC_Headache_Skeleton,
        NPC_Misassembled_Skeleton,
        NPC_Pantless_Skeleton,
        NPC_Spiked_Jungle_Slime,
        NPC_Moth,
        NPC_Icy_Merman,
        NPC_Dye_Trader,
        NPC_Party_Girl,
        NPC_Cyborg,
        NPC_Bee,
        NPC_Bee_Small,
        NPC_Pirate_Deckhand,
        NPC_Pirate_Corsair,
        NPC_Pirate_Deadeye,
        NPC_Pirate_Crossbower,
        NPC_Pirate_Captain,
        NPC_Cochineal_Beetle,
        NPC_Cyan_Beetle,
        NPC_Lac_Beetle,
        NPC_Sea_Snail,
        NPC_Squid,
        NPC_Queen_Bee,
        NPC_Raincoat_Zombie,
        NPC_Flying_Fish,
        NPC_Umbrella_Slime,
        NPC_Flying_Snake,
        NPC_Painter,
        NPC_Witch_Doctor,
        NPC_Pirate,
        NPC_Goldfish_Walker,
        NPC_Hornet_Fatty,
        NPC_Hornet_Honey,
        NPC_Hornet_Leafy,
        NPC_Hornet_Spikey,
        NPC_Hornet_Stingy,
        NPC_Jungle_Creeper,
        NPC_Jungle_Creeper_Wall,
        NPC_Black_Recluse_Wall,
        NPC_Blood_Crawler,
        NPC_Blood_Crawler_Wall,
        NPC_Blood_Feeder,
        NPC_Blood_Jelly,
        NPC_Ice_Golem,
        NPC_Rainbow_Slime,
        NPC_Golem,
        NPC_Golem_Head,
        NPC_Golem_Fist_Left,
        NPC_Golem_Fist_Right,
        NPC_Golem_Head_Free,
        NPC_Angry_Nimbus,
        NPC_Eyezor,
        NPC_Parrot,
        NPC_Reaper,
        NPC_Spore_Zombie,
        NPC_Spore_Zombie_Hat,
        NPC_Fungo_Fish,
        NPC_Anomura_Fungus,
        NPC_Mushi_Ladybug,
        NPC_Fungi_Bulb,
        NPC_Giant_Fungi_Bulb,
        NPC_Fungi_Spore,
        NPC_Plantera,
        NPC_Planteras_Hook,
        NPC_Planteras_Tentacle,
        NPC_Spore,
        NPC_Brain_of_Cthulhu,
        NPC_Creeper,
        NPC_Ichor_Sticker,
        NPC_Rusty_Armored_Bones_Axe,
        NPC_Rusty_Armored_Bones_Flail,
        NPC_Rusty_Armored_Bones_Sword,
        NPC_Rusty_Armored_Bones_Sword_No_Armor,
        NPC_Blue_Armored_Bones,
        NPC_Blue_Armored_Bones_Mace,
        NPC_Blue_Armored_Bones_No_Pants,
        NPC_Blue_Armored_Bones_Sword,
        NPC_Hell_Armored_Bones,
        NPC_Hell_Armored_Bones_Spike_Shield,
        NPC_Hell_Armored_Bones_Mace,
        NPC_Hell_Armored_Bones_Sword,
        NPC_Ragged_Caster,
        NPC_Ragged_Caster_Open_Coat,
        NPC_Necromancer,
        NPC_Necromancer_Armored,
        NPC_Diabolist,
        NPC_Diabolist_White,
        NPC_Bone_Lee,
        NPC_Dungeon_Spirit,
        NPC_Giant_Cursed_Skull,
        NPC_Paladin,
        NPC_Skeleton_Sniper,
        NPC_Tactical_Skeleton,
        NPC_Skeleton_Commando,
        NPC_Angry_Bones_Big,
        NPC_Angry_Bones_Big_Muscle,
        NPC_Angry_Bones_Big_Helmet,
        NPC_Blue_Jay,
        NPC_Cardinal,
        NPC_Squirrel,
        NPC_Mouse,
        NPC_Raven,
        NPC_Slime_Masked,
        NPC_Bunny_Slimed,
        NPC_Hoppin_Jack,
        NPC_Scarecrow,
        NPC_Scarecrow_2,
        NPC_Scarecrow_3,
        NPC_Scarecrow_4,
        NPC_Scarecrow_5,
        NPC_Scarecrow_6,
        NPC_Scarecrow_7,
        NPC_Scarecrow_8,
        NPC_Scarecrow_9,
        NPC_Scarecrow_10,
        NPC_Headless_Horseman,
        NPC_Ghost,
        NPC_Demon_Eye_Owl,
        NPC_Demon_Eye_Spaceship,
        NPC_Zombie_Doctor,
        NPC_Zombie_Superman,
        NPC_Zombie_Pixie,
        NPC_Skeleton_Top_Hat,
        NPC_Skeleton_Astronaut,
        NPC_Skeleton_Alien,
        NPC_Mourning_Wood,
        NPC_Splinterling,
        NPC_Pumpking,
        NPC_Pumpking_Scythe,
        NPC_Hellhound,
        NPC_Poltergeist,
        NPC_Zombie_Xmas,
        NPC_Zombie_Sweater,
        NPC_Slime_Ribbon_White,
        NPC_Slime_Ribbon_Yellow,
        NPC_Slime_Ribbon_Green,
        NPC_Slime_Ribbon_Red,
        NPC_Bunny_Xmas,
        NPC_Zombie_Elf,
        NPC_Zombie_Elf_Beard,
        NPC_Zombie_Elf_Girl,
        NPC_Present_Mimic,
        NPC_Gingerbread_Man,
        NPC_Yeti,
        NPC_Everscream,
        NPC_Ice_Queen,
        NPC_Santa,
        NPC_Elf_Copter,
        NPC_Nutcracker,
        NPC_Nutcracker_Spinning,
        NPC_Elf_Archer,
        NPC_Krampus,
        NPC_Flocko,
        NPC_Stylist,
        NPC_Webbed_Stylist,
        NPC_Firefly,
        NPC_Butterfly,
        NPC_Worm,
        NPC_Lightning_Bug,
        NPC_Snail,
        NPC_Glowing_Snail,
        NPC_Frog,
        NPC_Duck,
        NPC_Duck_2,
        NPC_Duck_White,
        NPC_Duck_White_2,
        NPC_Scorpion_Black,
        NPC_Scorpion,
        NPC_Traveling_Merchant,
        NPC_Angler,
        NPC_Duke_Fishron,
        NPC_Detonating_Bubble,
        NPC_Sharkron,
        NPC_Sharkron_2,
        NPC_Truffle_Worm,
        NPC_Truffle_Worm_Digger,
        NPC_Sleeping_Angler,
        NPC_Grasshopper,
        NPC_Chattering_Teeth_Bomb,
        NPC_Blue_Cultist_Archer,
        NPC_White_Cultist_Archer,
        NPC_Brain_Scrambler,
        NPC_Ray_Gunner,
        NPC_Martian_Officer,
        NPC_Bubble_Shield,
        NPC_Gray_Grunt,
        NPC_Martian_Engineer,
        NPC_Tesla_Turret,
        NPC_Martian_Drone,
        NPC_Gigazapper,
        NPC_Scutlix_Gunner,
        NPC_Scutlix,
        NPC_Martian_Saucer,
        NPC_Martian_Saucer_Turret,
        NPC_Martian_Saucer_Cannon,
        NPC_Martian_Saucer_Core,
        NPC_Moon_Lord,
        NPC_Moon_Lords_Hand,
        NPC_Moon_Lords_Core,
        NPC_Martian_Probe,
        NPC_Moon_Lord_Free_Eye,
        NPC_Moon_Leech_Clot,
        NPC_Milkyway_Weaver,
        NPC_Milkyway_Weaver_Body,
        NPC_Milkyway_Weaver_Tail,
        NPC_Star_Cell,
        NPC_Star_Cell_Mini,
        NPC_Flow_Invader,
        NPC_BUFFER_1,
        NPC_Twinkle_Popper,
        NPC_Twinkle,
        NPC_Stargazer,
        NPC_Crawltipede,
        NPC_Crawltipede_Body,
        NPC_Crawltipede_Tail,
        NPC_Drakomire,
        NPC_Drakomire_Rider,
        NPC_Sroller,
        NPC_Corite,
        NPC_Selenian,
        NPC_Nebula_Floater,
        NPC_Brain_Suckler,
        NPC_Vortex_Pillar,
        NPC_Evolution_Beast,
        NPC_Predictor,
        NPC_Storm_Diver,
        NPC_Alien_Queen,
        NPC_Alien_Hornet,
        NPC_Alien_Larva,
        NPC_Zombie_Armed,
        NPC_Zombie_Frozen,
        NPC_Zombie_Armed_Pincushion,
        NPC_Zombie_Armed_Frozen,
        NPC_Zombie_Armed_Slimed,
        NPC_Zombie_Armed_Swamp,
        NPC_Zombie_Armed_Twiggy,
        NPC_Zombie_Armed_Female,
        NPC_Mysterious_Tablet,
        NPC_Lunatic_Devotee,
        NPC_Lunatic_Cultist,
        NPC_Lunatic_Cultist_Clone,
        NPC_Tax_Collector,
        NPC_Gold_Bird,
        NPC_Gold_Bunny,
        NPC_Gold_Butterfly,
        NPC_Gold_Frog,
        NPC_Gold_Grasshopper,
        NPC_Gold_Mouse,
        NPC_Gold_Worm,
        NPC_Skeleton_Bone_Throwing,
        NPC_Skeleton_Bone_Throwing_2,
        NPC_Skeleton_Bone_Throwing_3,
        NPC_Skeleton_Bone_Throwing_4,
        NPC_Skeleton_Merchant,
        NPC_Phantasm_Dragon,
        NPC_Phantasm_Dragon_Body_1,
        NPC_Phantasm_Dragon_Body_2,
        NPC_Phantasm_Dragon_Body_3,
        NPC_Phantasm_Dragon_Body_4,
        NPC_Phantasm_Dragon_Tail,
        NPC_Butcher,
        NPC_Creature_from_the_Deep,
        NPC_Fritz,
        NPC_Nailhead,
        NPC_Crimtane_Bunny,
        NPC_Crimtane_Goldfish,
        NPC_Psycho,
        NPC_Deadly_Sphere,
        NPC_Dr_Man_Fly,
        NPC_The_Possessed,
        NPC_Vicious_Penguin,
        NPC_Goblin_Summoner,
        NPC_Shadowflame_Apparation,
        NPC_Corrupt_Mimic,
        NPC_Crimson_Mimic,
        NPC_Hallowed_Mimic,
        NPC_Jungle_Mimic,
        NPC_Mothron,
        NPC_Mothron_Egg,
        NPC_Baby_Mothron,
        NPC_Medusa,
        NPC_Hoplite,
        NPC_Granite_Golem,
        NPC_Granite_Elemental,
        NPC_Enchanted_Nightcrawler,
        NPC_Grubby,
        NPC_Sluggy,
        NPC_Buggy,
        NPC_Target_Dummy,
        NPC_Blood_Zombie,
        NPC_Drippler,
        NPC_Stardust_Pillar,
        NPC_Crawdad,
        NPC_Crawdad_2,
        NPC_Giant_Shelly,
        NPC_Giant_Shelly_2,
        NPC_Salamander,
        NPC_Salamander_2,
        NPC_Salamander_3,
        NPC_Salamander_4,
        NPC_Salamander_5,
        NPC_Salamander_6,
        NPC_Salamander_7,
        NPC_Salamander_8,
        NPC_Salamander_9,
        NPC_Nebula_Pillar,
        NPC_Antlion_Charger_Giant,
        NPC_Antlion_Swarmer_Giant,
        NPC_Dune_Splicer,
        NPC_Dune_Splicer_Body,
        NPC_Dune_Splicer_Tail,
        NPC_Tomb_Crawler,
        NPC_Tomb_Crawler_Body,
        NPC_Tomb_Crawler_Tail,
        NPC_Solar_Flare,
        NPC_Solar_Pillar,
        NPC_Drakanian,
        NPC_Solar_Fragment,
        NPC_Martian_Walker,
        NPC_Ancient_Vision,
        NPC_Ancient_Light,
        NPC_Ancient_Doom,
        NPC_Ghoul,
        NPC_Vile_Ghoul,
        NPC_Tainted_Ghoul,
        NPC_Dreamer_Ghoul,
        NPC_Lamia,
        NPC_Lamia_Dark,
        NPC_Sand_Poacher,
        NPC_Sand_Poacher_Wall,
        NPC_Basilisk,
        NPC_Desert_Spirit,
        NPC_Tortured_Soul,
        NPC_Spiked_Slime,
        NPC_The_Bride,
        NPC_Sand_Slime,
        NPC_Red_Squirrel,
        NPC_Gold_Squirrel,
        NPC_Bunny_Party,
        NPC_Sand_Elemental,
        NPC_Sand_Shark,
        NPC_Bone_Biter,
        NPC_Flesh_Reaver,
        NPC_Crystal_Thresher,
        NPC_Angry_Tumbler,
        NPC_QuestionMark,
        NPC_Eternia_Crystal,
        NPC_Mysterious_Portal,
        NPC_Tavernkeep,
        NPC_Betsy,
        NPC_Etherian_Goblin,
        NPC_Etherian_Goblin_2,
        NPC_Etherian_Goblin_3,
        NPC_Etherian_Goblin_Bomber,
        NPC_Etherian_Goblin_Bomber_2,
        NPC_Etherian_Goblin_Bomber_3,
        NPC_Etherian_Wyvern,
        NPC_Etherian_Wyvern_2,
        NPC_Etherian_Wyvern_3,
        NPC_Etherian_Javelin_Thrower,
        NPC_Etherian_Javelin_Thrower_2,
        NPC_Etherian_Javelin_Thrower_3,
        NPC_Dark_Mage,
        NPC_Dark_Mage_T3,
        NPC_Old_Ones_Skeleton,
        NPC_Old_Ones_Skeleton_T3,
        NPC_Wither_Beast,
        NPC_Wither_Beast_T3,
        NPC_Drakin,
        NPC_Drakin_T3,
        NPC_Kobold,
        NPC_Kobold_T3,
        NPC_Kobold_Glider,
        NPC_Kobold_Glider_T3,
        NPC_Ogre,
        NPC_Ogre_T3,
        NPC_Etherian_Lightning_Bug,
        NPC_Unconscious_Man,
        NPC_Walking_Charger,
        NPC_Flying_Antlion,
        NPC_Antlion_Larva,
        NPC_Pink_Fairy,
        NPC_Green_Fairy,
        NPC_Blue_Fairy,
        NPC_Zombie_Merman,
        NPC_Wandering_Eye_Fish,
        NPC_Golfer,
        NPC_Golfer_Rescue,
        NPC_Zombie_Torch,
        NPC_Zombie_Armed_Torch,
        NPC_Gold_Goldfish,
        NPC_Gold_Goldfish_Walker,
        NPC_Windy_Balloon,
        NPC_Dragonfly_Black,
        NPC_Dragonfly_Blue,
        NPC_Dragonfly_Green,
        NPC_Dragonfly_Orange,
        NPC_Dragonfly_Red,
        NPC_Dragonfly_Yellow,
        NPC_Dragonfly_Gold,
        NPC_Seagull,
        NPC_Seagull_2,
        NPC_Ladybug,
        NPC_Gold_Ladybug,
        NPC_Maggot,
        NPC_Pupfish,
        NPC_Grebe,
        NPC_Grebe_2,
        NPC_Rat,
        NPC_Owl,
        NPC_Water_Strider,
        NPC_Water_Strider_Gold,
        NPC_Explosive_Bunny,
        NPC_Dolphin,
        NPC_Turtle,
        NPC_Turtle_Jungle,
        NPC_Dreadnautilus,
        NPC_Blood_Squid,
        NPC_Hemogoblin_Shark,
        NPC_Blood_Eel,
        NPC_Blood_Eel_Body,
        NPC_Blood_Eel_Tail,
        NPC_Gnome,
        NPC_Sea_Turtle,
        NPC_Sea_Horse,
        NPC_Sea_Horse_Gold,
        NPC_Angry_Dandelion,
        NPC_Ice_Mimic,
        NPC_Blood_Mummy,
        NPC_Rock_Golem,
        NPC_Maggot_Zombie,
        NPC_Zoologist,
        NPC_Spore_Bat,
        NPC_Spore_Skeleton,
        NPC_Empress_of_Light,
        NPC_Town_Cat,
        NPC_Town_Dog,
        NPC_Amethyst_Squirrel,
        NPC_Topaz_Squirrel,
        NPC_Sapphire_Squirrel,
        NPC_Emerald_Squirrel,
        NPC_Ruby_Squirrel,
        NPC_Diamond_Squirrel,
        NPC_Amber_Squirrel,
        NPC_Amethyst_Bunny,
        NPC_Topaz_Bunny,
        NPC_Sapphire_Bunny,
        NPC_Emerald_Bunny,
        NPC_Ruby_Bunny,
        NPC_Diamond_Bunny,
        NPC_Amber_Bunny,
        NPC_Hell_Butterfly,
        NPC_Lavafly,
        NPC_Magma_Snail,
        NPC_Town_Bunny,
        NPC_Queen_Slime,
        NPC_Crystal_Slime,
        NPC_Bouncy_Slime,
        NPC_Heavenly_Slime,
        NPC_Prismatic_Lacewing,
        NPC_Pirate_Ghost,
        NPC_Princess,
        NPC_Toch_God,
        NPC_Chaos_Ball_Tim,
        NPC_Vile_Spit_EoW,
        NPC_Golden_Slime,
        NPC_Deerclops,
        NPC_Stinkbug,
        NPC_Nerdy_Slime,
        NPC_Scarlet_Macaw,
        NPC_Blue_Macaw,
        NPC_Toucan,
        NPC_Yellow_Cokatiel,
        NPC_Gray_Cokatiel,
        NPC_Shimmer_Slime,
        NPC_Faeling,
        NPC_Cool_Slime,
        NPC_Elder_Slime,
        NPC_Clumsy_Slime,
        NPC_Diva_Slime,
        NPC_Surly_Slime,
        NPC_Squire_Slime,
        NPC_Old_Shaking_Chest,
        NPC_Clumsy_Balloon_Slime,
        NPC_Mystic_Frog,

        NPC_MAX,
        NPC_MIN = NPC_Big_Hornet_Stingy,
        NPC_Count = NPC_MAX - NPC_MIN,
    };

    enum BiomeFlags : uint64_t {
        BIOME_None = 0x00000000,
        BIOME_Sky = 0x00000001,
        BIOME_Surface = 0x00000002,
        BIOME_Underground = 0x00000004,
        BIOME_Caverns = 0x00000008,
        BIOME_Underworld = 0x00000010,
        BIOME_Forest = 0x00000020,
        BIOME_Desert = 0x00000040,
        BIOME_Ocean = 0x00000080,
        BIOME_Snow = 0x00000100,
        BIOME_Jungle = 0x00000200,
        BIOME_Meteorite = 0x00000400,
        BIOME_Mushroom = 0x00000800,
        BIOME_Dungeon = 0x00001000,
        BIOME_Temple = 0x00002000,
        BIOME_Aether = 0x00004000,
        BIOME_BeeHive = 0x00008000,
        BIOME_Granite = 0x00010000,
        BIOME_Marble = 0x00020000,
        BIOME_Graveyard = 0x00040000,
        BIOME_InOldOnesArmy = 0x00080000,
        BIOME_Town = 0x00100000,
        BIOME_InWaterCandle = 0x00200000,
        BIOME_InPeaceCandle = 0x00400000,
        BIOME_InShadowCandle = 0x00800000,
        BIOME_TowerSolar = 0x01000000,
        BIOME_TowerNebula = 0x02000000,
        BIOME_TowerVortex = 0x03000000,
        BIOME_TowerStardust = 0x08000000,
        BIOME_Purity = 0x10000000,
        BIOME_Corruption = 0x20000000,
        BIOME_Crimson = 0x40000000,
        BIOME_Hallowed = 0x80000000,
        BIOME_RequireAll_00 = 0x100000000,
        BIOME_RequireAll_01 = 0x200000000,
        BIOME_RequireAll_02 = 0x400000000,
        BIOME_RequireAll_03 = 0x800000000,
    };

    enum WorldConditions : uint64_t {
        WCF_None = 0,
        WCF_IsHardmode = 0x0001,
        WCF_IsExpert = 0x0002,
        WCF_IsMaster = 0x0004,
        WCF_IsDay = 0x0008,
        WCF_IsNight = 0x0010,
        WCF_IsBloodMoon = 0x0020,
        WCF_IsEclipse = 0x0040,
        WCF_IsPumpkinMoon = 0x0080,
        WCF_IsFrostMoon = 0x0100,
        WCF_IsHalloween = 0x0200,
        WCF_IsChristmas = 0x0800,
        WCF_IsSandstorm = 0x1000,
        WCF_IsRaining = 0x2000,
        WCF_IsStorm = 0x4000,
        WCF_IsCrimson = 0x8000,

        WCF_DWNKingSlime = 0x00000000010000,
        WCF_DWNEyeOfCthulhu = 0x00000000020000,
        WCF_DWNEaterOfWorld = 0x00000000040000,
        WCF_DWNBrainOfCthulhu = 0x00000000080000,
        WCF_DWNQueenBee = 0x00000000100000,
        WCF_DWNSkeletron = 0x00000000200000,
        WCF_DWNDeerclops = 0x00000000400000,
        WCF_DWNWallOfFlesh = 0x00000000800000,
        WCF_DWNQueenSlime = 0x00000001000000,
        WCF_DWNTheTwins = 0x00000002000000,
        WCF_DWNTheDestroyer = 0x00000004000000,
        WCF_DWNSkeletronPrime = 0x00000008000000,
        WCF_DWNPlantera = 0x00000010000000,
        WCF_DWNGolem = 0x00000020000000,
        WCF_DWNDukeFishron = 0x00000040000000,
        WCF_DWNEmpressOfLight = 0x00000080000000,
        WCF_DWNLunaticCultist = 0x00000100000000,
        WCF_DWNMoonLord = 0x00000200000000,
        WCF_DWNMourningWood = 0x00000400000000,
        WCF_DWNPumpking = 0x00000800000000,
        WCF_DWNEverscream = 0x00001000000000,
        WCF_DWNSantaNK1 = 0x00002000000000,
        WCF_DWNIceQueen = 0x00004000000000,
        WCF_DWNSolarPillar = 0x00008000000000,
        WCF_DWNNebulaPillar = 0x00010000000000,
        WCF_DWNVortexPillar = 0x00020000000000,
        WCF_DWNStardustPillar = 0x00040000000000,
        WCF_DWNGoblinArmy = 0x00080000000000,
        WCF_DWNPirates = 0x00100000000000,
        WCF_DWNFrostLegion = 0x00200000000000,
        WCF_DWNMartians = 0x00400000000000,
        WCF_DWNPumpkinMoon = 0x00800000000000,
        WCF_DWNFrostMoon = 0x01000000000000,

        //WCF_DWNFrostMoon = 0x01000000000000,
        WCF_ValidOnGen = WCF_IsExpert | WCF_IsMaster,
    };

    enum AnimationMode : uint8_t {
        ANIM_None = 0x00,
        ANIM_Loop = 0x01,
        ANIM_Follow = 0x02,
        ANIM_COUNT,
    };

    enum TileDrawMode : uint8_t {
        TDR_Default,
        TDR_WithGlow,
    };

    static inline void writeShortString(const std::string& str, const Stream& stream) {
        uint8_t len = uint8_t(str.length() > 255 ? 255 : str.length());
        stream.writeValue(len);
        stream.write(str.c_str(), len, false);
    }

    enum GenFlags : uint8_t {
        PROJ_FLG_NO_SIMD = 0x1,
        PROJ_FLG_PREMULTIPLY_TILES = 0x2,
        PROJ_FLG_PREMULTIPLY_ICONS = 0x4,
        PROJ_DX9_ATLASES = 0x8,

        PROJ_FLG_PREMULTIPLY = PROJ_FLG_PREMULTIPLY_TILES | PROJ_FLG_PREMULTIPLY_ICONS,
    };

    enum VariationApplyMode : uint8_t {
        VAR_APPLY_None = 0x00,
        VAR_APPLY_Name = 0x01,
        VAR_APPLY_AnimMode = 0x02,
        VAR_APPLY_AnimSpeed = 0x04,
        VAR_APPLY_LoopStart = 0x08,
    };

#pragma pack(push, 1)

    struct TraderInfo {
        bool canBeTraded{};
        WorldConditions worldConditions{};
        BiomeFlags biomeFlags{};
        float weight{};

        void reset() {
            weight = 5.0f;
            worldConditions = WCF_None;
            biomeFlags = BIOME_None;
            canBeTraded = false;
        }

        void write(json& jsonF) const {
            jsonF["canBeTraded"] = canBeTraded;
            jsonF["worldConditions"] = uint64_t(worldConditions);
            jsonF["biomeFlags"] = uint64_t(biomeFlags);
            jsonF["weight"] = weight;
        }

        void write(const Stream& stream) const {
            stream.writeValue(canBeTraded);
            stream.writeValue(worldConditions);
            stream.writeValue(biomeFlags);
            stream.writeValue(weight);
        }

        void read(json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                canBeTraded = jsonF.value("canBeTraded", false);
                worldConditions = WorldConditions(jsonF.value("worldConditions", 0ULL));
                biomeFlags = BiomeFlags(jsonF.value("biomeFlags", 0ULL));
                weight = jsonF.value("weight", 25.0f);
            }
        }
    };

    struct DropInfo {
        enum DropFlags : uint8_t {
            DROP_None,
            DROP_Modded = 0x1,
            DROP_NetID = 0x2,
        };

        uint8_t flags{};
        std::string modSource{};
        NPCID sourceEntity{};
        WorldConditions worldConditions{};
        BiomeFlags biomeFlags{};
        float chance{ 0.15f };
        float weight{ 25.0f };

        bool isModded() const { return bool(flags & DROP_Modded); }
        bool isNetID() const { return bool(flags & DROP_NetID); }

        void setIsModded(bool value) { flags = (flags & ~DROP_Modded) | (value ? DROP_Modded : DROP_None); }
        void setIsNetID(bool value) { flags = (flags & ~DROP_NetID) | (value ? DROP_NetID : DROP_None); }


        void reset() {
            flags = 0;
            chance = 0.15f;
            weight = 25.0f;
            modSource = "";
            sourceEntity = NPC_Any;
            worldConditions = WCF_None;
            biomeFlags = BIOME_None;
        }

        void write(json& jsonF) const {
            if (flags & DROP_Modded) {
                jsonF["sourceEntity"] = modSource;
            }
            else {
                jsonF["isNetID"] = bool(flags & DROP_NetID);
                jsonF["sourceEntity"] = int32_t(sourceEntity);
            }
            jsonF["worldConditions"] = uint64_t(worldConditions);
            jsonF["biomeFlags"] = uint64_t(biomeFlags);
            jsonF["chance"] = chance;
            jsonF["weight"] = weight;
        }

        void write(const Stream& stream) const {
            stream.writeValue(flags, 1, false);

            if (flags & DROP_Modded) {
                writeShortString(modSource, stream);
            }
            else {
                stream.writeValue(sourceEntity);
            }

            stream.writeValue(worldConditions);
            stream.writeValue(biomeFlags);
            stream.writeValue(chance);
            stream.writeValue(weight);
        }

        void read(json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                flags = DROP_None;
                auto& srcE = jsonF["sourceEntity"];
                if (srcE.is_string()) {
                    modSource = srcE.get<std::string>();
                    flags |= DROP_Modded;
                }
                else if (srcE.is_number_integer()) {
                    sourceEntity = NPCID(srcE.get<int32_t>());
                    flags |= jsonF.value("isNetID", false) ? DROP_NetID : DROP_None;
                }
                worldConditions = WorldConditions(jsonF.value("worldConditions", 0ULL));
                biomeFlags = BiomeFlags(jsonF.value("biomeFlags", 0ULL));
                chance = jsonF.value("chance", 0.15f);
                weight = jsonF.value("weight", 25.0f);
            }
        }
    };

    struct GenerationSettings {
        GenFlags flags{ PROJ_FLG_PREMULTIPLY };
        int32_t alphaClip{ 4 };
        OutputFormat atlasFormat{};

        constexpr GenerationSettings() : flags{ PROJ_FLG_PREMULTIPLY }, alphaClip(4) {}
        constexpr GenerationSettings(GenFlags flags, int32_t alphaClip) : flags(flags), alphaClip(alphaClip < 0 ? 0 : alphaClip > 255 ? 255 : alphaClip) {}

        void reset() {
            flags = PROJ_FLG_PREMULTIPLY;
            alphaClip = 4;
        }

        void write(json& jsonF) const {
            jsonF["flags"] = uint32_t(flags);
            jsonF["alphaClip"] = alphaClip;
            atlasFormat.write(jsonF["atlasFormat"]);
        }

        void read(json& jsonF) {
            flags = GenFlags(jsonF.value<uint32_t>("flags", PROJ_FLG_PREMULTIPLY));
            alphaClip = jsonF.value("alphaClip", 4);
            atlasFormat.read(jsonF["atlasFormat"]);
        }

        constexpr bool useSIMD() const { return (flags & PROJ_FLG_NO_SIMD) == 0; }
        constexpr bool doPremult() const { return bool(flags & PROJ_FLG_PREMULTIPLY); }
        constexpr bool doPremultTiles() const { return bool(flags & PROJ_FLG_PREMULTIPLY_TILES); }
        constexpr bool doPremultIcons() const { return bool(flags & PROJ_FLG_PREMULTIPLY_ICONS); }
        constexpr bool isDX9() const { return bool(flags & PROJ_DX9_ATLASES); }
    };

    struct PixelTileIndex {
    public:
        static constexpr uint32_t INDEX_MASK = 0x3FFFFFFFU;

        constexpr PixelTileIndex() : _data(0) {}
        constexpr PixelTileIndex(uint32_t index, uint32_t rotation) : _data((index& INDEX_MASK) | ((rotation & 0x3) << 30)) {}
        constexpr PixelTileIndex(uint32_t data) : _data(data) {}

        constexpr bool operator==(uint32_t value) const { return (_data & INDEX_MASK) == value; }
        constexpr bool operator!=(uint32_t value) const { return (_data & INDEX_MASK) != value; }

        void setIndex(uint32_t index) { _data = (_data & ~INDEX_MASK) | (index & INDEX_MASK); }
        uint32_t getIndex() const { return _data & INDEX_MASK; }

        void setRotation(uint32_t rot) {
            _data = (_data & INDEX_MASK) | ((rot & 0x3) << 30);
        }
        uint32_t getRotation() const { return (_data >> 30) & 0x3; }

        void set(uint32_t index, uint32_t rot) {
            _data = (index & INDEX_MASK) | ((rot & 0x3) << 30);
        }
        constexpr uint32_t getData() const { return _data; }

    private:
        uint32_t _data;
    };
    struct PixelBlock {
        JCore::Color32 colors[16 * 16]{};

        constexpr PixelBlock() : colors{} {}
        PixelBlock(JCore::Color32 color) : PixelBlock() {
            for (size_t i = 0; i < 256; i++) {
                colors[i] = color;
            }
        }

        bool operator==(const PixelBlock& other) const {
            return memcmp(colors, other.colors, sizeof(colors)) == 0;
        }

        bool operator!=(const PixelBlock& other) const {
            return memcmp(colors, other.colors, sizeof(colors)) != 0;
        }
    };

    struct PixelBlockCH {
        PixelBlock blocks[4]{};

        PixelBlockCH() : blocks{} {}
        PixelBlockCH(const PixelBlock buffer[4]) : blocks{} { memcpy(blocks, buffer, sizeof(blocks)); }
        PixelBlockCH(JCore::Color32 color) : blocks{} {
            JCore::Color32* ptr = reinterpret_cast<JCore::Color32*>(blocks);
            for (size_t i = 0; i < sizeof(blocks) / sizeof(JCore::Color32); i++) {
                ptr[i] = color;
            }
        }

        PixelBlock& operator[](size_t i) { return blocks[i]; }
        const PixelBlock& operator[](size_t i) const { return blocks[i]; }
    };

    struct CRCBlock {
        uint32_t crc[4]{};

        constexpr CRCBlock() : crc{ 0 } {}

        uint32_t& operator[](size_t i) { return crc[0]; }
        constexpr uint32_t operator[](size_t i) const { return crc[0]; }

        void updateCrc(const PixelBlockCH& ch) {
            crc[0] = JCore::Data::updateCRC(0xFFFFFFFFU, ch[0].colors, 1024);
            crc[1] = JCore::Data::updateCRC(0xFFFFFFFFU, ch[1].colors, 1024);
            crc[2] = JCore::Data::updateCRC(0xFFFFFFFFU, ch[2].colors, 1024);
            crc[3] = JCore::Data::updateCRC(0xFFFFFFFFU, ch[3].colors, 1024);
        }

        void updateCrc(const PixelBlockCH& pix, int32_t ind) {
            crc[ind] = JCore::Data::updateCRC(0xFFFFFFFFU, pix[ind].colors, 1024);
        }

        void updateCrc(const PixelBlock& pix, int32_t ind) {
            crc[ind] = JCore::Data::updateCRC(0xFFFFFFFFU, pix.colors, 1024);
        }
    };

    struct PixelTile {
        uint8_t data{};
        uint16_t x{ 0 }, y{ 0 };

        constexpr PixelTile() : data(0x80) {}

        JCore::Color32 getColor(const PixelBlock& block) const {
            int32_t r = 0x00, g = 0x00, b = 0x00, a = 0x00;

            const uint8_t* data = reinterpret_cast<const uint8_t*>(block.colors);
            for (size_t i = 0; i < 256; i++) {
                r += *data++;
                g += *data++;
                b += *data++;
                a += *data++;
            }

            r >>= 8;
            g >>= 8;
            b >>= 8;
            a >>= 8;
            return JCore::Color32(uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a));
        }

        static JCore::PoolAllocator<PixelTile>& getAllocator() {
            static JCore::PoolAllocator<PixelTile> allocator{};
            return allocator;
        }

        constexpr uint8_t getAtlas() const {
            return data & 0x1F;
        }

        constexpr uint16_t getWidth() const { return 16; }
        constexpr uint16_t getHeight() const { return 16; }

        void setEmpty(bool val) {
            data = val ? (data | 0x80) : (data & 0x7F);
        }

        void setAtlas(uint8_t val) {
            data = (data & 0x80) | (val & 0x1F);
        }

        void setRotation(uint8_t rot) {
            data = (data & ~0x60) | ((rot & 0x3) << 5);
        }

        constexpr uint8_t getRotation() const {
            return (data >> 5) & 0x3;
        }

        constexpr bool isEmpty() const {
            return bool(data & 0x80);
        }

        void doPremultiply(PixelBlockCH& blockCh, CRCBlock& crc) {
            PixelBlock& block = blockCh[getRotation()];
            for (size_t i = 0; i < 256; i++) {
                premultiplyC32(block.colors[i]);
            }
            crc.updateCrc(blockCh, getRotation());
        }

        void setFull(PixelBlockCH& blockCh, JCore::Color32 color, CRCBlock& crc) {
            PixelBlock& block = blockCh[getRotation()];
            for (size_t i = 0; i < 256; i++) {
                block.colors[i] = color;
            }
            crc.updateCrc(blockCh, getRotation());
        }

        void copyFlipped(PixelBlockCH& blockMain, const PixelBlockCH& blockOther, const PixelTile& other, uint32_t rotation, CRCBlock& crc) {
            using namespace JCore;
            setRotation(uint8_t(rotation));
            PixelBlock& blockA = blockMain[rotation];
            const PixelBlock& blockB = blockOther[other.getRotation()];
            switch (rotation & 0x3) {
                case 0:
                    memcpy(this, &other, sizeof(PixelTile));
                    return;
                case 1: //X Flip
                    for (size_t y = 0, yP = 0; y < 16; y++, yP += 16) {
                        Data::reverseCopy(blockA.colors + yP, blockB.colors + yP, sizeof(Color32), 16);
                    }
                    break;
                case 2: //Y Flip
                    for (size_t y = 0, yP = 0, yPP = (16 * 16) - 16; y < 16; y++, yP += 16, yPP -= 16) {
                        memcpy(blockA.colors + yPP, blockB.colors + yP, 16 * sizeof(Color32));
                    }
                    break;
                case 3: //XY Flip
                    Data::reverseCopy(blockA.colors, blockB.colors, sizeof(Color32), 256);
                    break;
            }
            crc.updateCrc(blockMain, rotation);
        }

        void readFrom(PixelBlockCH& blockMain, const JCore::ImageData& img, int32_t index, uint8_t alphaClip, CRCBlock& crc) {
            using namespace JCore;
            PixelBlock& block = blockMain[getRotation()];
            if (img.format == TextureFormat::RGBA32) {
                const Color32* src = reinterpret_cast<const Color32*>(img.data);
                for (size_t y = 0, yX = 0; y < 16; y++, yX += 16)
                {
                    memcpy(block.colors + yX, src + index, 16 * sizeof(Color32));
                    index += img.width;
                }
            }
            else {
                const uint8_t* pixData = img.data + (img.isIndexed() ? img.paletteSize * sizeof(Color32) : 0);
                const int32_t bpp(getBitsPerPixel(img.format) >> 3);
                index *= bpp;
                const int32_t scan = img.width * bpp;
                for (int32_t y = 0, yP = 0; y < 16; y++, yP += 16) {
                    for (int32_t x = 0, xP = index; x < 16; x++, xP += bpp) {
                        convertPixel<Color32>(img.format, img.data, pixData + xP, block.colors[yP + x]);
                    }
                    index += scan;
                }
            }

            setEmpty(true);
            for (size_t i = 0; i < 256; i++) {
                auto& color = block.colors[i];
                if (color.a <= alphaClip)
                {
                    color.r = color.g = color.b = 0;
                }
                else { setEmpty(false); }
            }
            crc.updateCrc(blockMain, getRotation());
        }

        void maskWith(PixelBlock& block, const PixelBlock& blockOther, uint8_t alphaClip, CRCBlock& crc) {
            using namespace JCore;
            setEmpty(true);
            for (size_t i = 0; i < 256; i++) {
                Color32& self = block.colors[i];
                const Color32* otherC = blockOther.colors + i;
                self.a = multUI8(self.r, otherC->a);

                if (self.a <= alphaClip) { memset(&self, 0, 4); continue; }
                memcpy(&self, otherC, 3);
                setEmpty(false);
            }
            crc.updateCrc(block, getRotation());
        }

        void blitTo(const PixelBlockCH& blockCh, JCore::Color32* data, int32_t width) const {
            using namespace JCore;
            const PixelBlock& block = blockCh[getRotation()];
            for (size_t y = 0, yX = 0; y < 16; y++, yX += 16) {
                memcpy(data, block.colors + yX, 16 * sizeof(Color32));
                data += width;
            }
        }

        void writeTo(const PixelBlockCH& blockCh, JCore::Color32* data, int32_t width) const {
            blitTo(blockCh, data + (int32_t(y) * width + x), width);
        }

        void blitTo(const PixelBlockCH& blockCh, JCore::Color4444* data, int32_t width) const {
            using namespace JCore;
            const PixelBlock& block = blockCh[getRotation()];
            for (size_t y = 0, yX = 0; y < 16; y++, yX += 16) {
                auto pix = block.colors + yX;
                for (size_t x = 0; x < 16; x++) {
                    auto& p32 = pix[x];
                    data[x] = Color4444(p32.r >> 4, p32.g >> 4, p32.b >> 4, p32.a >> 4);
                }
                data += width;
            }
        }

        void writeTo(const PixelBlockCH& blockCh, JCore::Color4444* data, int32_t width) const {
            blitTo(blockCh, data + (int32_t(y) * width + x), width);
        }

        void blendTo(const PixelBlockCH& blockCh, JCore::Color32* data, int32_t width) const {
            using namespace JCore;
            const PixelBlock& block = blockCh[getRotation()];
            for (size_t y = 0, yX = 0; y < 16; y++, yX += 16) {
                auto ptrC = block.colors + yX;
                for (size_t i = 0; i < 16; i++) {
                    blendUI8(data[i], ptrC[i]);
                }
                data += width;
            }
        }
    };

    struct PixelTileCH {
        PixelTile tiles[4]{};
        PixelTileCH() : tiles{} {}
        PixelTileCH(const PixelTile buffer[4]) : tiles{} { memcpy(tiles, buffer, sizeof(tiles)); }

        PixelTile& operator[](size_t i) { return tiles[i]; }
        const PixelTile& operator[](size_t i) const { return tiles[i]; }
    };

    typedef std::vector<PixelBlockCH, JCore::AlignmentAllocator<PixelBlockCH, 16>> PixelBlockVector;
    typedef std::vector<CRCBlock, JCore::AlignmentAllocator<CRCBlock, 16>> CRCBlockVector;

    static constexpr PixelTile NullPixelTile{};

    struct ProjectionMaterial {
        static constexpr size_t MAX_DROPS = 48;

        std::string name{};
        std::string description{};

        RarityType rarity{};

        TraderInfo traderInfo{};
        uint8_t drops{ 0 };
        DropInfo dropInfo[MAX_DROPS]{};

        int16_t icon{ -1 };

        void removeDrop(int32_t index) {
            if (index < 0 || index >= drops) { return; }

            if (index != drops - 1) {
                memcpy(dropInfo + index, dropInfo + index + 1, size_t(drops) - index - 1);
            }
            drops--;
        }

        void read(json& jsonF) {
            using namespace JCore;
            name = IO::readString(jsonF["name"], "");
            description = IO::readString(jsonF["description"], "");
            rarity = RarityType(jsonF.value<int32_t>("rarity", 0));

            traderInfo.read(jsonF["traderInfo"]);

            drops = 0;
            auto& dropIs = jsonF["drops"];
            if (dropIs.is_array()) {
                drops = uint8_t(std::min(dropIs.size(), MAX_DROPS));
                for (size_t i = 0; i < drops; i++) {
                    dropInfo[i].read(dropIs[i]);
                }
            }
            else {
                dropIs = jsonF["drop"];
                if (dropIs.is_object()) {
                    drops = 1;
                    dropInfo[0].read(dropIs);
                }
            }
        }
        void write(json& jsonF) const {
            jsonF["name"] = name;
            jsonF["description"] = description;
            jsonF["rarity"] = int32_t(rarity);

            traderInfo.write(jsonF["traderInfo"]);
            if (drops > 1) {
                json::array_t arr{};
                for (size_t i = 0; i < drops; i++) {
                    dropInfo[i].write(arr.emplace_back(json::object_t{}));
                }
                jsonF["drops"] = arr;
            }
            else if (drops == 1) {
                dropInfo[0].write(jsonF["drop"]);
            }
        }

        void write(const Stream& stream) const {
            writeShortString(name, stream);
            writeShortString(description, stream);
            stream.writeValue(rarity);
            stream.writeValue(icon);
            traderInfo.write(stream);
            stream.writeValue(drops);
            for (size_t i = 0; i < drops; i++) {
                dropInfo[i].write(stream);
            }
        }
    };

    struct TileData {
    public:

        constexpr TileData() : _data(), _color() {}
        constexpr TileData(uint32_t x, uint32_t y, uint32_t atlas) :
            _data((x & 0xFFF) | ((y & 0xFFF) << 12) | ((atlas & 0xFF) << 24)), _color() {}

        void setColor(JCore::Color32 val) {
            _color = val;
        }

        void setX(uint32_t val) {
            _data = (_data & ~0xFFF) | (val & 0xFFF);
        }

        void setY(uint32_t val) {
            _data = (_data & ~0xFFF000) | ((val & 0xFFF) << 12);
        }

        void setAtlas(uint32_t atlas) {
            _data = (_data & ~0x1F000000) | ((atlas & 0x1F) << 24);
        }

        void setRotation(uint32_t rotation) {
            _data = (_data & ~0x60000000) | ((rotation & 0x3) << 29);
        }

        void setNull(bool value) {
            _data = value ? (_data | 0x80000000) : (_data & ~0x80000000);
        }

        constexpr JCore::Color32 getColor() const {
            return _color;
        }

        constexpr uint16_t getX() const {
            return uint32_t(_data & 0x0FFF);
        }

        constexpr uint16_t getY() const {
            return uint32_t((_data >> 12) & 0x0FFF);
        }

        constexpr uint8_t getAtlas() const {
            return uint8_t((_data >> 24) & 0x1F);
        }

        constexpr uint8_t getRotation() const {
            return uint8_t((_data >> 29) & 0x3);
        }

        constexpr bool isNull() const {
            return bool(_data & 0x80000000);
        }

    private:
        uint32_t _data;
        JCore::Color32 _color;
    };
    static constexpr TileData NullTile(0U, 0U, 0x80U);

    struct PFrameIndex {
    public:
        struct LData {
        public:
            constexpr LData() : _data() {}
            constexpr LData(int32_t value, bool flagVal) : _data(uint8_t(value & 0x7F) | uint8_t(flagVal ? 0x80 : 0x00)) {}

            constexpr operator uint8_t() const {
                return _data;
            }

            void setValue(int32_t value) {
                _data = (_data & 0x80) | uint8_t(value & 0x7F);
            }
            constexpr uint8_t getValue() const {
                return (_data & 0x7F);
            }

            void setFlag(bool value) {
                _data = (_data & 0x7F) | uint8_t(value ? 0x80 : 0x00);
            }
            constexpr bool getFlag() const {
                return bool(_data & 0x80);
            }
        private:
            uint8_t _data;
        };

        uint16_t frame;
        LData variation;
        uint8_t layer;

        constexpr PFrameIndex() : variation(), layer(), frame() {}
        constexpr PFrameIndex(int32_t frame, int32_t variation, int32_t layer, bool isEmissive) :
            frame(frame < 0 ? 0xFFFF : uint16_t(frame)),
            variation(variation < 0 ? 0xFF : uint8_t(variation), isEmissive),
            layer(layer)
        {}

        PFrameIndex(const std::string& path) : PFrameIndex(path.c_str(), path.size()) { }
        PFrameIndex(const char* path) : PFrameIndex(path, strlen(path)) {}
        PFrameIndex(const char* path, size_t size) : PFrameIndex() {
            using namespace JCore;
            ConstSpan<char> span(path, size);
            size_t curD = 0;
            size_t ind = span.lastIndexOf('_');

            if (ind == ConstSpan<char>::npos) {
                reinterpret_cast<uint32_t&>(*this) = 0x00U;
                return;
            }

            static constexpr size_t NPOS = ConstSpan<char>::npos;
            while (ind != NPOS) {
                auto cur = span.slice(0, ind);
                size_t nInd = cur.lastIndexOf('_');
                cur = span.slice(ind + 1);

                if (tolower(cur[0]) != 'e') {
                    switch (curD) {
                        default: goto end;
                        case 0:
                            Helpers::tryParseInt<uint16_t>(cur, frame, Helpers::IBase_10, 0);
                            break;
                        case 1: {
                            uint8_t lr = 0;
                            Helpers::tryParseInt<uint8_t>(cur, lr, Helpers::IBase_10, 0);
                            variation.setValue(lr);
                            break;
                        }
                        case 2:
                            Helpers::tryParseInt<uint8_t>(cur, layer, Helpers::IBase_10, 0);
                            break;
                    }
                    curD++;
                }
                else { variation.setFlag(true); }

            end:
                if (variation.getFlag() && curD >= 3) { break; }

                span = span.slice(0, ind);
                ind = span.slice(0, ind).lastIndexOf('_');
            }
        }

        bool operator==(const PFrameIndex& other) const {
            return reinterpret_cast<const uint32_t&>(*this) == reinterpret_cast<const uint32_t&>(other);
        }

        bool operator!=(const PFrameIndex& other) const {
            return reinterpret_cast<const uint32_t&>(*this) != reinterpret_cast<const uint32_t&>(other);
        }

        bool operator<(const PFrameIndex& other) const {
            return reinterpret_cast<const uint32_t&>(*this) < reinterpret_cast<const uint32_t&>(other);
        }

        bool operator>(const PFrameIndex& other) const {
            return reinterpret_cast<const uint32_t&>(*this) > reinterpret_cast<const uint32_t&>(other);
        }

        constexpr operator uint32_t() const {
            return uint32_t(frame) | (uint32_t(variation) << 8) | (uint32_t(layer) << 16);
        }
        PFrameIndex& operator=(uint32_t value) {
            *reinterpret_cast<uint32_t*>(this) = value;
            return *this;
        }
    };

    struct PFramePath {
        std::string path{};
        PFrameIndex index{};
        JCore::DataFormat format{};

        PFramePath() : path(""), index(), format() {}
        PFramePath(const std::string& str, PFrameIndex idx) : path(str), index(idx), format() {
            using namespace JCore;
            if (Helpers::endsWith(str.c_str(), ".png", false)) {
                format = FMT_PNG;
            }
            else if (Helpers::endsWith(str.c_str(), ".bmp", false)) {
                format = FMT_BMP;
            }
            else if (Helpers::endsWith(str.c_str(), ".dds", false)) {
                format = FMT_DDS;
            }
            else if (Helpers::endsWith(str.c_str(), ".jtex", false)) {
                format = FMT_JTEX;
            }
        }

        bool getInfo(JCore::ImageData& img) {
            using namespace JCore;
            switch (format)
            {
                case JCore::FMT_PNG:  return Png::getInfo(path.c_str(), img);
                case JCore::FMT_BMP:  return Bmp::getInfo(path.c_str(), img);
                case JCore::FMT_DDS:  return DDS::getInfo(path.c_str(), img);
                case JCore::FMT_JTEX: return JTEX::getInfo(path.c_str(), img);
                default: return false;
            }
        }

        bool decodeImage(JCore::ImageData& img) {
            using namespace JCore;
            switch (format)
            {
                case JCore::FMT_PNG:  return Png::decode(path.c_str(), img);
                case JCore::FMT_BMP:  return Bmp::decode(path.c_str(), img);
                case JCore::FMT_DDS:  return DDS::decode(path.c_str(), img);
                case JCore::FMT_JTEX: return JTEX::decode(path.c_str(), img);
                default: return false;
            }
        }

        constexpr bool isValid() const {
            return index != 0xFFFFFFFFU && path.length() > 0;
        }
    };

    struct PFrameVariation {
        bool hasEmission{};
        size_t pathStart{ 0 };
        size_t frames{ 0 };
        size_t minFrame = SIZE_MAX;

        void applyFrom(size_t variation, const std::vector<PFramePath>& paths) {
            pathStart = SIZE_MAX;
            minFrame = SIZE_MAX;

            size_t i = 0;
            hasEmission = false;
            for (auto& path : paths) {
                auto& index = path.index;
                if (index.variation.getValue() == variation) {
                    if (pathStart == SIZE_MAX) {
                        pathStart = i;
                    }
                    hasEmission |= index.variation.getFlag();
                    minFrame = std::min<size_t>(minFrame, index.frame);
                    frames = std::max<size_t>(size_t(index.frame) + 1, frames);
                }
                i++;
            }
            frames -= minFrame;
        }
    };

    static inline bool compare(const PFramePath& lhs, const PFramePath& rhs) {
        return lhs.index > rhs.index;
    }

    struct PFrameLayer {
        size_t variationCount{ 0 };
        std::vector<PFrameVariation> variations{};
        std::vector<PFramePath> paths{};

        int32_t indexOfPFrame(const std::vector<PFramePath>& paths, PFrameIndex idx) {
            for (size_t i = 0; i < paths.size(); i++) {
                if (paths[i].index == idx) { return int32_t(i); }
            }
            return -1;
        }

        void addPath(const std::string& path, PFrameIndex index) {
            if (indexOfPFrame(paths, index) < 0) {
                paths.emplace_back(path, index);
                variationCount = std::max<size_t>(index.variation.getValue() + 1, variationCount);
            }
        }

        void apply() {
            std::vector<PFramePath> temp(paths.begin(), paths.end());
            size_t frames = 0;
            variations.resize(variationCount);

            std::sort(temp.begin(), temp.end(), compare);
            for (size_t i = 0; i < variationCount; i++) {
                auto& vari = variations[i];
                vari.applyFrom(i, temp);
                frames += vari.frames * (vari.hasEmission ? 2 : 1);
            }
            paths.resize(frames);

            for (size_t i = 0; i < frames; i++) {
                paths[i].path = "";
                paths[i].index = PFrameIndex(0xFFFF, 0xFF, 0xFF, true);
            }

            size_t offsets[32]{ 0 };

            size_t offset = 0;
            for (size_t i = 0; i < variationCount; i++) {
                const auto& vari = variations[i];
                offsets[i] = offset;
                offset += vari.hasEmission ? vari.frames * 2 : vari.frames;
            }

            for (auto& tmp : temp) {
                auto& idx = tmp.index;
                size_t varInd = idx.variation.getValue();
                auto& vari = variations[varInd];
                offset = offsets[varInd];

                if (vari.minFrame > 0) {
                    idx.frame = uint16_t(idx.frame - vari.minFrame);
                }

                size_t cc = idx.frame + (idx.variation.getFlag() ? vari.frames : 0);
                paths[offset + cc] = tmp;
            }
        }
    };

    struct AnimationSpeed {
        AnimSpeedType type{};
        float duration{};
        uint16_t ticks{};

        void reset() {
            type = AnimSpeedType::ANIM_S_Ticks;
            ticks = 4;
            duration = 16.6f;
        }

        void write(json& jsonF) const {
            jsonF["type"] = int32_t(type);
            switch (type)
            {
                case Projections::ANIM_S_Ticks:
                    jsonF["ticksPerFrame"] = ticks;
                    break;
                case Projections::ANIM_S_Duration:
                    jsonF["duration"] = duration;
                    break;
            }
        }

        void write(const Stream& stream) const {
            stream.writeValue(type);
            switch (type)
            {
                case Projections::ANIM_S_Ticks:
                    stream.writeValue(ticks);
                    break;
                case Projections::ANIM_S_Duration:
                    stream.writeValue(duration);
                    break;
            }
        }

        void read(json& jsonF, AnimSpeedType type = ANIM_S_Ticks) {
            reset();
            if (jsonF.is_object()) {
                this->type = AnimSpeedType(jsonF.value("type", ANIM_S_Ticks));
                switch (this->type)
                {
                    case Projections::ANIM_S_Ticks:
                        ticks = jsonF.value<uint16_t>("ticksPerFrame", 4);
                        break;
                    case Projections::ANIM_S_Duration:
                        duration = jsonF.value<float>("duration", 16.6f);
                        break;
                }
                return;
            }

            this->type = type;
            if (jsonF.is_number()) {
                switch (this->type)
                {
                    case Projections::ANIM_S_Ticks:
                        ticks = jsonF.get<uint16_t>();
                        break;
                    case Projections::ANIM_S_Duration:
                        duration = jsonF.get<float>();
                        break;
                }
            }
        }
    };

    struct ProjectionVariation {
        std::string name{ "Default" };
        TileDrawMode drawMode{};
        bool directEmission{};
        AnimationMode animMode{};
        AnimationSpeed animSpeed{};
        int32_t loopStart{};

        void setDefaults() {
            name = "Default";
            drawMode = TileDrawMode::TDR_Default;
            animMode = AnimationMode::ANIM_None;
            directEmission = false;
            animSpeed.reset();
            loopStart = 0;
        }

        void copyFrom(const ProjectionVariation& other, VariationApplyMode copyMask) {
            if (copyMask & VariationApplyMode::VAR_APPLY_Name) {
                name = other.name;
            }

            if (copyMask & VariationApplyMode::VAR_APPLY_AnimMode) {
                animMode = other.animMode;
            }

            if (copyMask & VariationApplyMode::VAR_APPLY_AnimSpeed) {
                animSpeed = other.animSpeed;
            }

            if (copyMask & VariationApplyMode::VAR_APPLY_LoopStart) {
                loopStart = other.loopStart;
            }
        }

        void read(json& jsonF) {
            setDefaults();
            if (jsonF.is_object()) {
                name = jsonF.value("name", "Default");
                directEmission = jsonF.value<bool>("directEmission", false);
                animMode = AnimationMode(jsonF.value<int32_t>("animMode", 0));

                auto& aSpeed = jsonF["animationSpeed"];
                if (aSpeed.is_object()) {
                    animSpeed.read(aSpeed);
                }
                else {
                    aSpeed = jsonF["ticksPerFrame"];
                    if (aSpeed.is_number_unsigned()) {
                        animSpeed.read(aSpeed, AnimSpeedType::ANIM_S_Ticks);
                    }
                    else {
                        animSpeed.read(jsonF["frameDuration"], AnimSpeedType::ANIM_S_Duration);
                    }
                }
                loopStart = jsonF.value("loopStart", 0);
            }
        }

        void write(json& jsonF) const {
            jsonF["name"] = name;
            jsonF["directEmission"] = directEmission;
            jsonF["animMode"] = int32_t(animMode);
            animSpeed.write(jsonF["animationSpeed"]);
            jsonF["loopStart"] = loopStart;
        }

        void write(const Stream& stream) const {
            writeShortString(name, stream);
            stream.writeValue(drawMode);
            stream.writeValue(animMode);
            animSpeed.write(stream);
            stream.writeValue(uint16_t(loopStart));
        }
    };

    struct ProjectionLayer {
        std::string name{};
        uint8_t x{}, y{};
        uint16_t width{}, height{};

        uint8_t defaultVariation{ 0 };
        ProjectionVariation variations[32];

        void setDefault() {
            name = "Default";
            for (size_t i = 0; i < 32; i++) {
                variations[i].setDefaults();
            }
            defaultVariation = 0;
        }

        void read(json& jsonF, const PFrameLayer* frameLayer) {
            using namespace JCore;
            name = IO::readString(jsonF["name"], "Default");

            for (size_t i = 0; i < 32; i++) {
                variations[i].setDefaults();
            }

            auto& vars = jsonF["variations"];
            if (vars.is_array() && vars.size() > 0) {
                size_t len = std::min<size_t>(vars.size(), frameLayer ? frameLayer->variationCount : 32);
                for (size_t i = 0; i < len; i++) {
                    variations[i].read(vars[i]);
                }
            }
            else {
                vars = jsonF["variation"];
                if (vars.is_object()) {
                    variations[0].read(vars);
                    if (vars.value("useForAll", false)) {
                        if (frameLayer) {
                            for (size_t i = 1; i < frameLayer->variationCount; i++) {
                                variations[i] = variations[0];
                            }
                        }
                    }
                }
            }

            int32_t ind = jsonF.value<int32_t>("defaultVariation", 0);
            defaultVariation = uint8_t(ind < 0 ? 0xFF : ind);
        }

        void write(json& jsonF, const PFrameLayer* frameLayer) const {
            jsonF["name"] = name;
            if (frameLayer && frameLayer->variationCount > 1) {
                json::array_t arr{};
                for (size_t i = 0; i < frameLayer->variationCount; i++) {
                    json obj{};
                    variations[i].write(obj);
                    arr.push_back(obj);
                }
                jsonF["variations"] = arr;
            }
            else {
                variations[0].write(jsonF["variation"]);
            }
            jsonF["defaultVariation"] = int32_t(defaultVariation);
        }

        void write(const Stream& stream, const PFrameLayer& frameLayer) const {
            writeShortString(name, stream);
            stream.writeValue(x);
            stream.writeValue(y);

            stream.writeValue(width);
            stream.writeValue(height);
            stream.write(&frameLayer.variationCount, 1, false);
            stream.writeValue(defaultVariation);

            for (size_t i = 0; i < frameLayer.variationCount; i++) {
                auto& var = frameLayer.variations[i];
                stream.writeValue(uint16_t(var.frames));
                variations[i].write(stream);
            }
        }
    };

    struct ProjectionGroup;
    struct Projection {
        std::string name{};
        std::string description{};

        uint16_t width, height;
        std::vector<PixelTileIndex> tiles{};
        std::vector<ProjectionLayer> layers{};
        ProjectionMaterial material{};
        bool hasSameResolution{ true };

        int32_t lookOffset[2]{};
        float maxLookDistance{};

        std::string rootName{};
        fs::path rootPath{};
        std::vector<PFrameLayer> framesPaths{};

        bool isValid{ false };
        size_t maxTiles{};

        size_t getLayerCount() const { return std::min(framesPaths.size(), layers.size()); }

        void load(json& cfg, const fs::path& root, const std::string& rootName);
        void load(ProjectionGroup& group, bool premultiply, uint8_t alphaClip, JCore::ImageData& imgDat);

        void fixLayerCount() {
            if (isValid) {
                layers.resize(framesPaths.size());
            }
        }
        void reset() {
            tiles.clear();
            layers.clear();
            maxTiles = 0;
            hasSameResolution = true;
        }

        void read(json& jsonF, bool valid) {
            using namespace JCore;
            layers.clear();
            name = IO::readString(jsonF["name"], "");
            description = IO::readString(jsonF["description"], "");

            hasSameResolution = jsonF.value("sameResolution", true);

            isValid = valid;
            material.read(jsonF["material"]);
            auto& arr = jsonF["lookOffset"];
            if (arr.size() > 1) {
                lookOffset[0] = arr[0].get<int32_t>();
                lookOffset[1] = arr[1].get<int32_t>();
            }
            else {
                lookOffset[0] = 0;
                lookOffset[1] = 0;
            }
            maxLookDistance = jsonF.value("maxLookDistance", 256.0f);

            auto& lrs = jsonF["layers"];
            if (lrs.is_array() && lrs.size() > 0) {
                size_t i = 0;
                for (auto& layer : lrs) {
                    if (layer.is_object()) {
                        layers.emplace_back().read(layer, valid ? &framesPaths[i++] : nullptr);
                    }
                    if (i >= framesPaths.size()) { break; }
                }
            }
            else {
                lrs = jsonF["layer"];
                if (lrs.is_object()) {
                    layers.emplace_back().read(lrs, valid ? &framesPaths[0] : nullptr);
                }
                else {
                    layers.emplace_back().setDefault();
                }
            }
            fixLayerCount();

            if (isValid) {
                size_t reso = width * height;
                for (int32_t i = 0; i < layers.size(); i++) {
                    auto& lr = layers[i];
                    auto& lrP = framesPaths[i];
                    for (size_t j = 0; j < lrP.variationCount; j++) {
                        auto& vari = lrP.variations[j];
                        auto& variT = lr.variations[j];
                        variT.drawMode = vari.hasEmission ? TileDrawMode::TDR_WithGlow : TileDrawMode::TDR_Default;
                    }
                }
            }
        }
        void write(json& jsonF) const {
            jsonF["name"] = name;
            jsonF["description"] = description;

            material.write(jsonF["material"]);
            jsonF["lookOffset"] = json::array_t(lookOffset, lookOffset + 2);
            jsonF["maxLookDistance"] = maxLookDistance;
            jsonF["sameResolution"] = hasSameResolution;

            if (layers.size() > 1) {
                json::array_t arr{};
                size_t i = 0;
                for (auto& layer : layers) {
                    layer.write(arr.emplace_back(json()), isValid ? &framesPaths[i++] : nullptr);
                    if (i >= framesPaths.size()) { break; }
                }
                jsonF["layers"] = arr;
            }
            else if (layers.size() == 1) {
                layers[0].write(jsonF["layer"], isValid ? &framesPaths[0] : nullptr);
            }
        }

        void write(const ProjectionGroup& group, const Stream& stream, const std::vector<PixelTileCH>& rawTiles, const PixelBlockVector& rawBlocks) const;
    };

    struct ProjectionGroup;
    struct ProjectionConfig {
        std::string path{};
        std::string group{};
        fs::path root{};
        bool ignore{};
        std::vector<Projection> projections{};

        void clear() {
            projections.clear();
        }

        void save() const;
        void load(const fs::path& root);

        void addProjection(const char* path) {
            using namespace JCore;

            char buffer[512]{ 0 };
            std::string str = root.parent_path().string();
            size_t namePos = str.length();
            memcpy(buffer, str.c_str(), str.length());
            buffer[str.length()] = 0;
            char* nameStr = buffer + namePos;

            auto& proj = projections.emplace_back();
            proj.rootName = std::string(path);

            sprintf_s(nameStr, 260, "/%s", path);
            proj.rootPath = fs::path(buffer);
        }
    };

    struct ProjectionAtlas {
        int32_t width{}, height{};

        ProjectionAtlas() : width(0), height(0) {}

        void setup(uint8_t atlas, const JCore::AtlasDefiniton& definition, std::vector<PixelTileCH>& rawTiles) {
            using namespace JCore;
            width = definition.width;
            height = definition.height;

            size_t i = 0;
            for (auto& sprite : definition.atlas) {
                auto& rawTile = rawTiles[sprite.original][0];
                rawTile.setAtlas(atlas);
                rawTile.x = sprite.rect.x;
                rawTile.y = sprite.rect.y;
            }
        }

        void saveToPNG(int32_t padding, uint8_t atlas, const OutputFormat& format, JCore::ImageData buffer, const Stream& stream, const std::vector<PixelTileCH>& rawTiles, const PixelBlockVector& rawBlocks) const {
            using namespace JCore;
            buffer.width = width;
            buffer.height = height;

            switch (format.getSize())
            {
                default:
                    buffer.format = TextureFormat::RGBA32;
                    break;
                case A_EXP_S_RGBA4444:
                    buffer.format = TextureFormat::RGBA4444;
                    break;
            }

            size_t reso = size_t(width) * height;
            bool is4Bit = buffer.format == TextureFormat::RGBA4444;
            memset(buffer.data, 0, reso * (is4Bit ? 2 : 4));

            int32_t offsets[8]
            {
                0
            };

            if (is4Bit) {
                Color4444* pixels = reinterpret_cast<Color4444*>(buffer.data);
                for (size_t i = 0; i < rawTiles.size(); i++) {
                    auto& tile = rawTiles[i][0];
                    if (tile.getAtlas() != atlas) { continue; }
                    tile.writeTo(rawBlocks[i], pixels, width);

                    if (padding > 0) {
                        offsets[0] = tile.y * width + tile.x - 1;
                        offsets[1] = tile.y * width + tile.x;

                        offsets[2] = tile.y * width + tile.x + 16;
                        offsets[3] = tile.y * width + tile.x + 15;

                        offsets[4] = (tile.y - 1) * width + tile.x;
                        offsets[5] = tile.y * width + tile.x;

                        offsets[6] = (tile.y + 16) * width + tile.x;
                        offsets[7] = (tile.y + 15) * width + tile.x;

                        for (int32_t i = 0; i < 16; i++) {

                            pixels[offsets[0]] = pixels[offsets[1]];
                            pixels[offsets[2]] = pixels[offsets[3]];

                            pixels[offsets[4]] = pixels[offsets[5]];
                            pixels[offsets[6]] = pixels[offsets[7]];

                            offsets[0] += width;
                            offsets[1] += width;
                            offsets[2] += width;
                            offsets[3] += width;

                            offsets[4]++;
                            offsets[5]++;
                            offsets[6]++;
                            offsets[7]++;
                        }
                    }
                }
            }
            else {
                Color32* pixels = reinterpret_cast<Color32*>(buffer.data);
                for (size_t i = 0; i < rawTiles.size(); i++) {
                    auto& tile = rawTiles[i][0];
                    if (tile.getAtlas() != atlas) { continue; }
                    tile.writeTo(rawBlocks[i], pixels, width);
                    JCORE_ASSERT(tile.x + 16 < width && tile.x >= 0, "Something's wrong here!");

                    if (padding > 0) {
                        offsets[0] = tile.y * width + tile.x - 1;
                        offsets[1] = tile.y * width + tile.x;

                        offsets[2] = tile.y * width + tile.x + 16;
                        offsets[3] = tile.y * width + tile.x + 15;

                        offsets[4] = (tile.y - 1) * width + tile.x;
                        offsets[5] = tile.y * width + tile.x;

                        offsets[6] = (tile.y + 16) * width + tile.x;
                        offsets[7] = (tile.y + 15) * width + tile.x;

                        for (int32_t i = 0; i < 16; i++) {

                            pixels[offsets[0]] = pixels[offsets[1]];
                            pixels[offsets[2]] = pixels[offsets[3]];

                            pixels[offsets[4]] = pixels[offsets[5]];
                            pixels[offsets[6]] = pixels[offsets[7]];

                            offsets[0] += width;
                            offsets[1] += width;
                            offsets[2] += width;
                            offsets[3] += width;

                            offsets[4]++;
                            offsets[5]++;
                            offsets[6]++;
                            offsets[7]++;
                        }
                    }
                }
            }

            switch (format.type)
            {
                default:
                    Png::encode(stream, buffer);
                    break;
                case A_EXP_DDS:
                    DDS::encode(stream, buffer);
                    break;
                case A_EXP_JTEX:
                    JTEX::encode(stream, buffer);
                    break;
            }
        }
    };

    struct ProjectionGroup {
        fs::path root{};
        std::vector<fs::path> icons{};
        std::vector<ProjectionAtlas> atlases{};

        std::vector<PixelTileCH> rawTiles{};
        PixelBlockVector rawPIXTiles{};
        CRCBlockVector crcTiles{};
        bool useSlow{};

        void clear() {
            icons.clear();
            atlases.clear();

            rawTiles.clear();
            rawPIXTiles.clear();
            crcTiles.clear();
        }

        PixelTileIndex indexOfTileMemcmp(const uint32_t crc, const PixelBlock& block) const {
            uint32_t len = uint32_t(rawPIXTiles.size());

            const PixelBlock* ptr{ reinterpret_cast<const PixelBlock*>(rawPIXTiles.data() + 1) };
            const uint32_t* ptrP{ reinterpret_cast<const uint32_t*>(crcTiles.data() + 1) };

            for (uint32_t i = 1; i < len; i++) {
                if (crc == *ptrP++ && *ptr == block) {
                    return PixelTileIndex(i, 0);
                }
                ptr++;

                if (crc == *ptrP++ && *ptr == block) {
                    return PixelTileIndex(i, 1);
                }
                ptr++;

                if (crc == *ptrP++ && *ptr == block) {
                    return PixelTileIndex(i, 2);
                }
                ptr++;

                if (crc == *ptrP++ && *ptr == block) {
                    return PixelTileIndex(i, 3);
                }
                ptr++;
            }
            return PixelTileIndex(UINT32_MAX);
        }

        PixelTileIndex indexOfTileSIMD(uint32_t crc, const PixelBlock& block, size_t start = 0, size_t lenTgt = SIZE_MAX) const {
            const __m128i* n = reinterpret_cast<const __m128i*>(block.colors);
            uint32_t len = lenTgt == SIZE_MAX ? uint32_t(rawPIXTiles.size()) : uint32_t(lenTgt);

            const __m128i* ptr{ reinterpret_cast<const __m128i*>(rawPIXTiles.data() + start) };
            const __m128i* ptrP{ reinterpret_cast<const __m128i*>(crcTiles.data() + start) };

            __m128i crcBuf = _mm_set1_epi32(crc);
            __m128i m128Buf{};
            for (uint32_t k = start; k < len; k++) {
                m128Buf = _mm_cmpeq_epi32(*ptrP++, crcBuf);
                if (detail::getSimdORMask(m128Buf) != 0) {
                    const __m128i* pptr = ptr;
                    const __m128i* st = n;
                    if (m128Buf.m128i_u32[0] == 0xFFFFFFFFU) {
                        for (size_t i = 0, j = 0; i < 4; i++, j -= 16) {
                            if ((
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++))
                                ) != 0xFFFFFFFFU) {
                                goto skip000;
                            }
                        }
                        return PixelTileIndex(k, 0);
                    }
                skip000:
                    if (m128Buf.m128i_u32[1] == 0xFFFFFFFFU) {
                        pptr = ptr + 64;
                        st = n;
                        for (size_t i = 0, j = 0; i < 4; i++, j -= 16) {
                            if ((
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++))
                                ) != 0xFFFFFFFFU) {
                                goto skip001;
                            }
                        }
                        return PixelTileIndex(k, 1);
                    }
                skip001:

                    if (m128Buf.m128i_u32[2] == 0xFFFFFFFFU) {
                        pptr = ptr + 128;
                        st = n;
                        for (size_t i = 0, j = 0; i < 4; i++, j -= 16) {
                            if ((
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++))
                                ) != 0xFFFFFFFFU) {
                                goto skip002;
                            }
                        }
                        return PixelTileIndex(k, 2);
                    }
                skip002:

                    if (m128Buf.m128i_u32[3] == 0xFFFFFFFFU) {
                        pptr = ptr + 192;
                        st = n;
                        for (size_t i = 0, j = 0; i < 4; i++, j -= 16) {
                            if ((
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++)) &
                                detail::getSimdMask(_mm_cmpeq_epi32(*pptr++, *st++))
                                ) != 0xFFFFFFFFU) {
                                goto skip003;
                            }
                        }
                        return PixelTileIndex(k, 3);
                    }
                }
            skip003:
                ptr += 256;
            }
            if (start > 1) {
                return indexOfTileSIMD(crc, block, 1, start);
            }
            return PixelTileIndex(UINT32_MAX);
        }

        void pushNullTile() {
            rawTiles.emplace_back();

            auto& blocks = rawPIXTiles.emplace_back();
            auto& crcs = crcTiles.emplace_back();
            crcs.updateCrc(blocks.blocks[0], 0);
            crcs[1] = crcs[0];
            crcs[2] = crcs[0];
            crcs[3] = crcs[0];
        }

        void pushFullTile(JCore::Color32 color) {
            rawTiles.emplace_back();

            auto& blocks = rawPIXTiles.emplace_back(color);
            auto& crcs = crcTiles.emplace_back();

            crcs.updateCrc(blocks.blocks[0], 0);
            crcs[1] = crcs[0];
            crcs[2] = crcs[0];
            crcs[3] = crcs[0];
        }

        void doSave(const std::string& name, const OutputFormat& atlasFormat, bool premultiplyIcons, bool isDX9, char* path, char* dirName, JCore::ImageData img, std::vector<Projection*>& projections);
        void write(const std::string& name, const OutputFormat& atlasFormat, const Stream& stream, std::vector<Projection*>& projections) const;

        void buildAtlases(bool isDX9);
        void saveAtlases(JCore::ImageData img, const OutputFormat& atlasFormat, const char* root) const;

        int16_t addIcon(const fs::path& path);
        PixelTileIndex ProjectionGroup::addTile(CRCBlock& crc, PixelTile tiles[4], PixelBlock blocks[4], size_t start);
    };
#pragma pack(pop)
}

namespace JCore {
    template<>
    inline constexpr int32_t EnumNames<Projections::TileDrawMode>::Count{ 2 };

    template<>
    inline const char** EnumNames<Projections::TileDrawMode>::getEnumNames() {
        static const char* names[] =
        {
            "Default",
            "With Glow",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::RarityType>::Count{ 5 };

    template<>
    inline const char** EnumNames<Projections::RarityType>::getEnumNames() {
        static const char* names[] =
        {
            "Basic",
            "Intermediate",
            "Advanced",
            "Expert",
            "Master",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::DropType>::Count{ Projections::DropType::DROP_COUNT };

    template<>
    inline const char** EnumNames<Projections::DropType>::getEnumNames() {
        static const char* names[Projections::DropType::DROP_COUNT] =
        {
            "Null",
            "Enemy",
            "Chest",
            "Traveling Merchant",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::TileDrawLayer>::Count{ Projections::TileDrawLayer::TDRAW_COUNT };

    template<>
    inline const char** EnumNames<Projections::TileDrawLayer>::getEnumNames() {
        static const char* names[Projections::TileDrawLayer::TDRAW_COUNT] =
        {
            "Behind Walls",
            "Behind Tiles",
            "After Tiles",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 0>::Count{ 5 };

    template<>
    inline const char** EnumNames<Projections::BiomeFlags, 0>::getEnumNames() {
        static const char* names[Count] =
        {
            "Sky",
            "Surface",
            "Underground",
            "Caverns",
            "Underworld",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 1>::Start{ 5 };
    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 1>::Count{ 14 };

    template<>
    inline const char** EnumNames<Projections::BiomeFlags, 1>::getEnumNames() {
        static const char* names[Count] =
        {
            "Forest",
            "Desert",
            "Ocean",
            "Snow",
            "Jungle",
            "Meteorite",
            "Mushroom",
            "Dungeon",
            "Temple",
            "Aether",
            "Bee Hive",
            "Granite",
            "Marble",
            "Graveyard",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 2>::Start{ 19 };
    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 2>::Count{ 9 };

    template<>
    inline const char** EnumNames<Projections::BiomeFlags, 2>::getEnumNames() {
        static const char* names[Count] =
        {
            "Old Ones Army",
            "Town",
            "Water Candle",
            "Peace Candle",
            "Shadow Candle",
            "Solar Pillar",
            "Nebula Pillar",
            "Vortex Pillar",
            "Stardust Pillar",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 3>::Start{ 28 };

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 3>::Count{ 4 };

    template<>
    inline const char** EnumNames<Projections::BiomeFlags, 3>::getEnumNames() {
        static const char* names[Count] =
        {
            "Purity",
            "Corruption",
            "Crimson",
            "Hallow",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 4>::Start{ 32 };
    template<>
    inline constexpr int32_t EnumNames<Projections::BiomeFlags, 4>::Count{ 4 };

    template<>
    inline const char** EnumNames<Projections::BiomeFlags, 4>::getEnumNames() {
        static const char* names[Count] =
        {
            "Elevation",
            "Biome",
            "Effect",
            "Purity/Evil",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::WorldConditions>::Count{ 15 };

    template<>
    inline const char** EnumNames<Projections::WorldConditions>::getEnumNames() {
        static const char* names[EnumNames<Projections::WorldConditions>::Count] =
        {
            "Is Hardmode",
            "Is Expert",
            "Is Master",
            "Is Day",
            "Is Night",
            "Is Blood Moon",
            "Is Eclipse",
            "Is Pumpkin Moon",
            "Is Frost Moon",
            "Is Halloween",
            "Is Christmas",
            "Is Sandstorm",
            "Is Raining",
            "Is Storming",
            "Is Crimson",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::WorldConditions, 1>::Count{ 33 };

    template<>
    inline constexpr int32_t EnumNames<Projections::WorldConditions, 1>::Start{ 15 };

    template<>
    inline const char** EnumNames<Projections::WorldConditions, 1>::getEnumNames() {
        static const char* names[EnumNames<Projections::WorldConditions, 1>::Count] =
        {
            "King Slime",
            "Eyes of Cthulhu",
            "Eater of Worlds",
            "Brain of Cthulhu",
            "Queen Bee",
            "Skeletron",
            "Deerclops",
            "Wall of Flesh",
            "Queen Slime",
            "The Twins",
            "The Destroyer",
            "Skeletron Prime",
            "Plantera",
            "Golem",
            "Duke Fishron",
            "Empress of Light",
            "Lunatic Cultist",
            "Moon Lord",
            "Mourning Wood",
            "Pumpking",
            "Everscream",
            "Santa-NK1",
            "Ice Queen",
            "Solar Pillar",
            "Nebula Pillar",
            "Vortex Pillar",
            "Stardust Pillar",
            "Goblin Army",
            "Pirates",
            "Frost Legion",
            "Martians",
            "Pumpkin Moon",
            "Frost Moon",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::AnimationMode>::Count{ Projections::AnimationMode::ANIM_COUNT };

    template<>
    inline const char** EnumNames<Projections::AnimationMode>::getEnumNames() {
        static const char* names[EnumNames<Projections::AnimationMode>::Count] =
        {
            "None",
            "Loop",
            "Follow",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::GenFlags>::Count{ 4 };

    template<>
    inline const char** EnumNames<Projections::GenFlags>::getEnumNames() {
        static const char* names[EnumNames<Projections::GenFlags>::Count] =
        {
            "No SIMD",
            "Premultiply (Tiles)",
            "Premultiply (Icons)",
            "Is DX9",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::VariationApplyMode>::Count{ 4 };

    template<>
    inline const char** EnumNames<Projections::VariationApplyMode>::getEnumNames() {
        static const char* names[EnumNames<Projections::VariationApplyMode>::Count] =
        {
            "Name",
            "Animation Mode",
            "Animation Speed",
            "Loop Start",
        };
        return names;
    }


    template<>
    inline constexpr int32_t EnumNames<Projections::AtlasExportType>::Count{ 3 };

    template<>
    inline const char** EnumNames<Projections::AtlasExportType>::getEnumNames() {
        static const char* names[EnumNames<Projections::AtlasExportType>::Count] =
        {
            "PNG",
            "DDS",
            "JTEX",
        };
        return names;
    }


    template<>
    inline constexpr int32_t EnumNames<Projections::AtlasExportSize>::Count{ 2 };

    template<>
    inline const char** EnumNames<Projections::AtlasExportSize>::getEnumNames() {
        static const char* names[EnumNames<Projections::AtlasExportSize>::Count] =
        {
            "RGBA32",
            "RGBA4444",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::AtlasExportSize, 1>::Count{ 1 };

    template<>
    inline const char** EnumNames<Projections::AtlasExportSize, 1>::getEnumNames() {
        static const char* names[EnumNames<Projections::AtlasExportSize, 1>::Count] =
        {
            "RGBA32",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::AnimSpeedType>::Count{ 2 };

    template<>
    inline const char** EnumNames<Projections::AnimSpeedType>::getEnumNames() {
        static const char* names[EnumNames<Projections::AnimSpeedType>::Count] =
        {
            "Ticks Per Frame",
            "Frame Duration",
        };
        return names;
    }

    template<>
    inline constexpr int32_t EnumNames<Projections::NPCID, 0>::Count{ Projections::NPCID::NPC_Count };

    template<>
    inline constexpr int32_t EnumNames<Projections::NPCID, 0>::Start{ Projections::NPCID::NPC_MIN };


    template<>
    inline constexpr int32_t EnumNames<Projections::NPCID, 1>::Count{ Projections::NPCID::NPC_Count };

    template<>
    inline constexpr int32_t EnumNames<Projections::NPCID, 1>::Start{ Projections::NPCID::NPC_MIN };

    template<>
    inline const char** EnumNames<Projections::NPCID, 0>::getEnumNames() {
        static const char* names[EnumNames<Projections::NPCID, 0>::Count] =
        {
        "Big Hornet Stingy",
        "Little Hornet Stingy",
        "Big Hornet Spikey",
        "Little Hornet Spikey",
        "Big Hornet Leafy",
        "Little Hornet Leafy",
        "Big Hornet Honey",
        "Little Hornet Honey",
        "Big Hornet Fatty",
        "Little Hornet Fatty",
        "Big Rain Zombie",
        "Small Rain Zombie",
        "Big Pantless Skeleton",
        "Small Pantless Skeleton",
        "Big Misassembled Skeleton",
        "Small Misassembled Skeleton",
        "Big Headache Skeleton",
        "Small Headache Skeleton",
        "Big Skeleton",
        "Small Skeleton",
        "Big Female Zombie",
        "Small Female Zombie",
        "Demon Eye 2",
        "Purple Eye 24",
        "Green Eye 2",
        "Dialated Eye 2",
        "Sleepy Eye 2",
        "Cataract Eye 2",
        "Big Twiggy Zombie",
        "Small Twiggy Zombie",
        "Big Swamp Zombie",
        "Small Swamp Zombie",
        "Big Slimed Zombie",
        "Small Slimed Zombie",
        "Big Pincushion Zombie",
        "Small Pincushion Zombie",
        "Big Bald Zombie",
        "Small Bald Zombie",
        "Big Zombie",
        "Small Zombie",
        "Big Crimslime",
        "Little Crimslime",
        "Big Crimera",
        "Little Crimera",
        "Giant Moss Hornet",
        "Big Moss Hornet",
        "Little Moss Hornet",
        "Tiny Moss Hornet",
        "Big Stinger",
        "Little Stinger",
        "Heavy Skeleton",
        "Big Boned",
        "Short Bones",
        "Big Eater",
        "Little Eater",
        "Jungle Slime",
        "Yellow Slime",
        "Red Slime",
        "Purple Slime",
        "Black Slime",
        "Baby Slime",
        "Pinky",
        "Green Slime",
        "Slimer2",
        "Slimeling",
        "Any",
        "Blue Slime",
        "Demon Eye",
        "Zombie",
        "Eye of Cthulhu",
        "Servant of Cthulhu",
        "Eater of Souls",
        "Devourer",
        "Devourer Body",
        "Devourer Tail",
        "Giant Worm",
        "Giant Worm Body",
        "Giant Worm Tail",
        "Eater of Worlds",
        "Eater of Worlds Body",
        "Eater of Worlds Tail",
        "Mother Slime",
        "Merchant",
        "Nurse",
        "Arms Dealer",
        "Dryad",
        "Skeleton",
        "Guide",
        "Meteor Head",
        "Fire Imp",
        "Burning Sphere",
        "Goblin Peon",
        "Goblin Thief",
        "Goblin Warrior",
        "Goblin Sorcerer",
        "Chaos Ball",
        "Angry Bones",
        "Dark Caster",
        "Water Sphere",
        "Cursed Skull",
        "Skeletron",
        "Skeletron Hand",
        "Old Man",
        "Demolitionist",
        "Bone Serpent",
        "Bone Serpent Body",
        "Bone Serpent Tail",
        "Hornet",
        "Man Eater",
        "Undead Miner",
        "Tim",
        "Bunny",
        "Corrupt Bunny",
        "Harpy",
        "Cave Bat",
        "King Slime",
        "Jungle Bat",
        "Doctor Bones",
        "The Groom",
        "Clothier",
        "Goldfish",
        "Snatcher",
        "Corrupt Goldfish",
        "Piranha",
        "Lava Slime",
        "Hellbat",
        "Vulture",
        "Demon",
        "Blue Jellyfish",
        "Pink Jellyfish",
        "Shark",
        "Voodoo Demon",
        "Crab",
        "Dungeon Guardian",
        "Antlion",
        "Spike Ball",
        "Dungeon Slime",
        "Blazing Wheel",
        "Goblin Scout",
        "Bird",
        "Pixie",
        "BUFFER 0",
        "Armored Skeleton",
        "Mummy",
        "Dark Mummy",
        "Light Mummy",
        "Corrupt Slime",
        "Wraith",
        "Cursed Hammer",
        "Enchanted Sword",
        "Mimic",
        "Unicorn",
        "Wyvern",
        "Wyvern Legs",
        "Wyvern Body 0",
        "Wyvern Body 1",
        "Wyvern Body 2",
        "Wyvern Tail",
        "Giant Bat",
        "Corruptor",
        "Digger",
        "Digger Body",
        "Digger Tail",
        "World Feeder",
        "World Feeder Body",
        "World Feeder Tail",
        "Clinger",
        "Angler Fish",
        "Green Jellyfish",
        "Werewolf",
        "Bound Goblin",
        "Bound Wizard",
        "Goblin Tinkerer",
        "Wizard",
        "Clown",
        "Skeleton Archer",
        "Goblin Archer",
        "Vile Spit",
        "Wall of Flesh",
        "Wall of Flesh Eye",
        "The Hungry",
        "The Hungry II",
        "Leech",
        "Leech Body",
        "Leech Tail",
        "Chaos Elemental",
        "Slimer",
        "Gastropod",
        "Bound Mechanic",
        "Mechanic",
        "Retinazer",
        "Spazmatism",
        "Skeletron Prime",
        "Prime Cannon",
        "Prime Saw",
        "Prime Vice",
        "Prime Laser",
        "Zombie Bald",
        "Wandering Eye",
        "The Destroyer",
        "The Destroyer Body",
        "The Destroyer Tail",
        "Illuminant Bat",
        "Illuminant Slime",
        "Probe",
        "Possessed Armor",
        "Toxic Sludge",
        "Santa Claus",
        "Snowman Gangsta",
        "Mister Stabby",
        "Snow Balla",
        "ER 1",
        "Ice Slime",
        "Penguin",
        "Penguin Black",
        "Ice Bat",
        "Lava Bat",
        "Giant Flying Fox",
        "Giant Tortoise",
        "Ice Tortoise",
        "Wolf",
        "Red Devil",
        "Arapaima",
        "Vampire Bat",
        "Vampire",
        "Truffle",
        "Zombie Eskimo",
        "Frankenstein",
        "Black Recluse",
        "Wall Creeper",
        "Wall Creeper Wall",
        "Swamp Thing",
        "Undead Viking",
        "Corrupt Penguin",
        "Ice Elemental",
        "Pigron Corrupt",
        "Pigron Hallow",
        "Rune Wizard",
        "Crimera",
        "Herpling",
        "Angry Trapper",
        "Moss Hornet",
        "Derpling",
        "Steampunker",
        "Crimson Axe",
        "Pigron Crimson",
        "Face Monster",
        "Floaty Gross",
        "Crimslime",
        "Spiked Ice Slime",
        "Snow Flinx",
        "Pincushion Zombie",
        "Slimed Zombie",
        "Swamp Zombie",
        "Twiggy Zombie",
        "Cataract Eye",
        "Sleepy Eye",
        "Dialated Eye",
        "Green Eye",
        "Purple Eye",
        "Lost Girl",
        "Nymph",
        "Armored Viking",
        "Lihzahrd",
        "Lihzahrd Crawler",
        "Female Zombie",
        "Headache Skeleton",
        "Misassembled Skeleton",
        "Pantless Skeleton",
        "Spiked Jungle Slime",
        "Moth",
        "Icy Merman",
        "Dye Trader",
        "Party Girl",
        "Cyborg",
        "Bee",
        "Bee Small",
        "Pirate Deckhand",
        "Pirate Corsair",
        "Pirate Deadeye",
        "Pirate Crossbower",
        "Pirate Captain",
        "Cochineal Beetle",
        "Cyan Beetle",
        "Lac Beetle",
        "Sea Snail",
        "Squid",
        "Queen Bee",
        "Raincoat Zombie",
        "Flying Fish",
        "Umbrella Slime",
        "Flying Snake",
        "Painter",
        "Witch Doctor",
        "Pirate",
        "Goldfish Walker",
        "Hornet Fatty",
        "Hornet Honey",
        "Hornet Leafy",
        "Hornet Spikey",
        "Hornet Stingy",
        "Jungle Creeper",
        "Jungle Creeper Wall",
        "Black Recluse Wall",
        "Blood Crawler",
        "Blood Crawler Wall",
        "Blood Feeder",
        "Blood Jelly",
        "Ice Golem",
        "Rainbow Slime",
        "Golem",
        "Golem Head",
        "Golem Fist Left",
        "Golem Fist Right",
        "Golem Head Free",
        "Angry Nimbus",
        "Eyezor",
        "Parrot",
        "Reaper",
        "Spore Zombie",
        "Spore Zombie Hat",
        "Fungo Fish",
        "Anomura Fungus",
        "Mushi Ladybug",
        "Fungi Bulb",
        "Giant Fungi Bulb",
        "Fungi Spore",
        "Plantera",
        "Planteras Hook",
        "Planteras Tentacle",
        "Spore",
        "Brain of Cthulhu",
        "Creeper",
        "Ichor Sticker",
        "Rusty Armored Bones Axe",
        "Rusty Armored Bones Flail",
        "Rusty Armored Bones Sword",
        "Rusty Armored Bones Sword No Armor",
        "Blue Armored Bones",
        "Blue Armored Bones Mace",
        "Blue Armored Bones No Pants",
        "Blue Armored Bones Sword",
        "Hell Armored Bones",
        "Hell Armored Bones Spike Shield",
        "Hell Armored Bones Mace",
        "Hell Armored Bones Sword",
        "Ragged Caster",
        "Ragged Caster Open Coat",
        "Necromancer",
        "Necromancer Armored",
        "Diabolist",
        "Diabolist White",
        "Bone Lee",
        "Dungeon Spirit",
        "Giant Cursed Skull",
        "Paladin",
        "Skeleton Sniper",
        "Tactical Skeleton",
        "Skeleton Commando",
        "Angry Bones Big",
        "Angry Bones Big Muscle",
        "Angry Bones Big Helmet",
        "Blue Jay",
        "Cardinal",
        "Squirrel",
        "Mouse",
        "Raven",
        "Slime Masked",
        "Bunny Slimed",
        "Hoppin Jack",
        "Scarecrow",
        "Scarecrow 2",
        "Scarecrow 3",
        "Scarecrow 4",
        "Scarecrow 5",
        "Scarecrow 6",
        "Scarecrow 7",
        "Scarecrow 8",
        "Scarecrow 9",
        "Scarecrow 10",
        "Headless Horseman",
        "Ghost",
        "Demon Eye Owl",
        "Demon Eye Spaceship",
        "Zombie Doctor",
        "Zombie Superman",
        "Zombie Pixie",
        "Skeleton Top Hat",
        "Skeleton Astronaut",
        "Skeleton Alien",
        "Mourning Wood",
        "Splinterling",
        "Pumpking",
        "Pumpking Scythe",
        "Hellhound",
        "Poltergeist",
        "Zombie Xmas",
        "Zombie Sweater",
        "Slime Ribbon White",
        "Slime Ribbon Yellow",
        "Slime Ribbon Green",
        "Slime Ribbon Red",
        "Bunny Xmas",
        "Zombie Elf",
        "Zombie Elf Beard",
        "Zombie Elf Girl",
        "Present Mimic",
        "Gingerbread Man",
        "Yeti",
        "Everscream",
        "Ice Queen",
        "Santa",
        "Elf Copter",
        "Nutcracker",
        "Nutcracker Spinning",
        "Elf Archer",
        "Krampus",
        "Flocko",
        "Stylist",
        "Webbed Stylist",
        "Firefly",
        "Butterfly",
        "Worm",
        "Lightning Bug",
        "Snail",
        "Glowing Snail",
        "Frog",
        "Duck",
        "Duck 2",
        "Duck White",
        "Duck White 2",
        "Scorpion Black",
        "Scorpion",
        "Traveling Merchant",
        "Angler",
        "Duke Fishron",
        "Detonating Bubble",
        "Sharkron",
        "Sharkron 2",
        "Truffle Worm",
        "Truffle Worm Digger",
        "Sleeping Angler",
        "Grasshopper",
        "Chattering Teeth Bomb",
        "Blue Cultist Archer",
        "White Cultist Archer",
        "Brain Scrambler",
        "Ray Gunner",
        "Martian Officer",
        "Bubble Shield",
        "Gray Grunt",
        "Martian Engineer",
        "Tesla Turret",
        "Martian Drone",
        "Gigazapper",
        "Scutlix Gunner",
        "Scutlix",
        "Martian Saucer",
        "Martian Saucer Turret",
        "Martian Saucer Cannon",
        "Martian Saucer Core",
        "Moon Lord",
        "Moon Lords Hand",
        "Moon Lords Core",
        "Martian Probe",
        "Moon Lord Free Eye",
        "Moon Leech Clot",
        "Milkyway Weaver",
        "Milkyway Weaver Body",
        "Milkyway Weaver Tail",
        "Star Cell",
        "Star Cell Mini",
        "Flow Invader",
        "BUFFER 1",
        "Twinkle Popper",
        "Twinkle",
        "Stargazer",
        "Crawltipede",
        "Crawltipede Body",
        "Crawltipede Tail",
        "Drakomire",
        "Drakomire Rider",
        "Sroller",
        "Corite",
        "Selenian",
        "Nebula Floater",
        "Brain Suckler",
        "Vortex Pillar",
        "Evolution Beast",
        "Predictor",
        "Storm Diver",
        "Alien Queen",
        "Alien Hornet",
        "Alien Larva",
        "Zombie Armed",
        "Zombie Frozen",
        "Zombie Armed Pincushion",
        "Zombie Armed Frozen",
        "Zombie Armed Slimed",
        "Zombie Armed Swamp",
        "Zombie Armed Twiggy",
        "Zombie Armed Female",
        "Mysterious Tablet",
        "Lunatic Devotee",
        "Lunatic Cultist",
        "Lunatic Cultist Clone",
        "Tax Collector",
        "Gold Bird",
        "Gold Bunny",
        "Gold Butterfly",
        "Gold Frog",
        "Gold Grasshopper",
        "Gold Mouse",
        "Gold Worm",
        "Skeleton Bone Throwing",
        "Skeleton Bone Throwing 2",
        "Skeleton Bone Throwing 3",
        "Skeleton Bone Throwing 4",
        "Skeleton Merchant",
        "Phantasm Dragon",
        "Phantasm Dragon Body 1",
        "Phantasm Dragon Body 2",
        "Phantasm Dragon Body 3",
        "Phantasm Dragon Body 4",
        "Phantasm Dragon Tail",
        "Butcher",
        "Creature from the Deep",
        "Fritz",
        "Nailhead",
        "Crimtane Bunny",
        "Crimtane Goldfish",
        "Psycho",
        "Deadly Sphere",
        "Dr Man Fly",
        "The Possessed",
        "Vicious Penguin",
        "Goblin Summoner",
        "Shadowflame Apparation",
        "Corrupt Mimic",
        "Crimson Mimic",
        "Hallowed Mimic",
        "Jungle Mimic",
        "Mothron",
        "Mothron Egg",
        "Baby Mothron",
        "Medusa",
        "Hoplite",
        "Granite Golem",
        "Granite Elemental",
        "Enchanted Nightcrawler",
        "Grubby",
        "Sluggy",
        "Buggy",
        "Target Dummy",
        "Blood Zombie",
        "Drippler",
        "Stardust Pillar",
        "Crawdad",
        "Crawdad 2",
        "Giant Shelly",
        "Giant Shelly 2",
        "Salamander",
        "Salamander 2",
        "Salamander 3",
        "Salamander 4",
        "Salamander 5",
        "Salamander 6",
        "Salamander 7",
        "Salamander 8",
        "Salamander 9",
        "Nebula Pillar",
        "Antlion Charger Giant",
        "Antlion Swarmer Giant",
        "Dune Splicer",
        "Dune Splicer Body",
        "Dune Splicer Tail",
        "Tomb Crawler",
        "Tomb Crawler Body",
        "Tomb Crawler Tail",
        "Solar Flare",
        "Solar Pillar",
        "Drakanian",
        "Solar Fragment",
        "Martian Walker",
        "Ancient Vision",
        "Ancient Light",
        "Ancient Doom",
        "Ghoul",
        "Vile Ghoul",
        "Tainted Ghoul",
        "Dreamer Ghoul",
        "Lamia",
        "Lamia Dark",
        "Sand Poacher",
        "Sand Poacher Wall",
        "Basilisk",
        "Desert Spirit",
        "Tortured Soul",
        "Spiked Slime",
        "The Bride",
        "Sand Slime",
        "Red Squirrel",
        "Gold Squirrel",
        "Bunny Party",
        "Sand Elemental",
        "Sand Shark",
        "Bone Biter",
        "Flesh Reaver",
        "Crystal Thresher",
        "Angry Tumbler",
        "QuestionMark",
        "Eternia Crystal",
        "Mysterious Portal",
        "Tavernkeep",
        "Betsy",
        "Etherian Goblin",
        "Etherian Goblin 2",
        "Etherian Goblin 3",
        "Etherian Goblin Bomber",
        "Etherian Goblin Bomber 2",
        "Etherian Goblin Bomber 3",
        "Etherian Wyvern",
        "Etherian Wyvern 2",
        "Etherian Wyvern 3",
        "Etherian Javelin Thrower",
        "Etherian Javelin Thrower 2",
        "Etherian Javelin Thrower 3",
        "Dark Mage",
        "Dark Mage T3",
        "Old Ones Skeleton",
        "Old Ones Skeleton T3",
        "Wither Beast",
        "Wither Beast T3",
        "Drakin",
        "Drakin T3",
        "Kobold",
        "Kobold T3",
        "Kobold Glider",
        "Kobold Glider T3",
        "Ogre",
        "Ogre T3",
        "Etherian Lightning Bug",
        "Unconscious Man",
        "Walking Charger",
        "Flying Antlion",
        "Antlion Larva",
        "Pink Fairy",
        "Green Fairy",
        "Blue Fairy",
        "Zombie Merman",
        "Wandering Eye Fish",
        "Golfer",
        "Golfer Rescue",
        "Zombie Torch",
        "Zombie Armed Torch",
        "Gold Goldfish",
        "Gold Goldfish Walker",
        "Windy Balloon",
        "Dragonfly Black",
        "Dragonfly Blue",
        "Dragonfly Green",
        "Dragonfly Orange",
        "Dragonfly Red",
        "Dragonfly Yellow",
        "Dragonfly Gold",
        "Seagull",
        "Seagull 2",
        "Ladybug",
        "Gold Ladybug",
        "Maggot",
        "Pupfish",
        "Grebe",
        "Grebe 2",
        "Rat",
        "Owl",
        "Water Strider",
        "Water Strider Gold",
        "Explosive Bunny",
        "Dolphin",
        "Turtle",
        "Turtle Jungle",
        "Dreadnautilus",
        "Blood Squid",
        "Hemogoblin Shark",
        "Blood Eel",
        "Blood Eel Body",
        "Blood Eel Tail",
        "Gnome",
        "Sea Turtle",
        "Sea Horse",
        "Sea Horse Gold",
        "Angry Dandelion",
        "Ice Mimic",
        "Blood Mummy",
        "Rock Golem",
        "Maggot Zombie",
        "Zoologist",
        "Spore Bat",
        "Spore Skeleton",
        "Empress of Light",
        "Town Cat",
        "Town Dog",
        "Amethyst Squirrel",
        "Topaz Squirrel",
        "Sapphire Squirrel",
        "Emerald Squirrel",
        "Ruby Squirrel",
        "Diamond Squirrel",
        "Amber Squirrel",
        "Amethyst Bunny",
        "Topaz Bunny",
        "Sapphire Bunny",
        "Emerald Bunny",
        "Ruby Bunny",
        "Diamond Bunny",
        "Amber Bunny",
        "Hell Butterfly",
        "Lavafly",
        "Magma Snail",
        "Town Bunny",
        "Queen Slime",
        "Crystal Slime",
        "Bouncy Slime",
        "Heavenly Slime",
        "Prismatic Lacewing",
        "Pirate Ghost",
        "Princess",
        "Toch God",
        "Chaos Ball Tim",
        "Vile Spit EoW",
        "Golden Slime",
        "Deerclops",
        "Stinkbug",
        "Nerdy Slime",
        "Scarlet Macaw",
        "Blue Macaw",
        "Toucan",
        "Yellow Cokatiel",
        "Gray Cokatiel",
        "Shimmer Slime",
        "Faeling",
        "Cool Slime",
        "Elder Slime",
        "Clumsy Slime",
        "Diva Slime",
        "Surly Slime",
        "Squire Slime",
        "Old Shaking Chest",
        "Clumsy Balloon Slime",
        "Mystic Frog",
        };
        return names;
    }

    template<>
    inline const char** EnumNames<Projections::NPCID, 1>::getEnumNames() {
        return  EnumNames<Projections::NPCID, 0>::getEnumNames();
    }

    template<>
    inline bool EnumNames<Projections::NPCID, 0>::noDraw(int32_t index) {
        using namespace Projections;
        auto names = getEnumNames();
        if (index < 0 || index >= Count || !names || index < -Start) { return true; }

        switch (Projections::NPCID(index + Start))
        {
            case NPCID::NPC_BUFFER_0:
            case NPCID::NPC_BUFFER_1:

            case NPCID::NPC_Devourer_Body:
            case NPCID::NPC_Devourer_Tail:
            case NPCID::NPC_Giant_Worm_Body:
            case NPCID::NPC_Giant_Worm_Tail:
            case NPCID::NPC_Eater_of_Worlds_Body:
            case NPCID::NPC_Eater_of_Worlds_Tail:
            case NPCID::NPC_Skeletron_Hand:
            case NPCID::NPC_Bone_Serpent_Body:
            case NPCID::NPC_Bone_Serpent_Tail:
            case NPCID::NPC_Spike_Ball:
            case NPCID::NPC_Wyvern_Legs:
            case NPCID::NPC_Wyvern_Body_0:
            case NPCID::NPC_Wyvern_Body_1:
            case NPCID::NPC_Wyvern_Body_2:
            case NPCID::NPC_Wyvern_Tail:
            case NPCID::NPC_Digger_Body:
            case NPCID::NPC_Digger_Tail:
            case NPCID::NPC_World_Feeder_Body:
            case NPCID::NPC_World_Feeder_Tail:
            case NPCID::NPC_Wall_of_Flesh_Eye:
            case NPCID::NPC_Leech_Body:
            case NPCID::NPC_Leech_Tail:
            case NPCID::NPC_Dune_Splicer_Body:
            case NPCID::NPC_Dune_Splicer_Tail:
            case NPCID::NPC_Tomb_Crawler_Body:
            case NPCID::NPC_Tomb_Crawler_Tail:
            case NPCID::NPC_Crawltipede_Body:
            case NPCID::NPC_Crawltipede_Tail:
            case NPCID::NPC_Milkyway_Weaver_Body:
            case NPCID::NPC_Milkyway_Weaver_Tail:
            case NPCID::NPC_Phantasm_Dragon_Body_1:
            case NPCID::NPC_Phantasm_Dragon_Body_2:
            case NPCID::NPC_Phantasm_Dragon_Body_3:
            case NPCID::NPC_Phantasm_Dragon_Body_4:
            case NPCID::NPC_Phantasm_Dragon_Tail:
            case NPCID::NPC_Golem_Fist_Left:
            case NPCID::NPC_Golem_Fist_Right:
            case NPCID::NPC_Golem_Head:
            case NPCID::NPC_Golem_Head_Free:
            case NPCID::NPC_The_Destroyer_Body:
            case NPCID::NPC_The_Destroyer_Tail:
                return true;
        }

        return isNoName(names[index]);
    }

    template<>
    inline bool EnumNames<Projections::NPCID, 1>::noDraw(int32_t index) {
        using namespace Projections;
        auto names = getEnumNames();
        if (index < 0 || index >= Count || !names) { return true; }
        switch (Projections::NPCID(index + Start)) {
            case NPCID::NPC_BUFFER_0:
            case NPCID::NPC_BUFFER_1:
                return true;
        }
        return isNoName(names[index]);
    }



}