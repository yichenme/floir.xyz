#pragma once

#include <Shared/EntityDef.hh>

#include <Helpers/Bits.hh>
#include <Helpers/Macros.hh>
#include <Helpers/Vector.hh>

#include <cstdint>
#include <unordered_map>

SERVER_ONLY(class Writer;)
CLIENT_ONLY(class Reader;)

SERVER_ONLY(typedef uint8_t StickyFlag;)
CLIENT_ONLY(typedef PersistentFlag StickyFlag;)

SERVER_ONLY(typedef float Float;)
CLIENT_ONLY(typedef LerpFloat Float;)

enum Components {
    #define COMPONENT(name) k##name,
    PERCOMPONENT
    #undef COMPONENT
    kComponentCount
};

class Entity {
    enum Fields {
        #define SINGLE(component, name, type) k##name,
        #define MULTIPLE(component, name, type, amt) k##name,
        PERFIELD
        #undef SINGLE
        #undef MULTIPLE
        kFieldCount
    };
    uint32_t components;
#define SINGLE(component, name, type) type name;
#define MULTIPLE(component, name, type, amt) type name[amt];
    PERFIELD
#undef SINGLE
#undef MULTIPLE
    uint8_t state[div_round_up(kFieldCount, 8)];
#define SINGLE(component, name, type);
#define MULTIPLE(component, name, type, amt) uint8_t state_per_##name[div_round_up(amt, 8)];
    PERFIELD
#undef SINGLE
#undef MULTIPLE
public:
    Entity();
    void init();
    void reset_protocol();
    Entity(Entity const &) = delete;
    Entity &operator=(Entity const &) = delete;
    Entity &operator=(Entity &&) = delete;
    uint32_t lifetime;
    EntityID id;
    uint8_t pending_delete;
    void add_component(uint32_t);
    uint8_t has_component(uint32_t) const;

#define SINGLE(component, name, type) type const &get_##name() const;
#define MULTIPLE(component, name, type, amt) type const &get_##name(uint32_t) const;
    PERFIELD
#undef SINGLE
#undef MULTIPLE

#define SINGLE(name, type, reset) type name;
#define MULTIPLE(name, type, amt, reset) type name[amt];
    PER_EXTRA_FIELD
#undef SINGLE
#undef MULTIPLE

#ifdef SERVERSIDE
    // Per-player (keyed by camera id) cumulative damage dealt to this mob, used
    // to pick eligible looters for the individual-loot system. Cleared on spawn.
    std::unordered_map<uint16_t, float> mob_damage;
    void write(Writer *, uint8_t);

    template<bool>
    void write(Writer *);
#define SINGLE(component, name, type) void set_##name(type const &);
#define MULTIPLE(component, name, type, amt) void set_##name(uint32_t, type const &);
    PERFIELD
#undef SINGLE
#undef MULTIPLE
#else
    void tick_lerp(float);
    void read(Reader *, uint8_t);

    template<bool>
    void read(Reader *);

    #define SINGLE(component, name, type) uint8_t get_state_##name() const;
    #define MULTIPLE(component, name, type, amt) uint8_t get_state_##name(uint32_t) const;
    PERFIELD
    #undef SINGLE
    #undef MULTIPLE
#endif
};