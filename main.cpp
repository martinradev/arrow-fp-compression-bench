#include <iostream>
#include "arrow/builder.h"
#include "arrow/table.h"
#include "arrow/type.h"
#include "arrow/io/file.h"
#include "arrow/pretty_print.h"
#include "arrow/util/compression.h"
#include <parquet/arrow/writer.h>
#include <parquet/arrow/reader.h>
#include <parquet/properties.h>
#include <parquet/types.h>

#include <unordered_map>
#include <fstream>
#include <cstring>
#include <sys/time.h>

namespace
{

inline double gettime() {
    struct timespec ts = {0};
    int err = clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)err;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}


// Reads a parquet file and returns an arrow::Table object.
auto readParquetFile(const std::string &fname)
{
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_THROW_NOT_OK(arrow::io::ReadableFile::Open(
      fname, arrow::default_memory_pool(), &infile));

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(
          parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));
    std::shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

    return std::move(table);
}

template<typename T>
auto transformRawVectorToArrowTable(const std::vector<T> &data)
{
    std::cerr << "This should not be reachable" << std::endl;
}

template<>
auto transformRawVectorToArrowTable<float>(const std::vector<float> &data)
{
    arrow::FloatBuilder builder;
    PARQUET_THROW_NOT_OK(builder.AppendValues(data));
    std::shared_ptr<arrow::Array> array;
    PARQUET_THROW_NOT_OK(builder.Finish(&array));

    std::shared_ptr<arrow::Schema> schema = arrow::schema(
        {arrow::field("values", arrow::float32())});
    
    return arrow::Table::Make(schema, {array});
}

template<>
auto transformRawVectorToArrowTable<double>(const std::vector<double> &data)
{
    arrow::DoubleBuilder builder;
    PARQUET_THROW_NOT_OK(builder.AppendValues(data));
    std::shared_ptr<arrow::Array> array;
    PARQUET_THROW_NOT_OK(builder.Finish(&array));

    std::shared_ptr<arrow::Schema> schema = arrow::schema(
        {arrow::field("values", arrow::float64())});
    
    return arrow::Table::Make(schema, {array});
}

template<typename T>
auto readBinaryFile(const std::string &fname)
{
    std::vector<T> data;
    
    std::ifstream input(fname, std::ifstream::in | std::ifstream::binary);

    input.seekg(0, input.end);
    size_t fileSize = input.tellg();
    input.seekg(0, input.beg);

    data.resize(fileSize / sizeof(T));

    input.read(reinterpret_cast<char*>(data.data()), fileSize);

    input.close();

    return data;
}

// Prints information on how well each chunk is distributed to give an idea
// on how repetitive the data is.
#if 0
void getStatistics(const std::string &fname)
{
    auto table = readParquetFile(fname);
    int numColumns = table->num_columns();
    for (int i = 0; i < numColumns; ++i)
    {
        const auto &col = table->column(i);
        if (!arrow::is_floating(col->type()->id()))
        {
            continue;
        }
        const auto &data = col->data();
        int numChunks = data->num_chunks();
        for (int i = 0; i < numChunks; ++i)
        {
            const auto &chunk = data->chunk(i);
            const auto &arr = chunk->data();
            if (chunk->type_id() == arrow::Type::DOUBLE)
            {
                bool skip = true;
                for (const auto &buf : arr->buffers)
                {
                    if (skip)
                    {
                        // Every second buffer seems to be invalid data, so let's have this hack to skip it.
                        skip = !skip;
                        continue;
                    }
                    const uint8_t *rawData = buf->data();
                    int64_t rawDataSize = buf->size();
                    std::unordered_map<double, uint64_t> distr;
                    for (int j = 0; j < rawDataSize; j+=8)
                    {
                        double v = *reinterpret_cast<const double*>(rawData);
                        auto it = distr.find(v);
                        if (it == distr.end())
                        {
                            distr.insert(std::make_pair(v, 1));
                        }
                        else
                        {
                            it->second += 1;
                        }
                        rawData += 8;
                    }
                    std::cout << "Num unique: " << distr.size() << "/" << (rawDataSize/8) << std::endl;
                }
            }
        }
    }
}
#endif

// Saves the table using the specified compression algorithm, FP encoding and dictionary encoding.
void runTest(const std::string &fileName,
             const std::shared_ptr<arrow::Table> &table,
             parquet::Compression::type compression,
             parquet::Encoding::type encodingType,
             int32_t compressionLevel,
             uint8_t lossyCompressionPrecision,
             size_t numRuns)
{
    std::string encoding;
    switch(encodingType)
    {
        case parquet::Encoding::type::BYTE_STREAM_SPLIT:
            encoding += "fp";
            break;
        case parquet::Encoding::type::BYTE_STREAM_SPLIT_XOR:
            encoding += "fp_xor";
            break;
        case parquet::Encoding::type::BYTE_STREAM_SPLIT_BYTE_RLE:
            encoding += "fp_rle";
            break;
        case parquet::Encoding::type::BYTE_STREAM_SPLIT_COMPONENT:
            encoding += "fp_comp";
            break;
        case parquet::Encoding::type::BYTE_STREAM_SPLIT_RYANBLUE_RLE:
            encoding += "fp_rb";
            break;
        case parquet::Encoding::type::RLE_DICTIONARY:
            encoding += "dict";
            break;
        case parquet::Encoding::type::PLAIN:
            encoding += "no_enc";
            break;
        default:
            std::cerr << "We cannot have both dictionary and fp" << std::endl;
            exit(-1);
    }
    std::string newName = fileName + "." + arrow::util::Codec::GetCodecAsString(compression) + "." + encoding;
    if (compressionLevel != -1)
    {
        newName += "." + std::to_string(compressionLevel);
    }
    newName += "_pre" + std::to_string(lossyCompressionPrecision);

    parquet::WriterProperties::Builder props_builder;

    const auto &schema = table->schema();
    const auto &fields = schema->fields();
    for (const auto &field : fields)
    {
        // Tamper with only columns which are for FP data.
        if (arrow::is_floating(field->type()->id()))
        {
            // We cannot have both byte_stream_split and dictionary encoding.
            if (encodingType == parquet::Encoding::type::RLE_DICTIONARY)
            {
                props_builder.enable_dictionary(field->name());
            }
            else
            {
                props_builder.disable_dictionary(field->name());
                props_builder.encoding(field->name(), encodingType);
            }
            if (compressionLevel != -1)
            {
                props_builder.compression_level(compressionLevel);
            }
            props_builder.lossy_compression_precision(field->name(), lossyCompressionPrecision);
        }
    }
    props_builder.compression(compression);
    
    auto props = props_builder.build();
    double totalTime = .0;
    for (size_t i = 0; i < numRuns; ++i)
    {
        std::shared_ptr<arrow::io::FileOutputStream> file_output_stream;
        arrow::io::FileOutputStream::Open(newName, &file_output_stream);
        double t1 = gettime();
        parquet::arrow::WriteTable(*table, ::arrow::default_memory_pool(),
           file_output_stream, table->num_rows(), props);
        double t2 = gettime();
        totalTime += (t2-t1);
    }
    std::cout << newName << ": " << (totalTime / numRuns) << std::endl;
}

void printHelp()
{
    std::cout << "Run as:" << std::endl;
    std::cout << "parquet_test [OPTION] ..." << std::endl;
    std::cout << std::endl;
    std::cout << "Possible options:" << std::endl;
    std::cout << " " << "-p [FILE] ..." << std::endl;
    std::cout << "  " << "Reads the specifial file as a parquet file." << std::endl;
    std::cout << std::endl;
    std::cout << " " << "-b [FILE] ..." << std::endl;
    std::cout << "  " << "Read a raw binary file of FP32 or FP64 values." << std::endl;
    std::cout << "  " << "For FP32, the file extension should be .sp." << std::endl;
    std::cout << "  " << "For FP64, the file extension should be .dp." << std::endl;
    std::cout << " " << "-c [CODEC],[ENCODING],[COMPRESSION_LEVEL],[PRECISION] ..." << std::endl;
    std::cout << "  " << "CODEC must be one of the following:" << std::endl;
    std::cout << "   " << "zstd, gzip, snappy, lz4, zfp, uncompressed" << std::endl;
    std::cout << "  " << "ENCODING must be one of the following:" << std::endl;
    std::cout << "   " << "plain, dictionary, split" << std::endl;
    std::cout << "  " << "COMPRESSION_LEVEL depends on the codec being used." << std::endl;
    std::cout << "  " << "Pass -1 if you want to use the default compression level." << std::endl;
    std::cout << "  " << "PRECISION determines the precision for ZFP." << std::endl;
    std::cout << "  " << "It always has to be specified and it is dependent on the data type." << std::endl;
    std::cout << std::endl;
    std::cout << "Example runs:" << std::endl;
    std::cout << "  " << "parquet_test -p file.parquet -c zstd,plain,6 gzip,dictionary,-1" << std::endl;
    std::cout << "   " << "Reads file.parquet. It first tries to create a new parquet file" << std::endl;
    std::cout << "   " << "for which each FP column uses ZSTD as a codec with compression level 6" << std::endl;
    std::cout << "   " << "and plain encoding." << std::endl;
    std::cout << "  " << "parquet_test -b mydata.sp -c snappy,split,-1" << std::endl;
    std::cout << "   " << "Reads the binary file consisting of F32 values and generates a parquet file" << std::endl;
    std::cout << "   " << "using snappy as a codec with BYTE_STREAM_SPLIT encoding. The compression level" << std::endl;
    std::cout << "   " << "is the default on." << std::endl;
}

void handleInvalidArg()
{
    std::cout << "Invalid arguments" << std::endl;
    printHelp();
    exit(-1);
}

parquet::Compression::type GetCompressionTypeFromString(const char *compressionName)
{
    if (strcmp(compressionName, "gzip") == 0)
    {
        return parquet::Compression::GZIP;
    }
    else if (strcmp(compressionName, "zstd") == 0)
    {
        return parquet::Compression::ZSTD;
    }
    else if (strcmp(compressionName, "snappy") == 0)
    {
        return parquet::Compression::SNAPPY;
    }
    else if (strcmp(compressionName, "lz4") == 0)
    {
        return parquet::Compression::LZ4;
    }
    else if (strcmp(compressionName, "uncompressed") == 0)
    {
        return parquet::Compression::UNCOMPRESSED;
    }
    else if (strcmp(compressionName, "zfp") == 0)
    {
        return parquet::Compression::ZFP;
    }
    else
    {
        handleInvalidArg();
    }
}

parquet::Encoding::type GetEncodingTypeFromString(const char *encodingName)
{
    if (strcmp(encodingName, "plain") == 0)
    {
        return parquet::Encoding::type::PLAIN;
    }
    else if (strcmp(encodingName, "dictionary") == 0)
    {
        return parquet::Encoding::type::RLE_DICTIONARY;
    }
    else if (strcmp(encodingName, "split") == 0)
    {
        return parquet::Encoding::type::BYTE_STREAM_SPLIT;
    }
    else if (strcmp(encodingName, "split_xor") == 0)
    {
        return parquet::Encoding::type::BYTE_STREAM_SPLIT_XOR;
    }
    else if (strcmp(encodingName, "split_component") == 0)
    {
        return parquet::Encoding::type::BYTE_STREAM_SPLIT_COMPONENT;
    }
    else if (strcmp(encodingName, "split_rle") == 0)
    {
        return parquet::Encoding::type::BYTE_STREAM_SPLIT_BYTE_RLE;
    }
    else if (strcmp(encodingName, "split_ryanblue_rle") == 0)
    {
        return parquet::Encoding::type::BYTE_STREAM_SPLIT_RYANBLUE_RLE;
    }
    else
    {
        handleInvalidArg();
    }
}

struct TestParameters
{
    parquet::Compression::type compression;
    parquet::Encoding::type encoding;
    int32_t compressionLevel;
    uint8_t precision;
};

enum class FileType
{
    RawFloatFile,
    RawDoubleFile,
    ParquetFile
};

struct TestFile
{
    FileType type;
    std::string fileName;
};

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 3)
    {
        handleInvalidArg();
    }

    std::vector<TestParameters> testJobs;
    std::vector<TestFile> files;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (arg[0] == '-')
        {
            if (strcmp(arg, "-p") == 0)
            {
                int j;
                for (j = i + 1; j < argc; ++j)
                {
                    arg = argv[j];
                    std::string fname(arg);
                    if (fname.length() <= 8)
                    {
                        break;
                    }
                    if (fname.compare(fname.length() - 8, 8, ".parquet") == 0)
                    {
                        files.push_back({FileType::ParquetFile, fname});
                    }
                    else
                    {
                        break;
                    }
                }
                i = j - 1;
            }
            else if (strcmp(arg, "-b") == 0)
            {
                int j;
                for (j = i + 1; j < argc; ++j)
                {
                    arg = argv[j];
                    std::string fname(arg);
                    if (fname.length() <= 3)
                    {
                        break;
                    }
                    if (fname.compare(fname.length() - 3, 3, ".sp") == 0)
                    {
                        files.push_back({FileType::RawFloatFile, fname});
                    }
                    else if (fname.compare(fname.length() - 3, 3, ".dp") == 0)
                    {
                        files.push_back({FileType::RawDoubleFile, fname});
                    }
                    else
                    {
                        break;
                    }
                }
                i = j - 1;
            }
            else if (strcmp(arg, "-c") == 0)
            {
                int j;
                for (j = i + 1; j < argc; ++j)
                {
                    if (argv[j][0] != '-')
                    {
                        char *combination = argv[j];
                        char *splitBorder = strstr(combination, ",");
                        if (splitBorder == nullptr)
                        {
                            handleInvalidArg();
                        }
                        splitBorder[0] = '\0';
                        char *compression = combination;
                        char *encoding = splitBorder + 1;
                        splitBorder = strstr(encoding, ",");
                        if (splitBorder == nullptr)
                        {
                            handleInvalidArg();
                        }
                        splitBorder[0] = '\0';
                        const int32_t compressionLevel = atoi(splitBorder+1);
                        const auto compressionType = GetCompressionTypeFromString(compression);
                        const auto encodingType = GetEncodingTypeFromString(encoding);
                        splitBorder = strstr(splitBorder + 1, ",");
                        if (splitBorder == nullptr)
                        {
                            handleInvalidArg();
                        }
                        splitBorder[0] = '\0';
                        char *precisionStr = splitBorder + 1;
                        const uint8_t precision = atoi(precisionStr);

                        TestParameters job =
                        {
                            compressionType,
                            encodingType,
                            compressionLevel,
                            precision
                        };
                        testJobs.push_back(job);
                    }
                }
                i = j - 1;
            }
            else
            {
                handleInvalidArg();
            }
            continue;
        } else {
            handleInvalidArg();
        }
    }
    for (const auto &file : files)
    {
        std::shared_ptr<arrow::Table> table;
        const auto &fileName = file.fileName;
        switch(file.type)
        {
            case FileType::ParquetFile:
                table = readParquetFile(fileName);
                break;
            case FileType::RawFloatFile:
                table = transformRawVectorToArrowTable(readBinaryFile<float>(fileName));
                break;
            case FileType::RawDoubleFile:
                table = transformRawVectorToArrowTable(readBinaryFile<double>(fileName));
                break;
        }
        for (const auto &job : testJobs)
        {
            runTest(fileName, table, job.compression, job.encoding, job.compressionLevel, job.precision, 2U);
        }
    }

    return 0;
}
