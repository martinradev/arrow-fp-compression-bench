#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <tmmintrin.h>
#include <smmintrin.h>

void print_simd(__m128i value) {
    uint8_t *data = (uint8_t*)&value;
    for (size_t i = 0; i < 16; ++i) {
        printf("%x ", data[i]);
    }
    printf("\n");
}

void encode(const uint8_t *input_data, size_t num_elements, uint8_t *output_data)
{
    const size_t numBytesPerStream = num_elements;
    __m128i *output_data_simd = (__m128i*)output_data;
    size_t size = num_elements * sizeof(float);

    const __m128i shuffle1_0 = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 12, 8, 4, 0);
    const __m128i shuffle1_1 = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 12, 8, 4, 0, 0x80, 0x80, 0x80, 0x80);
    const __m128i shuffle1_2 = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 12, 8, 4, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
    const __m128i shuffle1_3 = _mm_set_epi8(12, 8, 4, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);

    const __m128i delta0 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 1, 1, 1, 1);
    const __m128i delta1 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0, 0, 0, 0);
    const __m128i delta2 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0);
    const __m128i delta3 = _mm_set_epi8(0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0);

    const __m128i* input_data_simd = (const __m128i*)input_data;
    for (size_t i = 0; i < size; i += 64) {
        size_t idx16b = i / 16UL;
        printf("%zu\n", idx16b);
        __m128i input0 = _mm_loadu_si128(&input_data_simd[idx16b]);
        __m128i input1 = _mm_loadu_si128(&input_data_simd[idx16b+1]);
        __m128i input2 = _mm_loadu_si128(&input_data_simd[idx16b+2]);
        __m128i input3 = _mm_loadu_si128(&input_data_simd[idx16b+3]);

        __m128i input0_1 = _mm_shuffle_epi8(input0, shuffle1_0);
        __m128i input0_2 = _mm_shuffle_epi8(input1, shuffle1_1);
        __m128i input0_3 = _mm_shuffle_epi8(input2, shuffle1_2);
        __m128i input0_4 = _mm_shuffle_epi8(input3, shuffle1_3);
        __m128i output0 = input0_1;
        output0 = _mm_add_epi8(output0, input0_2);
        output0 = _mm_add_epi8(output0, input0_3);
        output0 = _mm_add_epi8(output0, input0_4);

        const __m128i shuffle2_0 = _mm_add_epi8(shuffle1_0, delta0);
        const __m128i shuffle2_1 = _mm_add_epi8(shuffle1_1, delta1);
        const __m128i shuffle2_2 = _mm_add_epi8(shuffle1_2, delta2);
        const __m128i shuffle2_3 = _mm_add_epi8(shuffle1_3, delta3);
        __m128i input1_1 = _mm_shuffle_epi8(input0, shuffle2_0);
        __m128i input1_2 = _mm_shuffle_epi8(input1, shuffle2_1);
        __m128i input1_3 = _mm_shuffle_epi8(input2, shuffle2_2);
        __m128i input1_4 = _mm_shuffle_epi8(input3, shuffle2_3);
        __m128i output1 = input1_1;
        output1 = _mm_add_epi8(output1, input1_2);
        output1 = _mm_add_epi8(output1, input1_3);
        output1 = _mm_add_epi8(output1, input1_4);

        const __m128i shuffle3_0 = _mm_add_epi8(shuffle2_0, delta0);
        const __m128i shuffle3_1 = _mm_add_epi8(shuffle2_1, delta1);
        const __m128i shuffle3_2 = _mm_add_epi8(shuffle2_2, delta2);
        const __m128i shuffle3_3 = _mm_add_epi8(shuffle2_3, delta3);
        __m128i input2_1 = _mm_shuffle_epi8(input0, shuffle3_0);
        __m128i input2_2 = _mm_shuffle_epi8(input1, shuffle3_1);
        __m128i input2_3 = _mm_shuffle_epi8(input2, shuffle3_2);
        __m128i input2_4 = _mm_shuffle_epi8(input3, shuffle3_3);
        __m128i output2 = input2_1;
        output2 = _mm_add_epi8(output2, input2_2);
        output2 = _mm_add_epi8(output2, input2_3);
        output2 = _mm_add_epi8(output2, input2_4);

        const __m128i shuffle4_0 = _mm_add_epi8(shuffle3_0, delta0);
        const __m128i shuffle4_1 = _mm_add_epi8(shuffle3_1, delta1);
        const __m128i shuffle4_2 = _mm_add_epi8(shuffle3_2, delta2);
        const __m128i shuffle4_3 = _mm_add_epi8(shuffle3_3, delta3);
        __m128i input3_1 = _mm_shuffle_epi8(input0, shuffle4_0);
        __m128i input3_2 = _mm_shuffle_epi8(input1, shuffle4_1);
        __m128i input3_3 = _mm_shuffle_epi8(input2, shuffle4_2);
        __m128i input3_4 = _mm_shuffle_epi8(input3, shuffle4_3);
        __m128i output3 = input3_1;
        output3 = _mm_add_epi8(output3, input3_2);
        output3 = _mm_add_epi8(output3, input3_3);
        output3 = _mm_add_epi8(output3, input3_4);

        _mm_storeu_si128(output_data_simd + idx16b/4UL, output0);
        _mm_storeu_si128(output_data_simd + (numBytesPerStream/16UL) + idx16b/4UL, output1);
        _mm_storeu_si128(output_data_simd + (numBytesPerStream/16UL)*2UL + idx16b/4UL, output2);
        _mm_storeu_si128(output_data_simd + (numBytesPerStream/16UL)*3UL + idx16b/4UL, output3);
    }
}

int main() {
    const size_t buf_size = 256U;
    uint8_t *buf = malloc(buf_size);
    uint8_t *output_buf = malloc(buf_size);
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (uint8_t)i;
    }
    encode(buf, buf_size/4U, output_buf);
    for (size_t i = 0; i < buf_size; ++i) {
        printf("%x ", output_buf[i]);
    }
    return 0;
}
