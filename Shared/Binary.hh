#pragma once

#include <Shared/EntityDef.hh>

#include <cstdint>
#include <string>
#include <vector>

// kAuthResponse: uint8 success, string session_key_or_error
// kInventoryUpdate: uint32 n, then n × (uint8 type, uint8 rarity, uint64 count)
// kKillsUpdate: uint32 n, then n × (uint8 mob, uint8 rarity, uint64 count)
// kChatMessage: string sender, string message
// kSystemMessage: uint8 kind (SystemMsgKind), string message -- shown in chat as
//   plain coloured text (no sender label)
// kCraftResult: uint8 type, uint8 out_rarity, uint32 crafted, uint32 remaining,
//   uint8 any_success -- remaining is how many of the SELECTED amount are left
//   uncommitted (the rest of the stack, if any, is untouched); crafted can be
//   >1 if a large selection rolled multiple successful rounds.
// kSquadUpdate: uint32 squad_id (0 = none), uint32 n, then n × (EntityID
//   camera_id, string name) -- full roster, pushed whenever membership changes.
// kSquadNotice: string message -- shown as a standalone banner (like the
//   disconnect-with-code one), not routed through the chat log.
enum Clientbound {
    kClientUpdate,
    kAuthResponse,
    kInventoryUpdate,
    kKillsUpdate,
    kChatMessage,
    kSystemMessage,
    kCraftResult,
    kSquadUpdate,
    kSquadNotice
};

// Colour band for a system chat line.
enum SystemMsgKind {
    kSysSuper,    // super-green (super petal crafted / super mob killed)
    kSysUnique,   // unique-grey (unique mob killed)
    kSysSquad     // squad-pink (squad command feedback; matches minimap squad dot)
};

// kRegister / kLogin: string username, string password
// kSessionRestore: string username, string session_key
// kPetalStore: uint8 loadout_static_pos
// kEquipPetal: uint32 inventory_index, uint8 loadout_static_pos
// kInventorySwap: uint32 inventory_index, uint8 loadout_static_pos (swap one equipped ↔ one from stack)
// kPetalSwap: loadout↔loadout (existing)
// kChat: string message -- "/squad-invite <username>" and "/squad-leave" are
//   intercepted server-side (Client.cc) and never broadcast as chat text.
// kCraft: uint8 type, uint8 rarity, uint32 amount -- craft `amount` (>=5) of
//   the (type,rarity) inventory stack into (type,rarity+1).
enum Serverbound {
    kVerify,
    kClientInput,
    kClientSpawn,
    kPetalSwap,
    kPetalStore,
    kEquipPetal,
    kInventorySwap,
    kRegister,
    kLogin,
    kSessionRestore,
    kLogout,
    kChat,
    kCraft,
    kLeave      // voluntarily end the run: despawn the flower so re-entering spawns fresh
};

enum CloseReason {
    kServer = 4001,
    kProtocol = 4002,
    kOutdated = 4003
};

class Writer {
public:
    uint8_t *at;
    uint8_t *packet;
    template<typename T>
    class Encoder {
        friend class Writer;
        static void write(Writer &, T const &);
    };

    template<typename T>
    class Encoder<std::vector<T>> {
        friend class Writer;
        static void write(Writer &w, std::vector<T> const &v) {
            w.write<uint32_t>(v.size());
            for (T const &x : v) w.write<T>(x);
        };
    };

    Writer(uint8_t *);
    template<typename T>
    void write(T const &v) {
        Encoder<T>::write(*this, v);
    };
    void push(uint8_t);
};

class Reader {
public:
    template<typename T>
    class Decoder {
        friend class Reader;
        static T read(Reader &);
        static void read(Reader &, T &);
    };

    template<typename T>
    class Decoder<std::vector<T>> {
        friend class Reader;
        static std::vector<T> read(Reader &r) {
            uint32_t len = r.read<uint32_t>();
            std::vector<T> ret(len);
            for (uint32_t i = 0; i < len; ++i)
                ret.push_back(r.read<T>());
            return ret;
        }

        static void read(Reader &r, std::vector<T> &v) {
            uint32_t len = r.read<uint32_t>();
            v.clear();
            v.reserve(len);
            for (uint32_t i = 0; i < len; ++i)
                v.push_back(r.read<T>());
        }
    };

    uint8_t const *packet;
    uint8_t const *at;
    Reader(uint8_t const *);

    template<typename T>
    T read() {
        return Decoder<T>::read(*this);
    }

    template<typename T>
    void read(T &v) {
        Decoder<T>::read(*this, v);
    }

    uint8_t next();
};

class Validator {
public:
    uint8_t const *at;
    uint8_t const *end;
    Validator(uint8_t const *, uint8_t const *);

    uint8_t validate_uint8();
    uint8_t validate_uint32();
    uint8_t validate_uint64();
    uint8_t validate_float();
    uint8_t validate_string(uint32_t);
};
