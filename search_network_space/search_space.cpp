#include <iostream>
#include <emmintrin.h>
#include <immintrin.h>
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
    cmd_end,

    cmd_start_avx2,
    avx2_unpack8_next = cmd_start_avx2,
    avx2_unpack8_skip,
    avx2_unpack16_next,
    avx2_unpack16_skip,
    avx2_unpack32_next,
    avx2_unpack32_skip,
    avx2_unpack64_next,
    avx2_unpack64_skip,
    avx2_unpack128_next,
    avx2_unpack128_skip,
    avx2_permute64_self,
    avx2_shuffle32_self,

    cmd_end_avx2
};

template<size_t SIZE, typename VTYPE>
struct State {
    VTYPE v[SIZE];
    std::vector<Command> cmds;
};

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

void do_avx2(__m256i a, __m256i b, Command cmd, __m256i &low, __m256i &high) {
    switch(cmd) {
        case avx2_unpack8_next:
        case avx2_unpack8_skip:
            low = _mm256_unpacklo_epi8(a, b);
            high = _mm256_unpackhi_epi8(a, b);
            break;
        case avx2_unpack16_next:
        case avx2_unpack16_skip:
            low = _mm256_unpacklo_epi16(a, b);
            high = _mm256_unpackhi_epi16(a, b);
            break;
        case avx2_unpack32_next:
        case avx2_unpack32_skip:
            low = _mm256_unpacklo_epi32(a, b);
            high = _mm256_unpackhi_epi32(a, b);
            break;
        case avx2_unpack64_next:
        case avx2_unpack64_skip:
            low = _mm256_unpacklo_epi64(a, b);
            high = _mm256_unpackhi_epi64(a, b);
            break;
        case avx2_unpack128_next:
        case avx2_unpack128_skip:
            low = _mm256_permute2x128_si256(a, b, 2 << 4);
            high = _mm256_permute2x128_si256(a, b, 1 | (3 << 4));
            break;
        default:
            assert(!"Unknown");
            break;
    }
}

template<size_t SIZE, typename VTYPE>
State<SIZE, VTYPE> apply_command(const State<SIZE, VTYPE> &state, Command cmd);

template<size_t SIZE>
State<SIZE, __m128i> apply_command(const State<SIZE, __m128i> &state, Command cmd) {
    State<SIZE, __m128i> new_state;
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
State<SIZE, __m256i> apply_command(const State<SIZE, __m256i> &state, Command cmd) {
    State<SIZE, __m256i> new_state;
    new_state.cmds = state.cmds;
    new_state.cmds.push_back(cmd);
    switch (cmd) {
        case avx2_unpack8_next:
        case avx2_unpack16_next:
        case avx2_unpack32_next:
        case avx2_unpack64_next:
        case avx2_unpack128_next:
            for (size_t i = 0; i < SIZE / 2; i += 1) {
                do_avx2(state.v[i * 2], state.v[i * 2 + 1], cmd, new_state.v[i * 2], new_state.v[i * 2 + 1]);
            }
            break;
        case avx2_unpack8_skip:
        case avx2_unpack16_skip:
        case avx2_unpack32_skip:
        case avx2_unpack64_skip:
        case avx2_unpack128_skip:
            for (size_t i = 0; i < SIZE / 2; i += 1) {
                do_avx2(state.v[i], state.v[i + SIZE / 2], cmd, new_state.v[i * 2], new_state.v[i * 2 + 1]);
            }
            break;
        case avx2_permute64_self:
            for (size_t i = 0; i < SIZE; i += 1) {
                new_state.v[i] = _mm256_permute4x64_epi64(state.v[i], (3 << 6) | (1 << 4) | (2 << 2) | (0 << 0));
            }
            break;
        case avx2_shuffle32_self:
            for (size_t i = 0; i < SIZE; i += 1) {
                new_state.v[i] = _mm256_shuffle_epi32(state.v[i], (3 << 6) | (1 << 4) | (2 << 2) | (0 << 0));
            }
            break;
        default:
            assert(!"Unknown");
            break;
    }
    return new_state;
}

template<size_t SIZE, typename VTYPE>
void print_network(const State<SIZE, VTYPE> &state) {
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
            case avx2_unpack8_next:
                name = "avx2_unpack8_next";
                break;
            case avx2_unpack8_skip:
                name = "avx2_unpack8_skip";
                break;
            case avx2_unpack16_skip:
                name = "avx2_unpack16_skip";
                break;
            case avx2_unpack16_next:
                name = "avx2_unpack16_next";
                break;
            case avx2_unpack32_skip:
                name = "avx2_unpack32_skip";
                break;
            case avx2_unpack32_next:
                name = "avx2_unpack32_next";
                break;
            case avx2_unpack64_skip:
                name = "avx2_unpack64_skip";
                break;
            case avx2_unpack64_next:
                name = "avx2_unpack64_next";
                break;
            case avx2_unpack128_skip:
                name = "avx2_unpack128_skip";
                break;
            case avx2_unpack128_next:
                name = "avx2_unpack128_next";
                break;
            case avx2_permute64_self:
                name = "avx2_permute64_self";
                break;
            case avx2_shuffle32_self:
                name = "avx2_shufle32_self";
                break;
            default:
                assert(!"Unknown");
        }
        std::cout << name << std::endl;
    }
}

template<size_t SIZE, typename VTYPE>
bool states_are_equal(const State<SIZE, VTYPE>& a, const State<SIZE, VTYPE>& b) {
    return memcmp(&a.v[0], &b.v[0], SIZE * sizeof(VTYPE)) == 0;
}

template<size_t SIZE, typename VTYPE>
void traverse(const State<SIZE, VTYPE> &state, State<SIZE, VTYPE> &expected_state, std::vector<State<SIZE, VTYPE>> &best_networks) {
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
    Command start, end;
    if (std::is_same<VTYPE, __m128i>::value) {
        start = cmd_start;
        end = cmd_end;
    } else if (std::is_same<VTYPE, __m256i>::value) {
        start = cmd_start_avx2;
        end = cmd_end_avx2;
    } else {
        assert(!"Unknown input type");
    }
    for (int i = start; i < end; ++i) {
        State<SIZE, VTYPE> new_state = apply_command(state, (Command)i);
        traverse(new_state, expected_state, best_networks);
    }
}

int main() {
    {
        // Search for the best networks for float types.
        State<4, __m128i> initial_state;
        uint8_t *raw = (uint8_t*)&initial_state.v[0];
        for (size_t i = 0; i < 64; ++i) {
            raw[i] = (uint8_t)i;
        }
        State<4, __m128i> expected_state;
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
            std::vector<State<4, __m128i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Float decode  networks" << std::endl;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            std::vector<State<4, __m128i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
    }

    {
        // Search for the best networks for double types.
        State<8, __m128i> initial_state;
        State<8, __m128i> expected_state;
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
            std::vector<State<8, __m128i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Double decode networks" << std::endl;
            std::vector<State<8, __m128i>> best_networks;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }

    }

    {
        // Search for the best networks for float types.
        State<4, __m256i> initial_state;
        uint8_t *raw = (uint8_t*)&initial_state.v[0];
        for (size_t i = 0; i < 128; ++i) {
            raw[i] = (uint8_t)i;
        }
        State<4, __m256i> expected_state;
        for (size_t i = 0; i < 7; ++i) {
            expected_state.cmds.push_back(cmd_end);
        }
        raw = (uint8_t*)&expected_state.v[0];
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 32; ++j) {
                raw[j + i * 32] = j * 4 + i;
            }
        }

        {
            std::cout << "Float AVX2 encode networks" << std::endl;
            std::vector<State<4, __m256i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Float AVX2 decode networks" << std::endl;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            std::vector<State<4, __m256i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<4>(best_networks[i]);
                std::cout << std::endl;
            }
        }
    }

    {
        // Search for the best networks for double types.
        State<8, __m256i> initial_state;
        uint8_t *raw = (uint8_t*)&initial_state.v[0];
        for (size_t i = 0; i < 256; ++i) {
            raw[i] = (uint8_t)i;
        }
        State<8, __m256i> expected_state;
        for (size_t i = 0; i < 9; ++i) {
            expected_state.cmds.push_back(cmd_end);
        }
        raw = (uint8_t*)&expected_state.v[0];
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 32; ++j) {
                raw[j + i * 32] = j * 8 + i;
            }
        }

        {
            std::cout << "Double AVX2 encode networks" << std::endl;
            std::vector<State<8, __m256i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }
        {
            std::cout << "Double AVX2 decode networks" << std::endl;
            std::swap(initial_state.v, expected_state.v); // Quick hack.
            std::vector<State<8, __m256i>> best_networks;
            traverse(initial_state, expected_state, best_networks);
            for (size_t i = 0; i < best_networks.size(); ++i) {
                print_network<8>(best_networks[i]);
                std::cout << std::endl;
            }
        }
    }
    return 0;
}
