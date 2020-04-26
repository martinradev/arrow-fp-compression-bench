#include <iostream>
#include <emmintrin.h>
#include <set>
#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <algorithm>

enum Command {
    cmd_start = 0,
    unpack8_next = cmd_start,
    unpack8_skip,
    unpack16_next,
    unpack16_skip,
    unpack32_next,
    unpack32_skip,
    unpack64_next,
    unpack64_skip,

    cmd_end
};

template<size_t SIZE>
struct State {
    __m128i v[SIZE];
    std::vector<Command> cmds;
};

template<size_t SIZE>
bool operator<(const State<SIZE> &lft, const State<SIZE> &rht) {
        uint8_t lanes[SIZE * sizeof(lft.v[0])];
        uint8_t lanes_rht[sizeof(lanes)];
        memcpy(&lanes[0], &lft.v[0], sizeof(lft.v));
        memcpy(&lanes_rht[0], &rht.v[0], sizeof(rht.v));
        for (size_t i = 0; i < sizeof(lanes); ++i) {
            if (lanes[i] != lanes_rht[i]) {
                return lanes[i] < lanes_rht[i];
            }
        }
        return true;
    }

void do_simd(__m128i a, __m128i b, Command cmd, __m128i &low, __m128i &high) {
    switch(cmd) {
        case unpack8_next:
        case unpack8_skip:
            low = _mm_unpacklo_epi8(a, b);
            high = _mm_unpackhi_epi8(a, b);
            break;
        case unpack16_next:
        case unpack16_skip:
            low = _mm_unpacklo_epi16(a, b);
            high = _mm_unpackhi_epi16(a, b);
            break;
        case unpack32_next:
        case unpack32_skip:
            low = _mm_unpacklo_epi32(a, b);
            high = _mm_unpackhi_epi32(a, b);
            break;
        case unpack64_next:
        case unpack64_skip:
            low = _mm_unpacklo_epi64(a, b);
            high = _mm_unpackhi_epi64(a, b);
            break;
        default:
            assert(!"Unknown");
            break;
    }
}

template<size_t SIZE>
State<SIZE> apply_command(const State<SIZE> &state, Command cmd) {
    State<SIZE> new_state;
    new_state.cmds = state.cmds;
    new_state.cmds.push_back(cmd);
    switch (cmd) {
        case unpack8_next:
        case unpack16_next:
        case unpack32_next:
        case unpack64_next:
            for (size_t i = 0; i < SIZE / 2; i += 1) {
                do_simd(state.v[i * 2], state.v[i * 2 + 1], cmd, new_state.v[i * 2], new_state.v[i * 2 + 1]);
            }
            break;
        case unpack8_skip:
        case unpack16_skip:
        case unpack32_skip:
        case unpack64_skip:
            for (size_t i = 0; i < SIZE / 2; i += 1) {
                do_simd(state.v[i], state.v[i + SIZE / 2], cmd, new_state.v[i * 2], new_state.v[i * 2 + 1]);
            }
            break;
        default:
            assert(!"Unknown");
            break;
    }
    return new_state;
}

template<size_t SIZE>
void print_network(const State<SIZE> &state) {
    std::cout << "Num cmds: " << state.cmds.size() << std::endl;
    for (size_t i = 0; i < state.cmds.size(); ++i) {
        Command cmd = state.cmds[i];
        std::string name;
        switch(cmd) {
            case unpack8_next:
                name = "unpack8_next";
                break;
            case unpack8_skip:
                name = "unpack8_skip";
                break;
            case unpack16_skip:
                name = "unpack16_skip";
                break;
            case unpack16_next:
                name = "unpack16_next";
                break;
            case unpack32_skip:
                name = "unpack32_skip";
                break;
            case unpack32_next:
                name = "unpack32_next";
                break;
            case unpack64_skip:
                name = "unpack64_skip";
                break;
            case unpack64_next:
                name = "unpack64_next";
                break;
            default:
                assert(!"Unknown");
        }
        std::cout << name << std::endl;
    }
}

template<size_t SIZE>
bool states_are_equal(const State<SIZE>& a, const State<SIZE>& b) {
    return memcmp(&a.v[0], &b.v[0], 64) == 0;
}

template<size_t SIZE>
void traverse(const State<SIZE> &state, State<SIZE> &expected_state, std::vector<State<SIZE>> &best_networks) {
    if (state.cmds.size() > expected_state.cmds.size()) {
        return;
    }
    if (states_are_equal(state, expected_state) && state.cmds.size() <= expected_state.cmds.size()) {
        if (state.cmds.size() == expected_state.cmds.size()) {
            // NOP
        } else {
            expected_state = state;
            best_networks.clear();
        }
        best_networks.push_back(state);
        return;
    }
    for (int i = cmd_start; i < cmd_end; ++i) {
        State<SIZE> new_state = apply_command(state, (Command)i);
        traverse(new_state, expected_state, best_networks);
    }
}

int main() {
    {
        // Search for the best networks for float types.
        State<4> initial_state;
        uint8_t *raw = (uint8_t*)&initial_state.v[0];
        for (size_t i = 0; i < 64; ++i) {
            raw[i] = (uint8_t)i;
        }
        State<4> expected_state;
        for (size_t i = 0; i < 6; ++i) {
            expected_state.cmds.push_back(cmd_end);
        }
        raw = (uint8_t*)&expected_state.v[0];
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 16; ++j) {
                raw[j + i * 16] = j * 4 + i;
            }
        }

        {
            std::cout << "Float encode networks" << std::endl;
            std::vector<State<4>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Float decode  networks" << std::endl;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            std::vector<State<4>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
    }

    {
        // Search for the best networks for float types.
        State<8> initial_state;
        State<8> expected_state;
        uint8_t *raw = (uint8_t*)&initial_state.v[0];
        for (size_t i = 0; i < 128; ++i) {
            raw[i] = (uint8_t)i;
        }
        for (size_t i = 0; i < 6; ++i) {
            expected_state.cmds.push_back(cmd_end);
        }
        raw = (uint8_t*)&expected_state.v[0];
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 16; ++j) {
                raw[j + i * 16] = j * 8 + i;
            }
        }
        {
            std::cout << "Double encode networks" << std::endl;
            std::vector<State<8>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Double decode networks" << std::endl;
            std::vector<State<8>> best_networks;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }

    }

    return 0;
}
