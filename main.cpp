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

void runTest(const std::string &fname)
{
    const auto table = readParquetFile(fname);

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
    std::cout << "Run as: parquet_test file1.parquet file2.parquet ... file_N.parquet" << std::endl;
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc == 1)
    {
        std::cout << "Please provide input file" << std::endl;
        printHelp();
        return -1;
    }

    for (int i = 1; i < argc; ++i)
    {
        const char *fileName = argv[i];
        runTest(fileName);
    }

    return 0;
}
