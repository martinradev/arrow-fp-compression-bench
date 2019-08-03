#include <iostream>
#include "arrow/builder.h"
#include "arrow/table.h"
#include "arrow/type.h"
#include "arrow/io/file.h"
#include "arrow/pretty_print.h"
#include <parquet/arrow/writer.h>
#include <parquet/arrow/reader.h>
#include <parquet/properties.h>
#include <parquet/types.h>

#include <unordered_map>
#include <fstream>
#include <cstring>

namespace
{

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

// Saves the table using the specified compression algorithm, FP encoding and dictionary encoding.
void runTest(const std::string &fileName, const std::shared_ptr<arrow::Table> &table, parquet::Compression::type compression, bool useFP, bool useDictionary)
{
    std::string encoding;
    if (useFP)
    {
        encoding += "fp";
    }
    else if (useDictionary)
    {
        encoding += "dict";
    }
    else if (!useFP && !useDictionary)
    {
        encoding += "no_enc";
    }
    else
    {
        std::cerr << "We cannot have both dictionary and fp" << std::endl;
        exit(-1);
    }
    std::string newName = fileName + "." + parquet::CompressionToString(compression) + "." + encoding;

    std::shared_ptr<arrow::io::FileOutputStream> file_output_stream;
    arrow::io::FileOutputStream::Open(newName, &file_output_stream);
    parquet::WriterProperties::Builder props_builder;

    const auto &schema = table->schema();
    const auto &fields = schema->fields();
    for (const auto &field : fields)
    {
        // Tamper with only columns which are for FP data.
        if (arrow::is_floating(field->type()->id()))
        {
            // We cannot have both byte_stream_split and dictionary encoding.
            if (useFP)
            {
                props_builder.encoding(field->name(), parquet::Encoding::type::BYTE_STREAM_SPLIT);
            }
            if (useDictionary)
            {
                props_builder.enable_dictionary(field->name());
            }
            else
            {
                props_builder.disable_dictionary(field->name());
            }
        }
    }
    props_builder.compression(compression);
    
    auto props = props_builder.build();
    parquet::arrow::WriteTable(*table, ::arrow::default_memory_pool(),
       file_output_stream, table->num_rows(), props);
}

void runTest(const std::string &fname, const std::shared_ptr<arrow::Table> &table)
{
    // Generate uncompressed parquet file.
    runTest(fname, table, parquet::Compression::UNCOMPRESSED, false, false);

    // Use GZIP.
    runTest(fname, table, parquet::Compression::GZIP, false, false);
    runTest(fname, table, parquet::Compression::GZIP, true, false);
    runTest(fname, table, parquet::Compression::GZIP, false, true);

    // Use ZSTD.
    runTest(fname, table, parquet::Compression::ZSTD, false, false);
    runTest(fname, table, parquet::Compression::ZSTD, true, false);
    runTest(fname, table, parquet::Compression::ZSTD, false, true);

    // Use SNAPPY.
    runTest(fname, table, parquet::Compression::SNAPPY, false, false);
    runTest(fname, table, parquet::Compression::SNAPPY, true, false);
    runTest(fname, table, parquet::Compression::SNAPPY, false, true);

    // Use LZ4.
    runTest(fname, table, parquet::Compression::LZ4, false, false);
    runTest(fname, table, parquet::Compression::LZ4, true, false);
    runTest(fname, table, parquet::Compression::LZ4, false, true);
}

void printHelp()
{
    std::cout << "Run as:" << std::endl;
    std::cout << "parquet_test [OPTION] [FILE] ..." << std::endl;
    std::cout << std::endl;
    std::cout << "Possible options:" << std::endl;
    std::cout << "\t" << "-p [FILE] ..." << std::endl;
    std::cout << "\t\t" << "Reads each file as a parquet file and" << std::endl;
    std::cout << "\t\t" << "iterates over a combination of BYTE_STREAM_SPLIT|DICTIONARY encoding" << std::endl;
    std::cout << "\t\t" << "and a compression algorithm among GZIP, ZSTD, SNAPPY and LZ4." << std::endl;
    std::cout << std::endl;
    std::cout << "\t" << "-b [FILE] ..." << std::endl;
    std::cout << "\t\t" << "Read a raw binary file of FP32 or FP64 values and performs the same" << std::endl;
    std::cout << "\t\t" << "as with the -p option. The output is parquet files." << std::endl;
    std::cout << "\t\t" << "For FP32, the file extension should be .sp." << std::endl;
    std::cout << "\t\t" << "For FP64, the file extension should be .dp." << std::endl;
}

void handleInvalidArg()
{
    std::cout << "Invalid arguments" << std::endl;
    printHelp();
    exit(-1);
}

enum class Option
{
    None,
    ReadParquetFile,
    ReadBinaryFile
};

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 3)
    {
        handleInvalidArg();
    }

    Option opt = Option::None;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (arg[0] == '-')
        {
            if (strcmp(arg, "-p") == 0)
            {
                opt = Option::ReadParquetFile;
            }
            else if (strcmp(arg, "-b") == 0)
            {
                opt = Option::ReadBinaryFile;
            }
            else
            {
                handleInvalidArg();
            }
            continue;
        }
        const char *fileName = arg;
        if (opt == Option::None)
        {
            handleInvalidArg();
        }
        else if (opt == Option::ReadParquetFile)
        {
            std::string fname(arg);
            if (fname.length() < 8)
            {
                handleInvalidArg();
            }
            if (fname.compare(fname.length() - 8, 8, ".parquet") != 0)
            {
                std::cerr << "A parquet file should end with the \".parquet\" suffix." << std::endl;
                return -1;
            }
            runTest(arg, readParquetFile(arg));
        }
        else if (opt == Option::ReadBinaryFile)
        {
            std::string fname(arg);
            if (fname.length() < 3)
            {
                handleInvalidArg();
            }
            std::shared_ptr<arrow::Table> table = nullptr;
            if (fname.compare(fname.length() - 3, 3, ".sp") == 0)
            {
                table = transformRawVectorToArrowTable(readBinaryFile<float>(fname));
            }
            else if (fname.compare(fname.length() - 3, 3, ".dp") == 0)
            {
                table = transformRawVectorToArrowTable(readBinaryFile<double>(fname));
            }
            else
            {
                std::cerr << "Invalid file name: " << fname << std::endl;
                return -1;
            }
            runTest(fname, table);
        }
    }

    return 0;
}
