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
#include <parquet/file_reader.h>

#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <cstring>
#include <tuple>
#include <sys/time.h>
#include <vector>
#include <libgen.h>

namespace
{

struct TestResult {
    std::string file_name;
    std::string compression_name;
    std::string encoding_name;
    int32_t compression_level;
    uint64_t original_size;
    uint64_t compressed_size;
    double write_time_in_s;
    double read_time_in_s;
};

void print_result(const TestResult &result) {
    const double compression_ratio = (double)result.original_size / result.compressed_size;
    const double compression_speed = ((double)result.original_size / (1024*1024)) / result.write_time_in_s;
    const double decompression_speed = ((double)result.original_size / (1024*1024)) / result.read_time_in_s;
    std::cout << result.file_name << " " << result.compression_name << " "
              << result.encoding_name << " " << result.compression_level << " "
              << compression_ratio << " " << compression_speed << " " << decompression_speed << std::endl;
}

inline double gettime() {
    struct timespec ts = {0};
    int err = clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)err;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

// Reads a parquet file and returns an arrow::Table object.
auto readParquetFile(const std::string &fname)
{
    arrow::Result<std::shared_ptr<arrow::io::ReadableFile> > infile = arrow::io::ReadableFile::Open(
      fname, arrow::default_memory_pool());
    if (!infile.ok()) {
        std::cerr << "Couldn't open file " << fname << std::endl;
        exit(-1);
    }

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(
          parquet::arrow::OpenFile(*infile, arrow::default_memory_pool(), &reader));
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

// Saves the table using the specified compression algorithm, FP encoding and dictionary encoding.
void runTest(const std::string &fileName,
             const std::shared_ptr<arrow::Table> &table,
             uint64_t original_size,
             parquet::Compression::type compression,
             parquet::Encoding::type encodingType,
             int32_t compressionLevel,
             size_t numRuns,
             bool use_io,
             TestResult &result)
{
    std::string compression_name = arrow::util::Codec::GetCodecAsString(compression);
    std::string encoding_name;
    switch(encodingType)
    {
        case parquet::Encoding::type::BYTE_STREAM_SPLIT:
            encoding_name += "BYTE_STREAM_SPLIT";
            break;
        case parquet::Encoding::type::RLE_DICTIONARY:
            encoding_name += "RLE_DICTIONARY";
            break;
        case parquet::Encoding::type::PLAIN:
            encoding_name += "PLAIN";
            break;
        default:
            std::cerr << "We cannot have both dictionary and fp" << std::endl;
            exit(-1);
    }

    parquet::WriterProperties::Builder props_builder;
    props_builder.data_pagesize(1024 * 1024 * 16);

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
        }
    }
    props_builder.compression(compression);

    auto props = props_builder.build();
    double totalTime = .0;
    double totalDecompressTime = .0;
    int64_t sz;
    std::shared_ptr<arrow::io::FileOutputStream> file_output_stream;
    for (size_t i = 0; i < numRuns; ++i)
    {
        if (use_io) {
            const std::string save_file_name = "/tmp/tmp_arrow_file.txt";
            auto result = arrow::io::FileOutputStream::Open(save_file_name);
            if (!result.ok()) {
                std::cerr << "Couldn't create an output stream" << std::endl;
                exit(-1);
            }
            std::shared_ptr<arrow::io::FileOutputStream> file_output_stream = *result;
            arrow::Status status;
            system("sync");
            double t1 = gettime();
            status = parquet::arrow::WriteTable(*table, ::arrow::default_memory_pool(),
               file_output_stream, table->num_rows(), props);
                file_output_stream->Flush();
                file_output_stream->Close();
            system("sync"); // this will add some latency, yes.
            double t2 = gettime();
            totalTime += (t2-t1);
            if (!status.ok()) {
                std::cerr << "Failed to write parquet" << status.message() << std::endl;
            }

            std::unique_ptr<parquet::arrow::FileReader> reader;
            parquet::arrow::FileReaderBuilder builder;

            arrow::Result<std::shared_ptr<arrow::io::ReadableFile> > infile = arrow::io::ReadableFile::Open(
                  save_file_name, arrow::default_memory_pool());
            builder.Open(*infile);

            // Clear caches
            system("sudo /sbin/sysctl -w vm.drop_caches=3 > /dev/null");

            builder.properties(parquet::default_arrow_reader_properties())->Build(&reader);
            t1 = gettime();
            std::shared_ptr<arrow::Table> out;
            status = reader->ReadTable(&out);
            t2 = gettime();
            totalDecompressTime += (t2-t1);
            if (!status.ok()) {
                std::cerr << "Failed to read parquet " << status.message() << std::endl;
            }
            if (!table->Equals(*out, false)) {
                std::cerr << "Table after decompression differs" << std::endl;
            }

            std::ifstream in(save_file_name, std::ifstream::ate | std::ifstream::binary);
            sz = in.tellg();

        } else {
            auto result =
                arrow::io::BufferOutputStream::Create(1024 * 1024 * 1024, ::arrow::default_memory_pool());
            auto ok = result.ok();
            if (!ok) {
                std::cerr << "Couldn't create an output stream" << std::endl;
                exit(-1);
            }
            std::shared_ptr<arrow::io::BufferOutputStream> buf_output_stream = *result;
            arrow::Status status;
            double t1 = gettime();
            status = parquet::arrow::WriteTable(*table, ::arrow::default_memory_pool(),
               buf_output_stream, table->num_rows(), props);
            double t2 = gettime();
            totalTime += (t2-t1);
            if (!status.ok()) {
                std::cerr << "Failed to write parquet" << status.message() << std::endl;
            }

            arrow::Result<std::shared_ptr<arrow::Buffer> > buffer = buf_output_stream->Finish();
            std::unique_ptr<parquet::arrow::FileReader> reader;
            parquet::arrow::FileReaderBuilder builder;
            builder.Open(std::make_shared<arrow::io::BufferReader>(*buffer));
            builder.properties(parquet::default_arrow_reader_properties())->Build(&reader);
            t1 = gettime();
            std::shared_ptr<arrow::Table> out;
            status = reader->ReadTable(&out);
            t2 = gettime();
            totalDecompressTime += (t2-t1);
            if (!status.ok()) {
                std::cerr << "Failed to read parquet " << status.message() << std::endl;
            }
            if (!table->Equals(*out, false)) {
                std::cerr << "Table after decompression differs" << std::endl;
            }

            arrow::Result<int64_t> res_sz = buf_output_stream->Tell();
            sz = *res_sz;
        }
    }
    double avg_compress_time = totalTime / numRuns;
    double avg_decompress_time = totalDecompressTime / numRuns;
    unsigned long compress_mbs = (sz / (1024 * 1024)) / avg_compress_time;
    unsigned long decompress_mbs = (sz / (1024 * 1024)) / avg_decompress_time;
    //std::cout << newName << ": " << avg_compress_time << " " << avg_decompress_time << " " << sz << " " << compress_mbs << " " << decompress_mbs << std::endl;
    char *tmp_file_name = strdup(fileName.c_str());
    result.file_name = std::string(basename(tmp_file_name));
    free(tmp_file_name);
    result.original_size = original_size;
    result.compressed_size = sz;
    result.compression_name = compression_name;
    result.encoding_name = encoding_name;
    result.compression_level = compressionLevel;
    result.write_time_in_s = avg_compress_time;
    result.read_time_in_s = avg_decompress_time;
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
    std::cout << " " << "-c [CODEC],[ENCODING],[COMPRESSION_LEVEL] ..." << std::endl;
    std::cout << "  " << "CODEC must be one of the following:" << std::endl;
    std::cout << "   " << "zstd, gzip, snappy, lz4, zfp, uncompressed" << std::endl;
    std::cout << "  " << "ENCODING must be one of the following:" << std::endl;
    std::cout << "   " << "plain, dictionary, split" << std::endl;
    std::cout << "  " << "COMPRESSION_LEVEL depends on the codec being used." << std::endl;
    std::cout << "  " << "Pass -1 if you want to use the default compression level." << std::endl;
    std::cout << std::endl;
    std::cout << " " << "-r K" << std::endl;
    std::cout << "  " << "Run the compression/decompression K times." << std::endl;
    std::cout << std::endl;
    std::cout << "Example runs:" << std::endl;
    std::cout << "  " << "parquet_test -p file.parquet -c zstd,plain,6 gzip,dictionary,-1" << std::endl;
    std::cout << "   " << "Reads file.parquet. It first tries to create a new parquet file" << std::endl;
    std::cout << "   " << "for which each FP column uses ZSTD as a codec with compression level 6" << std::endl;
    std::cout << "   " << "and plain encoding." << std::endl;
    std::cout << "  " << "parquet_test -b mydata.sp -c snappy,split,-1 -r 8" << std::endl;
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
    unsigned long num_rounds = 16;
    bool use_io = false;
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

                        TestParameters job =
                        {
                            compressionType,
                            encodingType,
                            compressionLevel,
                        };
                        testJobs.push_back(job);
                    } else {
                        break;
                    }
                }
                i = j - 1;
            }
            else if (strcmp(arg, "-r") == 0) {
                i += 1;
                if (i == argc) {
                    handleInvalidArg();
                    break;
                }
                num_rounds = strtoul(argv[i], NULL, 10);
            }
            else if (strcmp(arg, "-io") == 0) {
                use_io = true;
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
        uint64_t file_size;
        switch(file.type)
        {
            case FileType::ParquetFile:
                table = readParquetFile(fileName);
                break;
            case FileType::RawFloatFile:
                {
                const auto data = readBinaryFile<float>(fileName);
                file_size = data.size() * sizeof(float);
                table = transformRawVectorToArrowTable(data);
                break;
                }
            case FileType::RawDoubleFile:
                {
                const auto data = readBinaryFile<double>(fileName);
                file_size = data.size() * sizeof(double);
                table = transformRawVectorToArrowTable(data);
                break;
                }
        }
        for (const auto &job : testJobs)
        {
            TestResult result;
            runTest(fileName, table, file_size, job.compression, job.encoding, job.compressionLevel, num_rounds, use_io, result);
            print_result(result);
        }
    }

    return 0;
}
