#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#include <time.h>
#include <utility>

template<typename T>
struct TypeConverter;

template<>
struct TypeConverter<float> {
    using UnsignedType = uint32_t;
};

template<>
struct TypeConverter<double> {
    using UnsignedType = uint64_t;
};

static inline double gettime(void) {
    struct timespec ts = {0};
    int err = clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)err;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void print_simd_reg(__m128i value) {
    uint8_t *data = (uint8_t*)&value;
    for (size_t i = 0; i < 16; ++i) {
        printf("%x ", data[i]);
    }
    printf("\n");
}

#define print_simd(v) \
do { \
    printf("%s: ", #v); \
    print_simd_reg(v); \
} while(0)

static void flush_buf(const uint8_t *data, uint64_t num_bytes) {
    for (uint64_t i = 0; i < num_bytes; i += 64) {
        _mm_clflush(&data[i]);
    }
}

/********* BEGIN SIMD ENCODERS ***************/
template<typename T>
void encode_fast(const T *input_data, size_t num_elements, uint8_t *output_data) {
    size_t size = num_elements * sizeof(T);
    const __m128i* input_data_simd = (const __m128i*)input_data;
    const uint8_t *input_data_u8 = (const uint8_t*)input_data;

    const __m128i mask_16_bits = _mm_set_epi32(0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU);
    const __m128i mask_8_bits = _mm_set_epi16(0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU);

    const size_t block_size = sizeof(__m128i) * sizeof(T);
    const size_t num_blocks = size / block_size;

    // Handle suffix first to catch potential out-of-bounds overwrites in the simd implementation.
    const size_t offset_element = (num_blocks * block_size) / sizeof(T);
    for (size_t i = offset_element; i < num_elements; ++i) { 
        for (size_t j = 0; j < sizeof(T); ++j) {
            output_data[j * num_elements + i] = input_data_u8[j + i * sizeof(T)];
        }
    }

    for (size_t k = 0; k < num_blocks; ++k) {
        size_t idx16b = k * sizeof(T);
        __m128i v[sizeof(T)];
        __m128i source[4U];

        if (std::is_same<float, T>::value) {
            // Handle single-precision data.
            for (size_t i = 0; i < sizeof(T); ++i) {
                source[i] = _mm_loadu_si128(&input_data_simd[idx16b+i]);
            }
        } else {
            // Handle double-precision data.
            for (size_t i = 0; i < sizeof(T); ++i) {
                v[i] = _mm_loadu_si128(&input_data_simd[idx16b+i]);
            }
        }

        for (size_t it = 0; it < sizeof(T) / sizeof(float); ++it) {
            if (std::is_same<double, T>::value) {
                for (size_t j = 0; j < 4; ++j) {
                    __m128i first, second;
                    if (it == 0) {
                        const uint8_t push_mask = _MM_SHUFFLE(3,1,2,0);
                        first = _mm_shuffle_epi32(v[j*2], push_mask);
                        second = _mm_shuffle_epi32(v[j*2+1], push_mask);
                    } else {
                        const uint8_t push_mask = _MM_SHUFFLE(2,0,3,1);
                        first = _mm_shuffle_epi32(v[j*2], push_mask);
                        second = _mm_shuffle_epi32(v[j*2+1], push_mask);
                    }
                    source[j] = _mm_unpacklo_epi64(first, second);
                }
            }
            
            // Handle first 2 bytes
            __m128i packed_blocks[4];
            for (size_t j = 0; j < 2; ++j) {
                __m128i v_low_16[4];
                for (size_t i = 0; i < 4; ++i) {
                    v_low_16[i]  = _mm_and_si128(source[i], mask_16_bits);
                }
                __m128i v_low_16_packed_low8[2];
                __m128i v_low_16_packed_high8[2];
                for (size_t i = 0; i < 2; ++i) {
                    __m128i v_low_16_packed = _mm_packus_epi32(v_low_16[i*2], v_low_16[i*2+1]);
                    v_low_16_packed_low8[i] = _mm_and_si128(v_low_16_packed, mask_8_bits);

                    v_low_16_packed = _mm_srli_epi16(v_low_16_packed, 8U);
                    v_low_16_packed_high8[i] = _mm_and_si128(v_low_16_packed, mask_8_bits);
                }
                packed_blocks[j*2] = _mm_packus_epi16(v_low_16_packed_low8[0], v_low_16_packed_low8[1]);
                packed_blocks[j*2+1] = _mm_packus_epi16(v_low_16_packed_high8[0], v_low_16_packed_high8[1]);

                for (size_t i = 0; i < 4; ++i) {
                    source[i] = _mm_srli_epi32(source[i], 16U);
                }
            }

            for (size_t j = 0; j < 4; ++j) {
                uint8_t *out_addr = output_data + num_elements * (j + it * 4) + idx16b*(16U/sizeof(T));
                _mm_storeu_si128((__m128i*)out_addr, packed_blocks[j]);
            }
        }
    }
}

// This implementation requires that input size is divisble by 64.
// It also only supports single-precision input.
// I didn't bother doing the changes since the unpack version is slightly faster.
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

/********* END SIMD ENCODERS ***************/

/********* BEGIN SIMD DECODERS ***************/
void decode_fast_float(const uint8_t *input_data, size_t num_elements, uint8_t *output_data)
{
    size_t size = num_elements * sizeof(float);
    const size_t block_size = sizeof(__m128i) * 4U;
    const size_t num_blocks = size / block_size;

    // Handle suffix first.
    const size_t num_processed_elements = (num_blocks * block_size) / sizeof(float);
    for (size_t i = num_processed_elements; i < num_elements; ++i) {
        for (size_t j = 0; j < sizeof(float); ++j) {
            const uint8_t value = input_data[num_elements * j + i];
            output_data[i * sizeof(float) + j] = value;
        }
    }

    for (size_t i = 0; i < num_blocks; ++i) {
        __m128i v[4];
        for (size_t j = 0; j < 4; ++j) {
            v[j] = _mm_loadu_si128((const __m128i*)&input_data[i * 16 + j * num_elements]);
        }
        __m128i comb[4];
        comb[0] = _mm_unpacklo_epi8(v[0], v[2]);
        comb[1] = _mm_unpacklo_epi8(v[1], v[3]);
        comb[2] = _mm_unpackhi_epi8(v[0], v[2]);
        comb[3] = _mm_unpackhi_epi8(v[1], v[3]);
        
        __m128i comb2[4];
        comb2[0] = _mm_unpacklo_epi8(comb[0], comb[1]);
        comb2[1] = _mm_unpackhi_epi8(comb[0], comb[1]);
        comb2[2] = _mm_unpacklo_epi8(comb[2], comb[3]);
        comb2[3] = _mm_unpackhi_epi8(comb[2], comb[3]);

        for (size_t j = 0; j < 4; ++j) {
            _mm_storeu_si128((__m128i*)(&output_data[(i * 4 + j) * 16]), comb2[j]);
        }
    }
}

void decode_fast_double(const uint8_t *input_data, size_t num_elements, uint8_t *output_data)
{
    size_t size = num_elements * sizeof(double);
    const size_t block_size = sizeof(__m128i) * sizeof(double);
    const size_t num_blocks = size / block_size;

    // Handle suffix first.
    const size_t num_processed_elements = (num_blocks * block_size) / sizeof(float);
    for (size_t i = num_processed_elements; i < num_elements; ++i) {
        for (size_t j = 0; j < sizeof(float); ++j) {
            const uint8_t value = input_data[num_elements * j + i];
            output_data[i * sizeof(float) + j] = value;
        }
    }

    for (size_t i = 0; i < num_blocks; ++i) {
        __m128i v[8];
        for (size_t j = 0; j < 8; ++j) {
            v[j] = _mm_loadu_si128((const __m128i*)&input_data[i * 16 + j * num_elements]);
        }
        __m128i comb[8];
        for (size_t j = 0; j < 4; ++j) {
            comb[j] = _mm_unpacklo_epi8(v[j], v[j+4]);
            comb[j+4] = _mm_unpackhi_epi8(v[j], v[j+4]);
        }

        __m128i comb2[8];
        for (size_t j = 0; j < 2; ++j) {
            comb2[j] = _mm_unpacklo_epi8(comb[j], comb[j+2]);
            comb2[j+2] = _mm_unpackhi_epi8(comb[j], comb[j+2]);
            comb2[j+4] = _mm_unpacklo_epi8(comb[j+4], comb[j+2+4]);
            comb2[j+6] = _mm_unpackhi_epi8(comb[j+4], comb[j+2+4]);
        }

        __m128i comb3[8];
        for (size_t j = 0; j < 4; ++j) {
            comb3[j*2] = _mm_unpacklo_epi8(comb2[j*2], comb2[j*2+1]);
            comb3[j*2+1] = _mm_unpackhi_epi8(comb2[j*2], comb2[j*2+1]);
        }

        for (size_t j = 0; j < 8; ++j) {
            _mm_storeu_si128((__m128i*)(&output_data[(i * 8 + j) * 16]), comb3[j]);
        }
    }
}


/********* END SIMD DECODERS ***************/


/********* BEGIN SCALAR ENCODERS AND DECODERS ***************/

template<typename T>
void encode_simple(const T *input_data, size_t num_elements, uint8_t *output_data)
{
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    for (size_t i = 0; i < num_elements; ++i) {
        UnsignedType value ;
        memcpy(&value, &input_data[i], sizeof(T));
        for (size_t k = 0; k < sizeof(T); ++k) {
            output_data[num_elements * k + i] = (value >> (8U * k)) & 0xFFU;
        }
    }
}

template<typename T>
void encode_simple_no_simd(const T *input_data, size_t num_elements, uint8_t *output_data) __attribute__ ((__target__ ("no-sse")));

template<typename T>
void encode_simple_no_simd(const T *input_data, size_t num_elements, uint8_t *output_data)
{
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    for (size_t i = 0; i < num_elements; ++i) {
        UnsignedType value;
        memcpy(&value, &input_data[i], sizeof(T));
        for (size_t k = 0; k < sizeof(T); ++k) {
            output_data[num_elements * k + i] = (value >> (8U * k)) & 0xFFU;
        }
    }
}

// The compiler actually produces very inefficient simd code for double-precision type.
template<typename T>
void decode_simple(const T *input_data, size_t num_elements, uint8_t *output_data)
{
    const uint8_t *input_data_u8 = (const uint8_t*)input_data;
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    for (size_t i = 0; i < num_elements; ++i) {
        UnsignedType value = 0;
        for (size_t k = 0; k < sizeof(T); ++k) {
            value |= ((UnsignedType)(input_data_u8[k * num_elements + i])) << (8U * k);
        }
        memcpy(&output_data[i*sizeof(T)], &value, sizeof(value));
    }
}

template<typename T>
void decode_simple_no_simd(const T *input_data, size_t num_elements, uint8_t *output_data) __attribute__ ((__target__ ("no-sse")));

template<typename T>
void decode_simple_no_simd(const T *input_data, size_t num_elements, uint8_t *output_data)
{
    const uint8_t *input_data_u8 = (const uint8_t*)input_data;
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    for (size_t i = 0; i < num_elements; ++i) {
        UnsignedType value = 0;
        for (size_t k = 0; k < sizeof(T); ++k) {
            value |= ((UnsignedType)(input_data_u8[k * num_elements + i])) << (8U * k);
        }
        memcpy(&output_data[i*sizeof(T)], &value, sizeof(value));
    }
}

/********* END SCALAR ENCODERS AND DECODERS ***************/

template<typename T>
void test_encode_typed() {
    const size_t num_elements = 1024 * 1024 + 13;
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    UnsignedType *input = (UnsignedType*)malloc(num_elements * sizeof(T));
    UnsignedType *output1 = (UnsignedType*)malloc(num_elements * sizeof(T));
    UnsignedType *output2 = (UnsignedType*)malloc(num_elements * sizeof(T));
    srand(1337);
    uint8_t *inputU8 = (uint8_t*)input;
    for (size_t i = 0; i < num_elements; ++i) {
        input[i] = rand();
    }
    encode_simple<T>((const T*)input, num_elements, (uint8_t*)output1);
    encode_fast<T>((const T*)input, num_elements, (uint8_t*)output2);
    if (memcmp(output1, output2, num_elements) == 0) {
        printf("Success\n");
    } else {
        printf("Fail\n");
    }
}

void test_encode() {
    test_encode_typed<float>(); 
    test_encode_typed<double>(); 
}

template<typename T>
void test_decode_typed() {
    const size_t num_elements = 1024 * 1024 + 133;
    using UnsignedType = typename TypeConverter<T>::UnsignedType;
    UnsignedType *input = (UnsignedType*)malloc(num_elements * sizeof(T));
    UnsignedType *output1 = (UnsignedType*)malloc(num_elements * sizeof(T));
    UnsignedType *output2 = (UnsignedType*)malloc(num_elements * sizeof(T));
    srand(1337);
    uint8_t *inputU8 = (uint8_t*)input;
    for (size_t i = 0; i < num_elements * sizeof(T); ++i) {
        inputU8[i] = rand();
    }
    decode_simple<T>((const T*)input, num_elements, (uint8_t*)output1);
    if (std::is_same<T, float>::value) {
        decode_fast_float((const uint8_t*)input, num_elements, (uint8_t*)output2);
    } else {
        decode_fast_double((const uint8_t*)input, num_elements, (uint8_t*)output2);
    }
    /*for (size_t i = 0; i < num_elements * sizeof(T); ) {
        for (size_t j = 0; j < 16; ++j) {
            printf("%x ", ((uint8_t*)input)[i + j]);        
        }
        printf("\n");
        i += 16;
    }
    printf("\n");
    for (size_t i = 0; i < num_elements * sizeof(T); ++i) {
        printf("%d %x %x\n", i, ((uint8_t*)output1)[i], ((uint8_t*)output2)[i]);        
    }*/
    if (memcmp(output1, output2, num_elements) == 0) {
        printf("Success\n");
    } else {
        printf("Fail\n");
    }
}

void test_decode() {
    test_decode_typed<float>();
    test_decode_typed<double>();
}

void benchmark_encode_float() {
    printf("Benchmark float\n");
    const size_t buf_size = 1024UL * 1024UL * 32UL;
    uint8_t *buf = (uint8_t*)malloc(buf_size);
    uint8_t *output_buf = (uint8_t*)malloc(buf_size);
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (uint8_t)i;
        output_buf[i] = 0;
    }

    const size_t cnt = 128;
    double total = 0;
    const size_t num_cases = 8;
    double res[num_cases];
    for (size_t k = 0; k < num_cases; ++k) {
        total = 0;
        for (size_t i = 0; i < cnt; ++i) {
            double t1,t2;
            flush_buf(buf, buf_size);
            flush_buf(output_buf, buf_size);
            
            t1 = gettime();
            switch(k) {
                case 0:
                encode(buf, buf_size/4UL, output_buf);
                break;
                case 1:
                encode_fast((const float*)buf, buf_size/4UL, output_buf);
                break;
                case 2:
                memcpy(output_buf, buf, buf_size);
                break;
                case 3:
                encode_simple<float>((const float*)buf, buf_size/4UL, (uint8_t*)output_buf);
                break;
                case 4:
                encode_simple_no_simd<float>((const float*)buf, buf_size/4UL, (uint8_t*)output_buf);
                break;
                case 5:
                decode_simple<float>((const float*)buf, buf_size/4UL, (uint8_t*)output_buf);
                break;
                case 6:
                decode_simple_no_simd<float>((const float*)buf, buf_size/4UL, (uint8_t*)output_buf);
                break;
                case 7:
                decode_fast_float(buf, buf_size/4UL, (uint8_t*)output_buf);
                break;
                default:
                printf("Error\n");
                exit(-1);
                break;
            }
            t2 = gettime();
            total += (t2-t1);
        }
        total /= cnt;
        res[k] =  ((double)(2UL * buf_size) / (1024UL * 1024UL * 1024UL)) / total;
    }

    printf("encode_SIMD_shuffle: %lf GiB/s\n", res[0]);
    printf("encode_SIMD_unpack: %lf GiB/s\n", res[1]);
    printf("encode_simple: %lf GiB/s\n", res[3]);
    printf("encode_simple_prevent_simd: %lf GiB/s\n", res[4]);
    printf("memcpy: %lf GiB/s\n", res[2]);
    printf("decode_simple: %lf GiB/s\n", res[5]);
    printf("decode_simple_prevent_simd: %lf GiB/s\n", res[6]);
    printf("decode_SIMD_unpack: %lf GiB/s\n", res[7]);
}

void benchmark_encode_double() {
    printf("Benchmark double\n");
    const size_t buf_size = 1024UL * 1024UL * 32UL;
    uint8_t *buf = (uint8_t*)malloc(buf_size);
    uint8_t *output_buf = (uint8_t*)malloc(buf_size);
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (uint8_t)i;
        output_buf[i] = 0;
    }

    const size_t cnt = 128;
    double total = 0;
    const size_t num_cases = 7;
    double res[num_cases];
    for (size_t k = 0; k < num_cases; ++k) {
        total = 0;
        for (size_t i = 0; i < cnt; ++i) {
            double t1,t2;
            flush_buf(buf, buf_size);
            flush_buf(output_buf, buf_size);
            
            t1 = gettime();
            switch(k) {
                case 0:
                encode_fast((const double*)buf, buf_size/8UL, output_buf);
                break;
                case 1:
                memcpy(output_buf, buf, buf_size);
                break;
                case 2:
                encode_simple<double>((const double*)buf, buf_size/8UL, (uint8_t*)output_buf);
                break;
                case 3:
                encode_simple_no_simd<double>((const double*)buf, buf_size/8UL, (uint8_t*)output_buf);
                break;
                case 4:
                decode_simple<double>((const double*)buf, buf_size/8UL, (uint8_t*)output_buf);
                break;
                case 5:
                decode_simple_no_simd<double>((const double*)buf, buf_size/8UL, (uint8_t*)output_buf);
                break;
                case 6:
                decode_fast_double(buf, buf_size/8UL, (uint8_t*)output_buf);
                break;
                default:
                printf("Error\n");
                exit(-1);
                break;
            }
            t2 = gettime();
            total += (t2-t1);
        }
        total /= cnt;
        res[k] =  ((double)(2UL * buf_size) / (1024UL * 1024UL * 1024UL)) / total;
    }

    printf("encode_SIMD_unpack: %lf GiB/s\n", res[0]);
    printf("encode_simple: %lf GiB/s\n", res[2]);
    printf("encode_simple_prevent_simd: %lf GiB/s\n", res[3]);
    printf("memcpy: %lf GiB/s\n", res[1]);
    printf("decode_simple: %lf GiB/s\n", res[4]);
    printf("decode_simple_prevent_simd: %lf GiB/s\n", res[5]);
    printf("decode_simd_unpack: %lf GiB/s\n", res[6]);
}


void benchmark_decode() {

}

int main() {
    test_encode();
    test_decode();
    //benchmark_encode_float();
    //benchmark_encode_double();
    return 0;
}
