#pragma once
#include <algorithm>
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
#include <J-Core/IO/AudioUtils.h>
#include <J-Core/IO/Audio.h>
#include <J-Core/IO/Stream.h>
#include <J-Core/Util/DataUtils.h>
#include <J-Core/Util/StringUtils.h>
#include <J-Core/Util/EnumUtils.h>
#include <J-Core/Util/Span.h>
#include <J-Core/Rendering/Atlas.h>
#include <J-Core/Util/DataFormatUtils.h>
#include <J-Core/Util/DataUtils.h>
#include <J-Core/Util/PoolAllocator.h>

#include <smmintrin.h>
#include <J-Core/Util/AlignmentAllocator.h>

namespace Projections {
    static constexpr int32_t PROJ_GEN_VERSION = 2;

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

    struct alignas(16) CRCBlocks {
        uint32_t crcs[8]{ 0 };

        void clear() {
            memset(crcs, 0, sizeof(crcs));
        }

        bool from(const uint8_t* data, size_t size) {
            using namespace JCore;
            clear();
            if (size < 0 || !data) { return false; }
            size_t chunkSize = Math::max(size >> 3, 1ULL);

            size_t left = size;
            size_t ind = 0;
            uint32_t crcV = 0x00;
            while (left > 0 && ind < 8) {
                size_t len = Math::min(left, chunkSize);
                crcV |= (crcs[ind++] = JCore::Data::calcuateCRC(data, len));
                data += len;
                left -= len;
            }
            return crcV != 0x00;
        }

        const bool operator==(const CRCBlocks& other) const {
            const __m128i* lhs = reinterpret_cast<const __m128i*>(crcs);
            const __m128i* rhs = reinterpret_cast<const __m128i*>(other.crcs);
            return (
                detail::getSimdMask(_mm_cmpeq_epi32(lhs[0], rhs[0])) &
                detail::getSimdMask(_mm_cmpeq_epi32(lhs[1], rhs[1]))
                ) == 0xFFFFFFFFU;

        }

        const bool operator!=(const CRCBlocks& other) const {
            const __m128i* lhs = reinterpret_cast<const __m128i*>(crcs);
            const __m128i* rhs = reinterpret_cast<const __m128i*>(other.crcs);
            return (
                detail::getSimdMask(_mm_cmpeq_epi32(lhs[0], rhs[0])) &
                detail::getSimdMask(_mm_cmpeq_epi32(lhs[1], rhs[1]))
                ) != 0xFFFFFFFFU;

        }
    };

    enum TexMode : uint8_t {
        TEX_None = 0x00,
        TEX_PNG,
        TEX_DDS,
        TEX_JTEX,
        TEX_RLE,

        __TEX_COUNT
    };

    enum PoolType : uint8_t {
        Pool_None,
        Pool_Trader,
        Pool_NPC,
        Pool_FishingQuest,
        Pool_Treasure,
        Pool_COUNT,
    };

    enum ProjectionLimits : uint64_t {
        PROJ_MAX_LAYERS = 16,
        PROJ_MAX_RESOLUTION = 1024,
    };

    enum RarityType : uint8_t {
        RARITY_Basic,
        RARITY_Intermediate,
        RARITY_Advanced,
        RARITY_Expert,
        RARITY_Master,


        RARITY_COUNT,
    };

    enum PMaterialFlags : uint8_t {
        PMat_None = 0x00,
        PMat_AllowUpload = 0x01,
        PMat_AllowShimmer = 0x02,
    };

    enum PLayerFlags : uint32_t {
        PLa_None = 0x00,
        PLa_DefaultState = 0x01,
        PLa_IsTransparent = 0x10000,
    };

    enum PFrameFlags : uint32_t {
        PFrm_None = 0x00,
    };


    /// <summary>
    ///  LAST ID IS -65, REMEMBER TO OFFSET!!!!
    /// </summary>
    enum NPCID : uint32_t {
        NPC_Big_Hornet_Stingy,
        NPC_Little_Hornet_Stingy,
        NPC_Big_Hornet_Spikey,
        NPC_Little_Hornet_Spikey,
        NPC_Big_Hornet_Leafy,
        NPC_Little_Hornet_Leafy,
        NPC_Big_Hornet_Honey,
        NPC_Little_Hornet_Honey,
        NPC_Big_Hornet_Fatty,
        NPC_Little_Hornet_Fatty,
        NPC_Big_Rain_Zombie,
        NPC_Small_Rain_Zombie,
        NPC_Big_Pantless_Skeleton,
        NPC_Small_Pantless_Skeleton,
        NPC_Big_Misassembled_Skeleton,
        NPC_Small_Misassembled_Skeleton,
        NPC_Big_Headache_Skeleton,
        NPC_Small_Headache_Skeleton,
        NPC_Big_Skeleton,
        NPC_Small_Skeleton,
        NPC_Big_Female_Zombie,
        NPC_Small_Female_Zombie,
        NPC_Demon_Eye_2,
        NPC_Purple_Eye_24,
        NPC_Green_Eye_2,
        NPC_Dialated_Eye_2,
        NPC_Sleepy_Eye_2,
        NPC_Cataract_Eye_2,
        NPC_Big_Twiggy_Zombie,
        NPC_Small_Twiggy_Zombie,
        NPC_Big_Swamp_Zombie,
        NPC_Small_Swamp_Zombie,
        NPC_Big_Slimed_Zombie,
        NPC_Small_Slimed_Zombie,
        NPC_Big_Pincushion_Zombie,
        NPC_Small_Pincushion_Zombie,
        NPC_Big_Bald_Zombie,
        NPC_Small_Bald_Zombie,
        NPC_Big_Zombie,
        NPC_Small_Zombie,
        NPC_Big_Crimslime,
        NPC_Little_Crimslime,
        NPC_Big_Crimera,
        NPC_Little_Crimera,
        NPC_Giant_Moss_Hornet,
        NPC_Big_Moss_Hornet,
        NPC_Little_Moss_Hornet,
        NPC_Tiny_Moss_Hornet,
        NPC_Big_Stinger,
        NPC_Little_Stinger,
        NPC_Heavy_Skeleton,
        NPC_Big_Boned,
        NPC_Short_Bones,
        NPC_Big_Eater,
        NPC_Little_Eater,
        NPC_Jungle_Slime,
        NPC_Yellow_Slime,
        NPC_Red_Slime,
        NPC_Purple_Slime,
        NPC_Black_Slime,
        NPC_Baby_Slime,
        NPC_Pinky,
        NPC_Green_Slime,
        NPC_Slimer2,
        NPC_Slimeling,
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

        NPC_Count,
        NPC_OFFSET = 65,
        NPC_NET_START = 0,
        NPC_MAIN_START = NPC_Any,
        NPC_MAIN_COUNT = NPC_Count - NPC_MAIN_START,
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

        WCF_ValidOnGen = WCF_IsExpert | WCF_IsMaster,
    };

    enum AnimationMode : uint8_t {
        ANIM_FrameSet = 0x00,
        ANIM_LoopVideo = 0x01,
        ANIM_LoopAudio = 0x02,
        ANIM_COUNT,
    };

    enum AudioMode : uint8_t {
        Aud_SFX = 0x00,
        Aud_Music = 0x01,
        Aud_Ambient = 0x02,

        Aud_COUNT,
        Aud_IsStereo = 0x80,
    };


    enum UIFlags : uint8_t {
        UI_None = 0x00,
        UI_Duplicate = 0x01,
        UI_Remove = 0x02,

        UI_DisableDuplicate = 0x20,
        UI_DisableRemove = 0x40,
        UI_IsElement = 0x80,
    };

    enum PType : uint8_t {
        PTY_Projection = 0x00,
        PTY_Material = 0x01,
        PTY_Bundle = 0x02,


        PTY_Count,
    };

    enum RecipeType :uint8_t {
        RECIPE_NONE = 0x00,
        RECIPE_VANILLA,
        RECIPE_MODDED,
        RECIPE_PROJECTION,
        RECIPE_PROJECTION_MAT,
        RECIPE_PROJECTION_BUN,
        __RECIPE_TYPE_COUNT,
    };

    template<size_t maxSize = 256>
    static inline void writeShortString(const std::string& str, const Stream& stream) {
        uint8_t len = uint8_t(str.length() > maxSize ? maxSize : str.length());
        stream.writeValue(len);
        stream.write(str.c_str(), len, false);
    }

    inline uint32_t calculateNameHash(std::string_view name) {
        if (name.length() < 1) {
            return 0;
        }

        uint32_t hash = JCore::Data::calcuateCRC(name);
        return hash != 0 ? hash : 1;
    }

    struct alignas(16) Palette {
        static constexpr int32_t SIZE = UINT16_MAX + 1;
        static constexpr int32_t HISTORY_SIZE = 16;
        static constexpr int32_t HISTORY_SIZE_BIG = 128;

        JCore::Color32 colors[SIZE]{};
        int32_t count{};

        int32_t histSize;
        uint32_t historyC{ 0 };
        int32_t history[HISTORY_SIZE_BIG]{};

        void clear() {
            count = 0;
            historyC = 0;
            histSize = HISTORY_SIZE - 1;
            memset(colors, 0, sizeof(colors));
            memset(&history, 0xFF, sizeof(history));
        }

        template<typename T>
        static __forceinline void swap(T& lhs, T& rhs) {
            T tmp = rhs;
            rhs = lhs;
            lhs = rhs;
        }

        int32_t indexOf(JCore::Color32 color) const {
            using namespace JCore;
            if (count > (SIZE >> 2)) {
                static constexpr int32_t OFFFSET_LUT[64]{
                    0 , 0 , 0 , 0 ,
                    1 , 1 , 1 , 1 ,
                    2 , 2 , 2 , 2 ,
                    3 , 3 , 3 , 3 ,
                    4 , 4 , 4 , 4 ,
                    5 , 5 , 5 , 5 ,
                    6 , 6 , 6 , 6 ,
                    7 , 7 , 7 , 7 ,
                    8 , 8 , 8 , 8 ,
                    9 , 9 , 9 , 9 ,
                    10, 10, 10, 10,
                    11, 11, 11, 11,
                    12, 12, 12, 12,
                    13, 13, 13, 13,
                    14, 14, 14, 14,
                    15, 15, 15, 15,
                };

                __m128i key = _mm_set1_epi32(reinterpret_cast<int32_t&>(color));
                const __m128i* simdPtr = reinterpret_cast<const __m128i*>(colors);

                int32_t sCount = count >> 5;
                uint64_t mask = 0;
                for (int32_t i = 0, j = 0; i < sCount; i++, j += 32) {
                    mask = uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key)));
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 16;
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 32;
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 48;

                    if (mask != 0) {
                        return i + OFFFSET_LUT[Math::findFirstLSB(mask)];
                    }

                    mask = uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key)));
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 16;
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 32;
                    mask |= uint64_t(_mm_movemask_epi8(_mm_cmpeq_epi32(*simdPtr++, key))) << 48;

                    if (mask != 0) {
                        return (i + 16) + OFFFSET_LUT[Math::findFirstLSB(mask)];
                    }
                }

                sCount <<= 4;
                for (int32_t i = sCount; i < count; i++) {
                    if (colors[i] == color) {
                        return i;
                    }
                }
                return -1;
            }

            for (int32_t i = 0; i < count; i++) {
                if (colors[i] == color) {
                    return i;
                }
            }
            return -1;
        }

        int32_t add(JCore::Color32 color) {
            using namespace JCore;
            int32_t index;
            for (int32_t i = 0; i <= histSize; i++) {
                if (colors[history[i]] == color) {
                    index = history[i];
                    if (i) {
                        swap(history[i], history[i - 1]);
                    }
                    return index;
                }
            }

            index = indexOf(color);
            if (index < 0 && count < SIZE) {
                index = count++;
                colors[index] = color;
                histSize = (count > 1024 ? HISTORY_SIZE_BIG - 1 : HISTORY_SIZE - 1);
            }

            if (index > -1) {
                history[historyC++ & histSize] = index;
            }
            return index;
        }
    };

    struct PBuffers {
        JCore::ImageData readBuffer{};
        JCore::ImageData frameBuffer{};
        JCore::AudioData audioBuffer{};

        uint8_t* idxUI8{};
        uint16_t* idxUI16{};

        bool noPalette{ false };
        Palette palettePrev{};
        Palette palette{};

        void init(int32_t maxResolution, int32_t baseSamples) {
            using namespace JCore;
            readBuffer.doAllocate(maxResolution, maxResolution, TextureFormat::RGBA32);
            frameBuffer.doAllocate(maxResolution, maxResolution, TextureFormat::RGBA32);
            audioBuffer.doAllocate(AudioFormat::PCM, AudioSampleType::Signed, 16, 2, baseSamples);

            if (idxUI8 != nullptr) { free(idxUI8); }
            idxUI8 = reinterpret_cast<uint8_t*>(malloc(size_t(maxResolution) * maxResolution * 3));
            idxUI16 = reinterpret_cast<uint16_t*>(idxUI8 + maxResolution * maxResolution);
            palettePrev.clear();
            palette.clear();
            noPalette = false;
        }

        void clear() {
            using namespace JCore;
            char temp[256]{ 0 };
            palettePrev.clear();
            palette.clear();

            Utils::formatDataSize(temp, readBuffer.getBufferSize());
            JCORE_INFO("Freeing Read  Buffer %s", temp);
            readBuffer.clear(true);

            Utils::formatDataSize(temp, frameBuffer.getBufferSize());
            JCORE_INFO("Freeing Frame Buffer %s", temp);
            frameBuffer.clear(true);

            Utils::formatDataSize(temp, audioBuffer.getBufferSize());
            JCORE_INFO("Freeing Audio Buffer %s", temp);
            audioBuffer.clear(true);

            noPalette = false;
            if (idxUI8) {
                free(idxUI8);
                idxUI8 = nullptr;
                idxUI16 = nullptr;
            }
        }
    };

    struct ProjectionIndex {
        uint32_t lo{ 0 }, hi{ 0 };

        ProjectionIndex() noexcept = default;
        ProjectionIndex(std::string_view view) : ProjectionIndex() {
            size_t ind = view.find(':', 0);
            if (ind != std::string_view::npos) {
                lo = calculateNameHash(view.substr(0, ind));
                hi = calculateNameHash(view.substr(ind + 1));
            }
        }

        constexpr bool isValid() const {
            return (lo & hi) != 0x00;
        }
    };

    struct CoinData {
        static constexpr int32_t COPPER_SIZE = 100;
        static constexpr int32_t SILVER_SIZE = COPPER_SIZE * 100;
        static constexpr int32_t GOLD_SIZE = SILVER_SIZE * 100;

        int32_t total{};

        constexpr CoinData() : total(0) {}
        constexpr CoinData(int32_t total) : total(total) {}
        constexpr CoinData(int32_t copper, int32_t silver, int32_t gold, int32_t platinum) : total(0) {
            merge(copper, silver, gold, platinum);
        }

        constexpr void extract(int32_t& copper, int32_t& silver, int32_t& gold, int32_t& platinum) const {
            copper = (total % COPPER_SIZE);
            silver = (total % SILVER_SIZE) / COPPER_SIZE;
            gold = ((total % GOLD_SIZE) / SILVER_SIZE);
            platinum = total / GOLD_SIZE;
        }

        constexpr void merge(int32_t copper, int32_t silver, int32_t gold, int32_t platinum) {
            total = copper + silver * COPPER_SIZE + gold * SILVER_SIZE + platinum * GOLD_SIZE;
        }

        void write(const Stream& stream) const {
            stream.writeValue(total, 1, false);
        }

        void write(json& jsonF) const {
            int32_t c{ 0 }, s{ 0 }, g{ 0 }, p{ 0 };
            extract(c, s, g, p);
            jsonF["copper"] = c;
            jsonF["silver"] = s;
            jsonF["gold"] = g;
            jsonF["platinum"] = p;
        }

        void read(const json& jsonF) {
            int32_t c{ 0 }, s{ 0 }, g{ 0 }, p{ 0 };
            if (jsonF.is_object()) {
                c = jsonF.value("copper", 0);
                s = jsonF.value("silver", 0);
                g = jsonF.value("gold", 0);
                p = jsonF.value("platinum", 0);
            }
            merge(c, s, g, p);
        }
    };

    struct PathNum {
        size_t loIndex{ 0 };
        size_t hiIndex{ 0 };

        size_t getCount() const { return hiIndex - loIndex; }
    };

    struct RecipeIngredient {
        uint8_t uiFlags{};

        RecipeType type{};
        std::string itemName{};
        int32_t itemID{ 0 };
        int32_t count{ 1 };

        RecipeIngredient() noexcept = default;

        void reset() {
            type = RECIPE_NONE;
            itemID = 0;
            itemName = "";
            count = 1;
        }

        void read(const json& jsonF) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                type = jsonF.value("type", RECIPE_NONE);
                count = Math::max(jsonF.value("count", 1), 1);
                switch (type)
                {
                case Projections::RECIPE_VANILLA:
                    itemID = jsonF.value("value", 0);
                    break;
                case Projections::RECIPE_PROJECTION:
                case Projections::RECIPE_PROJECTION_MAT:
                case Projections::RECIPE_PROJECTION_BUN:
                case Projections::RECIPE_MODDED:
                    itemName = IO::readString(jsonF, "value", "");
                    break;
                }
            }
        }

        void write(json& jsonF) const {
            jsonF["type"] = type;
            jsonF["count"] = count;
            switch (type) {
            case Projections::RECIPE_VANILLA:
                jsonF["value"] = itemID;
                break;
            case Projections::RECIPE_PROJECTION:
            case Projections::RECIPE_PROJECTION_MAT:
            case Projections::RECIPE_PROJECTION_BUN:
            case Projections::RECIPE_MODDED:
                jsonF["value"] = itemName;
                break;
            }
        }

        void write(const Stream& stream) const {
            using namespace JCore;
            stream.writeValue(type, 1, false);
            stream.writeValue(Math::max(count, 1), 1, false);
            switch (type)
            {
            default:
                return;
            case RECIPE_VANILLA:
                stream.writeValue(itemID);
                break;
            case RECIPE_MODDED:
            case RECIPE_PROJECTION:
            case RECIPE_PROJECTION_MAT:
            case RECIPE_PROJECTION_BUN:
                writeShortString(itemName, stream);
                break;
            }
        }
    };

    struct RecipeItem {
        uint8_t uiFlags{};
        std::vector<RecipeIngredient> ingredients{};

        void reset() {
            ingredients.resize(1);
            ingredients[0].reset();
        }

        void write(const Stream& stream, uint32_t count) const {
            RecipeIngredient defV{};
            for (size_t i = 0; i <= count; i++) {
                if (i >= ingredients.size()) {
                    defV.write(stream);
                    continue;
                }
                ingredients[i].write(stream);
            }
        }
        void write(json& jsonF) const {
            json::array_t arr{};
            for (size_t i = 0; i < ingredients.size(); i++) {
                ingredients[i].write(arr.emplace_back(json{}));
            }
            jsonF = arr;
        }
        void read(const json& jsonF) {
            using namespace JCore;
            reset();
            const json* arr = nullptr;
            if (jsonF.is_array()) {
                arr = &jsonF;
            }
            else if (jsonF.is_object()) {
                arr = &JCore::IO::getObject(jsonF, "ingredients");
            }

            if (arr != nullptr) {

                size_t count = Math::min<size_t>(1, arr->size());
                ingredients.resize(count);

                for (size_t i = 0; i < count; i++)
                {
                    ingredients[i].reset();
                }

                for (size_t i = 0; i < arr->size(); i++) {
                    ingredients[i].read((*arr)[i]);
                }
            }
        }

        void duplicate(size_t index) {
            if (index >= ingredients.size()) { return; }
            RecipeIngredient copy = ingredients[index];
            ingredients.insert(ingredients.begin() + index, copy);
        }

        void removeAt(size_t index) {
            if (index >= ingredients.size()) { return; }
            ingredients.erase(ingredients.begin() + index);
        }
    };

    struct Recipe {
        uint8_t uiFlags{};
        std::vector<RecipeItem> items{};

        void reset() {
            items.resize(1);
            items[0].reset();
        }

        void read(const json& jsonF) {
            reset();

            if (!jsonF.is_object()) {
                return;
            }

            auto& arr = JCore::IO::getObject(jsonF, "recipe");
            for (size_t i = 0; i < arr.size(); i++) {
                this->items.emplace_back().read(arr[i]);
            }
        }

        void write(json& jsonF) const {
            json::array_t arr{};
            for (size_t i = 0; i < items.size(); i++) {
                items[i].write(arr.emplace_back());
            }
            jsonF["recipe"] = arr;
        }

        void write(const Stream& stream) const {
            using namespace JCore;
            int32_t maxAlts = 0;
            for (size_t i = 0; i < items.size(); i++) {
                maxAlts = Math::max(int32_t(items[i].ingredients.size()) - 1, maxAlts);
            }
            maxAlts = Math::min(maxAlts, 255);

            uint8_t altCount = uint8_t(maxAlts);
            stream.writeValue<uint8_t>(altCount, 1, false);
            stream.writeValue<int32_t>(int32_t(items.size()), 1, false);
            for (size_t i = 0; i < items.size(); i++) {
                items[i].write(stream, altCount);
            }
        }

        void duplicate(size_t index) {
            if (index >= items.size()) { return; }
            RecipeItem copy = items[index];
            items.insert(items.begin() + index, copy);
        }

        void removeAt(size_t index) {
            if (index >= items.size()) { return; }
            items.erase(items.begin() + index);
        }
    };

    enum DropFlags : uint8_t {
        DROP_None,
        DROP_Modded = 0x1,
        DROP_NetID = 0x2,
    };

    struct PConditions {
        WorldConditions worldConditions{};
        BiomeFlags biomeConditions{};

        void reset() {
            worldConditions = WorldConditions::WCF_None;
            biomeConditions = BiomeFlags::BIOME_None;
        }

        void read(const json& jsonF) {
            reset();

            if (jsonF.is_object()) {
                worldConditions = jsonF.value("worldConditions", WorldConditions::WCF_None);
                biomeConditions = jsonF.value("biomeConditions", BiomeFlags::BIOME_None);
            }
        }

        void write(json& jsonF) const {
            jsonF["worldConditions"] = worldConditions;
            jsonF["biomeConditions"] = biomeConditions;
        }

        void write(const Stream& stream) const {
            stream.writeValue(worldConditions);
            stream.writeValue(biomeConditions);
        }
    };

    struct ItemConditions {
        PConditions conditions{};
        float chance{ 1.0f };
        float weight{ 1.0f };

        void reset() {
            conditions.reset();
            chance = 1.0f;
            weight = 1.0f;
        }

        void read(const json& jsonF) {
            reset();

            if (jsonF.is_object()) {
                conditions.read(JCore::IO::getObject(jsonF, "conditions"));
                chance = jsonF.value("chance", 1.0f);
                weight = jsonF.value("weight", 1.0f);
            }
        }

        void write(json& jsonF) const {
            conditions.write(jsonF["conditions"]);
            jsonF["chance"] = chance;
            jsonF["weight"] = weight;
        }

        void write(const Stream& stream) const {
            conditions.write(stream);
            stream.writeValue(chance);
            stream.writeValue(weight);
        }
    };

    struct SourceID {
        int32_t id;
        std::string strId;

        void reset() {

        }
    };

    struct DropSource {
        uint8_t uiFlags{};

        PoolType pool{};
        ItemConditions conditions{};
        DropFlags flags{};
        int32_t stack{};
        int32_t id{};
        std::string strId{};

        void reset() {
            pool = PoolType::Pool_None;
            conditions.reset();
            flags = DropFlags::DROP_None;
            stack = 1;
            id = 0;
            strId = "";
        }

        bool isModded() const {
            return (flags & DropFlags::DROP_Modded) != 0;
        }

        bool isNetID() const {
            return (flags & DropFlags::DROP_NetID) != 0;
        }

        void setIsModded(bool value) {
            flags = DropFlags(value ? flags | DROP_Modded : flags & ~DROP_Modded);
        }

        void setIsNetID(bool value) {
            flags = DropFlags(value ? flags | DROP_NetID : flags & ~DROP_NetID);
        }

        void read(const json& jsonF) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                pool = jsonF.value("pool", PoolType::Pool_None);
                stack = Math::max(jsonF.value("stack", 1), 1);
                flags = jsonF.value("flags", DROP_None);
                conditions.read(JCore::IO::getObject(jsonF, "conditions"));

                if (flags & DROP_Modded) {
                    strId = JCore::IO::readString(jsonF, "source", "");
                }
                else {
                    id = jsonF.value("source", 0);
                }
            }
        }

        void write(json& jsonF) const {
            jsonF["pool"] = pool;
            conditions.write(jsonF["conditions"]);
            jsonF["stack"] = stack;
            jsonF["flags"] = flags;

            if (flags & DROP_Modded) {
                jsonF["source"] = strId;
            }
            else {
                jsonF["source"] = id;
            }
        }

        void write(const Stream& stream) const {
            stream.writeValue(pool);
            conditions.write(stream);
            stream.writeValue(stack);
            stream.writeValue(flags);

            if (flags & DROP_Modded) {
                writeShortString(strId, stream);
            }
            else {
                stream.writeValue(stack);
            }
        }
    };

    struct Tag {
        std::string value{};

        void reset() {
            value = std::string{};
        }

        void write(json& jsonF) const {
            jsonF = value;
        }

        void read(const json& jsonF) {
            reset();
            value = JCore::IO::readString(jsonF, "");
        }

        static void pushIfNotFound(std::string_view str, std::vector<std::string>& tags) {
            if (JCore::Utils::isWhiteSpace(str)) { return; }
            for (size_t i = 0; i < tags.size(); i++) {
                if (JCore::Utils::strIEquals(str, tags[i])) {
                    return;
                }
            }
            auto& nTag = tags.emplace_back(str);
            for (size_t i = 0; i < nTag.length(); i++) {
                nTag[i] = char(tolower(nTag[i]));
            }
        }

        void addTag(std::vector<std::string>& tags, std::string_view root) const {
            using namespace JCore;
            std::string_view view = value;

            if (Utils::endsWith(view, ".tags", false)) {
                FileStream fs(IO::combine(root, view), "rb");
                std::string line{};
                if (fs.isOpen()) {
                    while (fs.readLine(line)) {
                        std::string_view lineTrim = Utils::trim(line);
                        if (Utils::isWhiteSpace(lineTrim)) {
                            continue;
                        }
                        pushIfNotFound(lineTrim, tags);
                    }
                }
                return;
            }
            view = Utils::trim(view);
            if (!Utils::isWhiteSpace(view)) {
                pushIfNotFound(Utils::trim(view), tags);
            }
        }
    };

    struct PMaterial {
        bool noExport{};

        std::string nameID{};
        std::string name{};
        std::string description{};

        RarityType rarity{};
        int32_t priority{ 0 };
        PMaterialFlags flags{ PMaterialFlags::PMat_AllowShimmer };
        CoinData coinValue{};

        std::vector<DropSource> sources{};
        std::vector<Recipe> recipes{};

        TexMode iconMode{};
        std::string icon{};
        std::string_view root{};

        void reset() {
            nameID = "";
            name = "";
            description = "";
            rarity = RARITY_Basic;
            priority = 0;
            flags = PMaterialFlags::PMat_AllowShimmer;
            coinValue = CoinData(50, 1, 0, 0);

            sources.clear();
            recipes.clear();

            noExport = false;
            iconMode = TexMode::TEX_None;
            icon = "";
        }

        void read(const json& jsonF, std::string_view root) {
            using namespace JCore;
            reset();
            this->root = IO::getDirectory(root);
            if (jsonF.is_object()) {
                nameID = IO::readString(jsonF, "nameID", "");
                name = IO::readString(jsonF, "name", "");
                description = IO::readString(jsonF, "description", "");
                rarity = jsonF.value("rarity", RARITY_Basic);
                priority = jsonF.value("priority", 0);
                flags = jsonF.value("flags", PMaterialFlags::PMat_AllowShimmer);
                coinValue.read(JCore::IO::getObject(jsonF, "coinValue"));

                auto& srcs = JCore::IO::getObject(jsonF, "sources");
                size_t count = srcs.size();

                sources.resize(count);
                for (size_t i = 0; i < count; i++)
                {
                    sources[i].read(srcs[i]);
                }
                auto& rec = JCore::IO::getObject(jsonF, "recipes");
                count = rec.size();

                recipes.resize(count);
                for (size_t i = 0; i < count; i++)
                {
                    recipes[i].read(rec[i]);
                }

                iconMode = jsonF.value("iconMode", TexMode::TEX_None);
                icon = IO::readString(jsonF, "icon", "");
                IO::eraseRoot(icon, root);
            }
        }

        void write(json& jsonF) const {
            jsonF["nameID"] = nameID;
            jsonF["name"] = name;
            jsonF["description"] = description;
            jsonF["rarity"] = rarity;
            jsonF["priority"] = priority;
            jsonF["flags"] = flags;
            coinValue.write(jsonF["coinValue"]);

            json::array_t srcA{};
            for (auto& src : sources)
            {
                src.write(srcA.emplace_back(json{}));
            }
            jsonF["sources"] = srcA;

            json::array_t recA{};
            for (auto& rec : recipes)
            {
                rec.write(recA.emplace_back(json{}));
            }
            jsonF["recipes"] = recA;

            jsonF["iconMode"] = iconMode;
            jsonF["icon"] = icon;
        }

        void write(const Stream& stream) const;

        void duplicateRecipe(size_t index) {
            if (index >= recipes.size()) { return; }
            Recipe copy = recipes[index];
            recipes.insert(recipes.begin() + index, copy);
        }

        void removeRecipeAt(size_t index) {
            if (index >= recipes.size()) { return; }
            recipes.erase(recipes.begin() + index);
        }

        void duplicateSource(size_t index) {
            if (index >= sources.size()) { return; }
            DropSource copy = sources[index];
            sources.insert(sources.begin() + index, copy);
        }

        void removeSourceAt(size_t index) {
            if (index >= sources.size()) { return; }
            sources.erase(sources.begin() + index);
        }
    };

    struct PFrameIndex {
    public:
        uint32_t data;

        constexpr PFrameIndex() : data() {}
        constexpr PFrameIndex(int32_t frame, int32_t layer, bool isEmissive) :
            data((frame & 0x7FFFFFU) | ((frame & 0x7FU) << 23) | (isEmissive ? 0x80000000U : 0x00))
        {}

        constexpr PFrameIndex(uint32_t data) :
            data(data)
        {}

        PFrameIndex(std::string_view view) : PFrameIndex() {
            static constexpr size_t NPOS = std::string_view::npos;
            using namespace JCore;
            size_t curD = 0;
            size_t ind = view.find_last_of('.');

            if (ind == NPOS) {
                reinterpret_cast<uint32_t&>(*this) = 0x00U;
                return;
            }

            view = view.substr(0, ind);
            ind = view.find_last_of('_');

            data = 0;
            while (ind != NPOS) {
                auto cur = view.substr(ind + 1);
                /*               size_t nInd = cur.find_last_of('_');
                               cur = nInd != std::string_view::npos ?  cur.substr(0, nInd) : cur;*/

                if (tolower(cur[0]) == 'e') {
                    data |= 0x80000000U;
                }

                switch (curD) {
                default: goto end;
                case 0: {
                    uint32_t frame{ 0 };
                    Utils::tryParseInt<uint32_t>(cur, frame, Utils::IBase_10, 0);
                    data |= (frame & 0x7FFFFFU);
                    break;
                }
                case 1: {
                    uint32_t frame{ 0 };
                    Utils::tryParseInt<uint32_t>(cur, frame, Utils::IBase_10, 0);
                    data |= (frame & 0x7FU) << 23;
                    break;
                }
                }
                curD++;

            end:
                if ((data & 0x80000000U) && curD >= 1) { break; }

                view = view.substr(0, ind);
                ind = view.substr(0, ind).find_last_of('_');
            }
        }

        constexpr bool isEmissive() const { return (data & 0x80000000U) != 0; }
        constexpr void setIndex(uint32_t index) {
            data = (data & ~0x7FFFFFU) | (index & 0x7FFFFFU);
        }

        constexpr int32_t getIndex() const { return int32_t(data & 0x7FFFFFU); }
        constexpr int32_t getLayer() const { return int32_t(data >> 23) & 0x7F; }

        bool operator==(const PFrameIndex& other) const {
            return data == other.data;
        }

        bool operator!=(const PFrameIndex& other) const {
            return data != other.data;
        }

        bool operator<(const PFrameIndex& other) const {
            return data < other.data;
        }

        bool operator>(const PFrameIndex& other) const {
            return data > other.data;
        }

        constexpr operator uint32_t() const {
            return data;
        }
        PFrameIndex& operator=(uint32_t value) {
            data = value;
            return *this;
        }
    };
    static constexpr PFrameIndex NullIdx = PFrameIndex(0xFFFFFFFFU);

    struct PFramePath {
        std::string path{};
        PFrameIndex index{};
        JCore::DataFormat format{};

        PFramePath() : path(""), index(), format() {}
        PFramePath(std::string_view str, PFrameIndex idx) : path(str), index(idx), format() {
            using namespace JCore;
            if (Utils::endsWith(str, ".png", false)) {
                format = FMT_PNG;
            }
            else if (Utils::endsWith(str, ".bmp", false)) {
                format = FMT_BMP;
            }
            else if (Utils::endsWith(str, ".dds", false)) {
                format = FMT_DDS;
            }
            else if (Utils::endsWith(str, ".jtex", false)) {
                format = FMT_JTEX;
            }
            else {
                format = FMT_UNKNOWN;
            }
        }

        bool getInfo(JCore::ImageData& img, std::string_view root) const {
            using namespace JCore;

            std::string path = IO::combine(root, this->path);
            switch (format)
            {
            case JCore::FMT_PNG:  return Png::getInfo(path.c_str(), img);
            case JCore::FMT_BMP:  return Bmp::getInfo(path.c_str(), img);
            case JCore::FMT_DDS:  return DDS::getInfo(path.c_str(), img);
            case JCore::FMT_JTEX: return JTEX::getInfo(path.c_str(), img);
            default: return false;
            }
        }
        bool decodeImage(JCore::ImageData& img, std::string_view root) const {
            using namespace JCore;

            std::string path = IO::combine(root, this->path);
            switch (format)
            {
            case JCore::FMT_PNG:  return Png::decode(path.c_str(), img);
            case JCore::FMT_BMP:  return Bmp::decode(path.c_str(), img);
            case JCore::FMT_DDS:  return DDS::decode(path.c_str(), img);
            case JCore::FMT_JTEX: return JTEX::decode(path.c_str(), img);
            default: return false;
            }
        }

        bool isValid() const {
            return index != NullIdx && path.length() > 0;
        }
    };

    static inline bool compare(const PFramePath& lhs, const PFramePath& rhs) {
        return lhs.index > rhs.index;
    }

    struct AudioVariant {
        std::string path{};
        JCore::AudioData audio{};

        bool isValid() const {
            return audio.depth == 16 && audio.sampleRate > 0 && audio.sampleRate <= 48000 && audio.channels > 0 && audio.channels <= 2 && audio.sampleCount > 0;
        }

        void read(const json& jsonF) {
            path = jsonF.is_string() ? jsonF.get<std::string>() : std::string{};
        }

        void write(json& jsonF) const {
            jsonF = path;
        }

        bool check(std::string_view root) {
            std::string path = JCore::IO::combine(root, this->path);
            return JCore::Wav::getInfo(path, audio) && isValid();
        }
    };

    struct PrFrame {
        float frameDuration{};
        PFrameFlags flags{};

        PFramePath path{};
        PFramePath pathE{};
        CRCBlocks block[2]{};

        void reset() {
            frameDuration = 0.0f;
            flags = PFrameFlags::PFrm_None;
            path = PFramePath("", NullIdx);
            pathE = PFramePath("", NullIdx);

            block[0].clear();
            block[1].clear();
        }
    };

    struct PrLayer {
        PLayerFlags flags{};
        std::string name{};
        int32_t stackThreshold{};

        void reset() {
            name = "Default";
            flags = PLa_DefaultState;
            stackThreshold = 1;
        }

        void read(const json& jsonF) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                name = JCore::IO::readString(jsonF, "name", "Default");
                flags = jsonF.value("defaultState", false) ? PLa_DefaultState : PLa_None;
                flags = PLayerFlags(flags | (jsonF.value("isTransparent", false) ? PLa_IsTransparent : PLa_None));
                stackThreshold = Math::clamp(jsonF.value("stackThreshold", 1), 1, 999);
            }
        }

        void write(json& jsonF) const {
            jsonF["name"] = name;
            jsonF["defaultState"] = (flags & PLa_DefaultState) != 0;
            jsonF["isTransparent"] = (flags & PLa_IsTransparent) != 0;
            jsonF["stackThreshold"] = stackThreshold;
        }

        void write(const Stream& stream) const {
            stream.writeValue(flags);
            writeShortString(name, stream);
            stream.writeValue(stackThreshold);
        }
    };

    enum TargetMode : uint8_t {
        TGT_LAST_VALID = 0x00,
        TGT_NEXT_VALID,
        TGT_SPECIFIC_VALID,

        TGT_COUNT
    };

    struct FrameTarget {
        TargetMode mode{};
        int32_t layer{ -1 };
        int32_t frame{ -1 };
        bool emission{ false };

        void reset() {
            mode = TargetMode::TGT_LAST_VALID;
            layer = -1;
            frame = -1;
            emission = false;
        }

        constexpr bool isValid(size_t frames, size_t layers, bool isSrc) const {
            if (!isSrc && mode != TGT_SPECIFIC_VALID && mode < TGT_COUNT) {
                return true;
            }

            if (layer < 0 || frame < 0) { return false; }
            return layer < layers && frame < frames;
        }

        bool read(const json& jsonF) {
            if (!jsonF.is_object()) {
                return false;
            }
            mode = jsonF.value("mode", TGT_LAST_VALID);
            layer = jsonF.value("layer", -1);
            frame = jsonF.value("frame", -1);
            emission = jsonF.value("emission", false);
            return true;
        }

        void write(json& jsonF) const {
            jsonF["mode"] = mode;
            jsonF["layer"] = layer;
            jsonF["frame"] = frame;
            jsonF["emission"] = emission;
        }
    };

    struct FrameInfo {
        float frameDuration{};
        FrameTarget sourceStart{};
        FrameTarget sourceEnd{};
        FrameTarget target{};

        bool read(const json& jsonF) {
            if (!jsonF.is_object()) {
                return false;
            }

            bool valid = true;
            valid &= sourceStart.read(JCore::IO::getObject(jsonF, "sourceStart"));
            valid &= sourceEnd.read(JCore::IO::getObject(jsonF, "sourceEnd"));
            valid &= target.read(JCore::IO::getObject(jsonF, "target"));
            return valid;
        }

        void write(json& jsonF) const {
            sourceStart.write(jsonF["sourceStart"]);
            sourceEnd.write(jsonF["sourceEnd"]);
            target.write(jsonF["target"]);
        }
    };

    struct StackThreshold {
        int32_t stack{};
        int32_t frames{};

        void reset() {
            stack = 0;
            frames = 0;
        }

        void read(const json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                stack = jsonF.value("stack", 0);
                frames = jsonF.value("frames", 0);
            }
        }

        void write(json& jsonF) const {
            jsonF["stack"] = stack;
            jsonF["frames"] = frames;
        }

        void write(const Stream& stream) const {
            stream.writeValue(stack);
            stream.writeValue(frames);
        }
    };

    struct AudioInfo {
        AudioMode type{};
        std::vector<AudioVariant> variants{};

        void reset() {
            type = AudioMode::Aud_SFX;
            variants.clear();
        }

        bool prepare(std::string_view root) {
            bool audioValid = true;
            for (auto& aud : variants) {
                if (!aud.check(root)) {
                    return false;
                }
            }
            return true;
        }

        bool hasValidAudio() const {
            uint32_t sRate = 0;
            size_t sCount = 0;
            uint32_t channels = 0;

            for (size_t i = 0; i < variants.size(); i++) {
                auto& variant = variants[i];
                if (!variant.isValid()) {
                    return false;
                }

                if (i == 0) {
                    sRate = variant.audio.sampleRate;
                    sCount = variant.audio.sampleCount;
                    channels = variant.audio.channels;
                }
                else if (variant.audio.sampleRate != sRate || variant.audio.sampleCount != sCount || variant.audio.channels != channels) {
                    return false;
                }
            }
            return variants.size() > 0;
        }

        void read(const json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                type = jsonF.value("type", AudioMode::Aud_SFX);

                auto& aVars = JCore::IO::getObject(jsonF, "variant");
                if (aVars.is_string()) {
                    variants.resize(1);
                    variants[0].read(aVars);
                }
                else if (aVars.is_array()) {
                    variants.reserve(aVars.size());
                    for (size_t i = 0; i < aVars.size(); i++) {
                        if (aVars[i].is_string()) {
                            variants.emplace_back().read(aVars[i]);
                        }
                    }
                }
            }
        }

        void write(json& jsonF) const {
            jsonF["type"] = type;

            if (variants.size() == 1) {
                variants[0].write(jsonF["variant"]);
            }
            else if (variants.size() > 2) {
                json::array_t aVars = json::array_t{};
                for (auto& aud : variants) {
                    aud.write(aVars.emplace_back());
                }
                jsonF["variant"] = aVars;
            }
        }

        void write(const Stream& stream, std::string_view root, JCore::AudioData& audioBuffer) const {
            using namespace JCore;
            if (hasValidAudio()) {
                auto& var0 = variants[0];
                size_t pos = stream.tell();;

                stream.writeValue(uint8_t(type | (var0.audio.channels > 1 ? Aud_IsStereo : 0x00)));
                stream.writeValue<uint32_t>(var0.audio.sampleRate);
                stream.writeValue<size_t>(var0.audio.sampleCount);
                stream.writeValue<uint16_t>(uint16_t(variants.size()));

                size_t byteSize = var0.audio.sampleCount * (var0.audio.channels > 1 ? 4 : 2);
                std::string aPath{};
                for (auto& var : variants) {
                    aPath = JCore::IO::combine(root, var.path);
                    if (Wav::decode(aPath.c_str(), audioBuffer)) {
                        stream.write(audioBuffer.data, byteSize, false);
                    }
                    else {
                        stream.seek(pos, SEEK_SET);
                        JCORE_ERROR("Failed to decode wave data of audio variant! @ path '{}'", aPath);
                        goto noData;
                    }
                }
                return;
            }
        noData:
            stream.writeValue(type);
            stream.writeValue<uint32_t>(0);
            stream.writeValue<uint32_t>(0);
            stream.writeValue<uint16_t>(0);
        }
    };

    struct PBundleEntry {
        PType type{};
        std::string id{};
        int32_t stack{ 1 };
        PConditions conditions{};
        float weight{};

        void reset() {
            type = PType::PTY_Projection;
            id = "";
            stack = 1;
            conditions.reset();
            weight = 1.0f;
        }

        void read(const json& jsonF) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                type = jsonF.value("type", PType::PTY_Projection);
                id = JCore::IO::readString(jsonF, "id", "");
                stack = Math::max(jsonF.value("stack", 1), 1);
                conditions.read(JCore::IO::getObject(jsonF, "conditions"));
                weight = Math::max(jsonF.value("weight", 1.0f), 0.001f);
            }
        }

        void write(json& jsonF) const {
            jsonF["type"] = type;
            jsonF["id"] = id;
            jsonF["stack"] = stack;
            conditions.write(jsonF["conditions"]);
            jsonF["weight"] = weight;
        }

        void write(const Stream& stream) const {
            stream.writeValue(type);
            writeShortString(id, stream);
            stream.writeValue(stack);
            conditions.write(stream);
            stream.writeValue(weight);
        }
    };

    struct PBundle {
        PMaterial material{};
        int32_t minSize{};
        int32_t maxSize{};
        std::vector<PBundleEntry> entries{};

        void reset() {
            material.reset();

            minSize = 1;
            maxSize = 1;

            entries.resize(1);
            entries[0].reset();
        }

        void read(const json& jsonF, std::string_view root) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                material.read(JCore::IO::getObject(jsonF, "material"), root);
                minSize = Math::max(jsonF.value("minSize", 1), 1);
                maxSize = Math::max(jsonF.value("maxSize", 1), minSize);

                auto& ent = JCore::IO::getObject(jsonF, "entries");
                if (ent.is_array() && ent.size() > 0) {
                    entries.resize(ent.size());
                    for (size_t i = 0; i < ent.size(); i++) {
                        entries[i].read(ent[i]);
                    }
                }
            }
        }

        void write(json& jsonF) const {
            material.write(jsonF["material"]);
            jsonF["minSize"] = minSize;
            jsonF["maxSize"] = maxSize;

            json::array_t arr{};
            for (size_t i = 0; i < entries.size(); i++) {
                entries[i].write(arr.emplace_back());
            }
            jsonF["entries"] = arr;
        }

        void write(const Stream& stream) const {
            material.write(stream);
            stream.writeValue(minSize);
            stream.writeValue(maxSize);
            stream.writeValue(int32_t(entries.size()));

            for (size_t i = 0; i < entries.size(); i++) {
                entries[i].write(stream);
            }
        }
    };

    struct FrameMask {
        bool compress{};
        std::string name{};
        std::string path{};

        void reset() {
            compress = true;
            path = "";
            name = "";
        }

        void checkName() {
            using namespace JCore;
            if (Utils::isWhiteSpace(name) &&
                !Utils::isWhiteSpace(path)) {
                name = IO::getName(path, true);
            }
        }

        void read(const json& jsonF) {
            reset();
            if (jsonF.is_object()) {
                compress = jsonF.value("compress", true);
                path = JCore::IO::readString(jsonF, "path", "");
                name = JCore::IO::readString(jsonF, "name", "");
                checkName();
            }
        }

        void write(json& jsonF) const {
            jsonF["compress"] = compress;
            jsonF["path"] = path;
            jsonF["name"] = name;
        }

        void write(const Stream& stream, std::string_view root, int32_t width, int32_t height) const;
    };

    struct Projection {
        PMaterial material{};

        float frameRate{};
        float loopStart{};

        std::string framePath{};
        int32_t width{};
        int32_t height{};

        AnimationMode animMode{};

        std::vector<PrLayer> layers{};
        std::vector<PrFrame> frames{};

        std::vector<FrameInfo> frameInfo{};
        std::vector<StackThreshold> stackThresholds{};
        std::vector<Tag> tags{};
        std::vector<std::string> rawTags{};
        std::vector<FrameMask> masks{};
        AudioInfo audioInfo;

        bool prepared;

        void reset() {
            material.reset();
            audioInfo.reset();
            frames.clear();
            layers.clear();

            masks.clear();

            frameInfo.clear();


            tags.clear();
            rawTags.clear();
            stackThresholds.clear();

            layers.resize(1);
            layers[0].reset();

            frameRate = 30.0f;
            loopStart = 0.0f;
            animMode = AnimationMode::ANIM_FrameSet;

            framePath = "Frames";
            prepared = false;
        }

        void refreshTags() {
            rawTags.clear();
            for (auto& tag : tags) {
                tag.addTag(rawTags, material.root);
            }
        }

        bool read(const json& jsonF, std::string_view root) {
            using namespace JCore;
            reset();
            if (jsonF.is_object()) {
                material.read(JCore::IO::getObject(jsonF, "material"), root);

                framePath = IO::readString(jsonF, "framePath", "Frames");

                frameRate = Math::clamp(jsonF.value("frameRate", 30.0f), 0.001f, 60.0f);
                loopStart = Math::max(jsonF.value("loopStart", 0.0f), 0.0f);
                animMode = jsonF.value("animMode", ANIM_FrameSet);

                auto& lrs = JCore::IO::getObject(jsonF, "layers");
                if (lrs.is_array() && lrs.size() > 0) {
                    layers.clear();
                    layers.reserve(lrs.size());
                    for (size_t i = 0; i < lrs.size() && layers.size() < PROJ_MAX_LAYERS; i++) {
                        if (lrs[i].is_object()) {
                            layers.emplace_back().read(lrs[i]);
                        }
                    }
                    if (layers.size() < 1) {
                        layers.emplace_back().reset();
                    }
                }

                auto& fInfo = JCore::IO::getObject(jsonF, "frameInfo");
                if (fInfo.is_array() && fInfo.size() > 0) {
                    frameInfo.reserve(fInfo.size());
                    for (size_t i = 0; i < fInfo.size(); i++) {
                        if (fInfo[i].is_object()) {
                            frameInfo.emplace_back().read(fInfo[i]);
                        }
                    }
                }

                auto& fMasks = JCore::IO::getObject(jsonF, "masks");
                if (fMasks.is_array() && fMasks.size() > 0) {
                    masks.reserve(fMasks.size());
                    for (size_t i = 0; i < fMasks.size(); i++) {
                        if (fMasks[i].is_object()) {
                            masks.emplace_back().read(fMasks[i]);
                        }
                    }
                }

                auto& tagS = JCore::IO::getObject(jsonF, "tags");
                if (tagS.is_array() && tagS.size() > 0) {
                    tags.reserve(tagS.size());
                    for (size_t i = 0; i < tagS.size(); i++) {
                        if (tagS[i].is_string()) {
                            tags.emplace_back().read(tagS[i]);
                        }
                    }
                }

                auto& stackT = JCore::IO::getObject(jsonF, "stackThresholds");
                if (stackT.is_array() && stackT.size() > 0) {
                    stackThresholds.reserve(stackT.size());
                    for (size_t i = 0; i < stackT.size(); i++) {
                        if (stackT[i].is_object()) {
                            stackThresholds.emplace_back().read(stackT[i]);
                        }
                    }
                }
                audioInfo.read(JCore::IO::getObject(jsonF, "audio"));

                refreshTags();
                return true;
            }
            return false;
        }
        void write(json& jsonF) const {
            material.write(jsonF["material"]);

            jsonF["framePath"] = framePath;
            jsonF["frameRate"] = frameRate;
            jsonF["loopStart"] = loopStart;
            jsonF["animMode"] = animMode;

            json::array_t lrs = json::array_t{};
            for (auto& layer : layers) {
                layer.write(lrs.emplace_back());
            }
            jsonF["layers"] = lrs;

            json::array_t fInfo = json::array_t{};
            for (auto& fi : frameInfo) {
                fi.write(fInfo.emplace_back());
            }
            jsonF["frameInfo"] = fInfo;

            json::array_t fMasks = json::array_t{};
            for (auto& mask : masks) {
                mask.write(fMasks.emplace_back());
            }
            jsonF["masks"] = fMasks;

            json::array_t tagS = json::array_t{};
            for (auto& tag : tags) {
                tag.write(tagS.emplace_back());
            }
            jsonF["tags"] = tagS;

            json::array_t stackT = json::array_t{};
            for (auto& st : stackThresholds) {
                st.write(stackT.emplace_back());
            }
            jsonF["stackThresholds"] = stackT;
            audioInfo.write(jsonF["audio"]);
        }

        bool prepare();
        bool write(const Stream& stream, PBuffers& buffers, float minCompression = 0.25f);

        void removeTagAt(size_t i) {
            if (i >= tags.size()) { return; }
            tags.erase(tags.begin() + i);
        }

        void duplicateTagAt(size_t i) {
            if (i >= tags.size()) { return; }
            Tag copy = tags[i];
            tags.insert(tags.begin() + i, copy);
        }

        void removeMaskAt(size_t i) {
            if (i >= masks.size()) { return; }
            masks.erase(masks.begin() + i);
        }

        void duplicateMaskAt(size_t i) {
            if (i >= masks.size()) { return; }
            FrameMask copy = masks[i];
            masks.insert(masks.begin() + i, copy);
        }
    };
}

namespace JCore {
    DEFINE_ENUM(Projections::RarityType, false, 0, Projections::RarityType::RARITY_COUNT,
        "Basic",
        "Intermediate",
        "Advanced",
        "Expert",
        "Master");

    DEFINE_ENUM(Projections::TargetMode, false, 0, Projections::TargetMode::TGT_COUNT,
        "Last Valid",
        "Next Valid",
        "Specific Frame");

    DEFINE_ENUM(Projections::PoolType, false, 0, Projections::PoolType::Pool_COUNT,
        "None",
        "Trader",
        "NPC Drop",
        "Fishing Quest",
        "Treasure Bag");

    DEFINE_ENUM(Projections::PType, false, 0, Projections::PType::PTY_Count,
        "Projection",
        "P-Material",
        "P-Bundle");

    DEFINE_ENUM(Projections::RecipeType, false, 0, Projections::RecipeType::__RECIPE_TYPE_COUNT,
        "None",
        "Vanilla Item",
        "Modded Item",
        "Projection",
        "P-Material",
        "P-Bundle");

    DEFINE_ENUM_ID(Projections::BiomeFlags, 0, true, 0, 5,
        "Sky",
        "Surface",
        "Underground",
        "Caverns",
        "Underworld");

    DEFINE_ENUM_ID(Projections::BiomeFlags, 1, true, 5, 14,
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
        "Graveyard");

    DEFINE_ENUM_ID(Projections::BiomeFlags, 2, true, 19, 9,
        "Old Ones Army",
        "Town",
        "Water Candle",
        "Peace Candle",
        "Shadow Candle",
        "Solar Pillar",
        "Nebula Pillar",
        "Vortex Pillar",
        "Stardust Pillar");

    DEFINE_ENUM_ID(Projections::BiomeFlags, 3, true, 28, 4,
        "Purity",
        "Corruption",
        "Crimson",
        "Hallow");

    DEFINE_ENUM_ID(Projections::BiomeFlags, 4, true, 32, 4,
        "Elevation",
        "Biome",
        "Effect",
        "Purity/Evil");

    DEFINE_ENUM_ID(Projections::WorldConditions, 0, true, 0, 15,
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
        "Is Crimson");

    DEFINE_ENUM_ID(Projections::WorldConditions, 1, true, 15, 33,
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
        "Frost Moon");

    DEFINE_ENUM(Projections::AnimationMode, false, 0, Projections::AnimationMode::ANIM_COUNT,
        "Image Set",
        "Loop (Video)",
        "Loop (Audio)");

    DEFINE_ENUM(Projections::TexMode, false, 0, Projections::TexMode::__TEX_COUNT,
        "None",
        "PNG",
        "DDS",
        "JTEX",
        "RLE");

    DEFINE_ENUM_ID(Projections::NPCID, 0, false, Projections::NPCID::NPC_NET_START, Projections::NPCID::NPC_Count,
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
        NULL_NAME,
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
        "Moon Lord's Hand",
        "Moon Lord's Core",
        "Martian Probe",
        "Moon Lord Free Eye",
        "Moon Leech Clot",
        "Milkyway Weaver",
        "Milkyway Weaver Body",
        "Milkyway Weaver Tail",
        "Star Cell",
        "Star Cell Mini",
        "Flow Invader",
        NULL_NAME,
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
        "Mystic Frog");

    DEFINE_ENUM_ID(Projections::NPCID, 1, false, Projections::NPCID::NPC_MAIN_START, Projections::NPCID::NPC_MAIN_COUNT,
        "Any",
        "Blue Slime",
        "Demon Eye",
        "Zombie",
        "Eye of Cthulhu",
        "Servant of Cthulhu",
        "Eater of Souls",
        "Devourer",
        NULL_NAME,
        NULL_NAME,
        "Giant Worm",
        NULL_NAME,
        NULL_NAME,
        "Eater of Worlds",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        "Old Man",
        "Demolitionist",
        "Bone Serpent",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        "Giant Bat",
        "Corruptor",
        "Digger",
        NULL_NAME,
        NULL_NAME,
        "World Feeder",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        "Zombie Bald",
        "Wandering Eye",
        "The Destroyer",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        "Moon Lord",
        NULL_NAME,
        NULL_NAME,
        "Martian Probe",
        NULL_NAME,
        NULL_NAME,
        "Milkyway Weaver",
        NULL_NAME,
        NULL_NAME,
        "Star Cell",
        "Star Cell Mini",
        "Flow Invader",
        NULL_NAME,
        "Twinkle Popper",
        "Twinkle",
        "Stargazer",
        "Crawltipede",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
        "Tomb Crawler",
        NULL_NAME,
        NULL_NAME,
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
        NULL_NAME,
        NULL_NAME,
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
        "Mystic Frog");
}