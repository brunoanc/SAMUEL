#include "FileExporter.h"

namespace HAYDEN
{
    // FileExportList - Subroutines
    std::vector<byte> FileExportList::DecompressEmbeddedFileHeader(std::vector<byte> embeddedHeader, const uint64 decompressedSize)
    {
        if (embeddedHeader.size() != decompressedSize)
        {
            std::vector<byte> decompressedHeader = oodleDecompress(embeddedHeader, decompressedSize);
            return decompressedHeader;
        }
        return embeddedHeader;
    }
    uint64 FileExportList::CalculateStreamDBIndex(const uint64 resourceId, const int mipCount) const
    {
        // Get hex bytes string
        std::string hexBytes = intToHex(resourceId);

        // Reverse each byte
        for (int i = 0; i < hexBytes.size(); i += 2)
            std::swap(hexBytes[i], hexBytes[i + (int64)1]);

        // Shift digits to the right
        hexBytes = hexBytes.substr(hexBytes.size() - 1) + hexBytes.substr(0, hexBytes.size() - 1);

        // Reverse each byte again
        for (int i = 0; i < hexBytes.size(); i += 2)
            std::swap(hexBytes[i], hexBytes[i + (int64)1]);

        // Get second digit based on mip count
        hexBytes[1] = intToHex((char)(6 + mipCount))[1];

        // Convert hex string back to uint64 and return
        return hexToInt64(hexBytes);
    }

    // FileExportList - Helper functions for FileExportList constructor
    void FileExportList::GetResourceEntries(const ResourceFile& resourceFile)
    {
        // Iterate through currently loaded .resource entries to find supported files
        for (uint64 i = 0; i < resourceFile.numFileEntries; i++)
        {
            ResourceEntry thisEntry = resourceFile.resourceEntries[i];

            // each instance of FileExportList is for a different filetype. 21 = .tga, 31 = .md6mesh
            if (thisEntry.version != _FileType)
                continue;

            // skips entries with no data to extract
            if (thisEntry.dataSizeCompressed == 0)
                continue;

            // skip unsupported images
            if (thisEntry.version == 21)
            {
                // skip entries with "lightprobes" path
                if (thisEntry.name.rfind("/lightprobes/") != -1)
                    continue;

                // skip wierd minmip files
                if (thisEntry.name.rfind("$minmip=") != -1)
                    continue;
            }

            FileExportItem exportItem;
            exportItem.resourceFileName = thisEntry.name;
            exportItem.resourceFileOffset = thisEntry.dataOffset;
            exportItem.resourceFileCompressedSize = thisEntry.dataSizeCompressed;
            exportItem.resourceFileDecompressedSize = thisEntry.dataSizeDecompressed;
            exportItem.resourceFileHash = thisEntry.hash;

            _ExportItems.push_back(exportItem);
        }
        return;
    }
    void FileExportList::ParseEmbeddedFileHeaders(const ResourceFile& resourceFile)
    {
        // Open .resource file containing embedded TGA data.
        FILE* f = fopen(resourceFile.filename.c_str(), "rb");
        if (f == NULL)
        {
            fprintf(stderr, "Error: failed to open %s for reading.\n", resourceFile.filename.c_str());
            return;
        }
        
        // read compressed file headers into memory
        for (uint64 i = 0; i < _ExportItems.size(); i++)
        {
            FileExportItem& thisFile = _ExportItems[i];
            std::vector<byte> embeddedHeader = resourceFile.GetEmbeddedFileHeader(f, thisFile.resourceFileOffset, thisFile.resourceFileCompressedSize);
            std::vector<byte> decompressedHeader = DecompressEmbeddedFileHeader(embeddedHeader, thisFile.resourceFileDecompressedSize);

            if (decompressedHeader.empty())
                continue; // decompression error
        
            if (_FileType == 21)
            {
                EmbeddedTGAHeader embeddedTGAHeader = resourceFile.ReadTGAHeader(decompressedHeader);
                _TGAHeaderData.push_back(embeddedTGAHeader);
            }

            if (_FileType == 31)
            {
                EmbeddedMD6Header embeddedMD6Header = resourceFile.ReadMD6Header(decompressedHeader);
                _MD6HeaderData.push_back(embeddedMD6Header);
            }

            if (_FileType == 67)
            {
                EmbeddedLWOHeader embeddedLWOHeader = resourceFile.ReadLWOHeader(decompressedHeader);
                _LWOHeaderData.push_back(embeddedLWOHeader);
            }
        }
        fclose(f);
        return;
    }
    void FileExportList::GetStreamDBIndexAndSize()
    {
        for (int i = 0; i < _ExportItems.size(); i++)
        {
            FileExportItem& thisFile = _ExportItems[i];
            int numMips = -6;

            // tga data
            if (_FileType == 21)
            {
                numMips = _TGAHeaderData[i].numMips;
                thisFile.streamDBSizeDecompressed = _TGAHeaderData[i].decompressedSize;
                thisFile.streamDBSizeCompressed = _TGAHeaderData[i].compressedSize;
                thisFile.tgaPixelHeight = _TGAHeaderData[i].pixelHeight;
                thisFile.tgaPixelWidth = _TGAHeaderData[i].pixelWidth;

                // FIXME, should get compressionType from .resources
                if (_TGAHeaderData[i].isCompressed == 1)
                    thisFile.streamDBCompressionType = 2;
            }
               
            // md6mesh data
            if (_FileType == 31)
            {
                thisFile.streamDBSizeDecompressed = _MD6HeaderData[i].decompressedSize;
                thisFile.streamDBSizeCompressed = _MD6HeaderData[i].compressedSize;
                thisFile.streamDBCompressionType = 2;
            }

            // lwo data
            if (_FileType == 67)
            {
                thisFile.streamDBSizeDecompressed = _LWOHeaderData[i].decompressedSize;
                thisFile.streamDBSizeCompressed = _LWOHeaderData[i].compressedSize;
                thisFile.streamDBCompressionType = 2;
            }

            // Convert resourceID to streamDBIndex
            endianSwap(thisFile.resourceFileHash);
            uint64 streamDBIndex = CalculateStreamDBIndex(thisFile.resourceFileHash, numMips);
            endianSwap(streamDBIndex);
            thisFile.streamDBIndex = streamDBIndex;
        }
        return;
    }
    void FileExportList::GetStreamDBFileOffsets(const std::vector<StreamDBFile>& streamDBFiles)
    {
        for (int i = 0; i < _ExportItems.size(); i++)
        {
            // Searches every .streamdb file for a matching streamDBIndex
            FileExportItem& thisFile = _ExportItems[i];
            for (int j = 0; j < streamDBFiles.size(); j++)
            {
                uint64 fileOffset = streamDBFiles[j].GetFileOffsetInStreamDB(thisFile.streamDBIndex, thisFile.streamDBSizeCompressed);
                
                // index matched, size too small, increment index by 1 and check again.
                if (fileOffset == -2)
                    fileOffset = streamDBFiles[j].GetFileOffsetInStreamDB(thisFile.streamDBIndex + 1, thisFile.streamDBSizeCompressed);
                
                if (fileOffset == -1)
                    continue;

                thisFile.streamDBNumber = j;
                thisFile.streamDBFileName = streamDBFiles[j].fileName;
                thisFile.streamDBFileOffset = fileOffset;
                break;
            }

            // match found, go to next file
            if (thisFile.streamDBNumber != -1)
                continue;

            // Second pass for images that aren't found. 
            // This catches some wierd UI files whose streamDBIndex is off by 1.
            thisFile.streamDBIndex--;
            for (int j = 0; j < streamDBFiles.size(); j++)
            {
                uint64 fileOffset = streamDBFiles[j].GetFileOffsetInStreamDB(thisFile.streamDBIndex, thisFile.streamDBSizeCompressed);
                if (fileOffset == -1)
                    continue;

                thisFile.streamDBNumber = j;
                thisFile.streamDBFileName = streamDBFiles[j].fileName;
                thisFile.streamDBFileOffset = fileOffset;
                break;
            }
        }
        return;
    }

    // Constructs FileExportList
    FileExportList::FileExportList(const ResourceFile& resourceFile, const std::vector<StreamDBFile>& streamDBFiles, int fileType)
    {
        _FileType = fileType;
        _ResourceFileName = resourceFile.filename;
        GetResourceEntries(resourceFile);
        ParseEmbeddedFileHeaders(resourceFile);
        GetStreamDBIndexAndSize();
        GetStreamDBFileOffsets(streamDBFiles);
        return;
    }

    // FileExporter - Subroutines
    std::vector<byte> FileExporter::GetBinaryFileFromStreamDB(const FileExportItem& fileExportInfo, const StreamDBFile& streamDBFile)
    {
        std::ifstream f;
        f.open(fileExportInfo.streamDBFileName, std::ios::in | std::ios::binary);
        auto fileData = streamDBFile.GetEmbeddedFile(f, fileExportInfo.streamDBFileOffset, fileExportInfo.streamDBSizeCompressed);
        f.close();

        return fileData;
    }
    std::vector<byte> FileExporter::ConstructDDSFileHeader(uint32 width, uint32 height, uint32 decompressedSize)
    {
        DDSHeader defaultDDS;
        auto ptr = reinterpret_cast<byte*>(&defaultDDS);
        auto buffer = std::vector<byte>(ptr, ptr + sizeof(defaultDDS));

        auto bytes = intToByteArray(width);
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + 12);

        bytes = intToByteArray(height);
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + 16);

        bytes = intToByteArray(decompressedSize);
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + 20);

        return buffer;
    }
    fs::path FileExporter::BuildOutputPath(const std::string filePath)
    {
        fs::path outputPath = _OutDir / fs::path(filePath);
        outputPath.make_preferred();
        return outputPath;
    }

    // FileExporter - Debug Functions
    void FileExporter::PrintMatchesToCSV(std::vector<FileExportItem>& fileExportList) const
    {
        std::ofstream outputMatched("matched.csv", std::ios::app);
        for (int i = 0; i < fileExportList.size(); i++)
        {
            FileExportItem& thisEntry = fileExportList[i];
            if (thisEntry.streamDBNumber == -1)
                continue;

            std::string output;
            output += "\"" + thisEntry.resourceFileName + "\",";
            output += std::to_string(thisEntry.streamDBIndex) + ",";
            output += std::to_string(thisEntry.streamDBFileOffset) + ",";
            output += std::to_string(thisEntry.streamDBSizeCompressed) + ",";
            output += std::to_string(thisEntry.streamDBSizeDecompressed) + ",";
            output += std::to_string(thisEntry.streamDBCompressionType) + ",";
            output += std::to_string(thisEntry.streamDBNumber) + ",";
            output += "\"" + thisEntry.streamDBFileName + "\"" + "\n";
            outputMatched.write(output.c_str(), output.length());
        }
        return;
    }
    void FileExporter::PrintUnmatchedToCSV(std::vector<FileExportItem>& fileExportList) const
    {
        std::ofstream outputUnmatched("unmatched.csv", std::ios::app);
        for (int i = 0; i < fileExportList.size(); i++)
        {
            FileExportItem& thisEntry = fileExportList[i];
            if (thisEntry.streamDBNumber != -1)
                continue;

            std::string output;
            output += "\"" + thisEntry.resourceFileName + "\",";
            output += std::to_string(thisEntry.streamDBIndex) + ",";
            output += std::to_string(thisEntry.streamDBFileOffset) + ",";
            output += std::to_string(thisEntry.streamDBSizeCompressed) + ",";
            output += std::to_string(thisEntry.streamDBSizeDecompressed) + ",";
            output += std::to_string(thisEntry.streamDBCompressionType) + ",";
            output += std::to_string(thisEntry.streamDBNumber) + "\n";
            outputUnmatched.write(output.c_str(), output.length());
        }
        return;
    }

    // FileExporter - Export Functions
    void FileExporter::ExportTGAFiles(const std::vector<StreamDBFile>& streamDBFiles)
    {
        int notFound = 0;
        int errorCount = 0;
        
        std::vector<FileExportItem> tgaFilesToExport = _TGAExportList.GetFileExportItems();
        printf("Now exporting %llu TGA files.\n", tgaFilesToExport.size());
        
        for (int i = 0; i < tgaFilesToExport.size(); i++)
        {
            // Don't try to export resources that weren't found
            if (tgaFilesToExport[i].streamDBNumber == -1)
            {
                // printf("File not found, skipping: %s \n", tgaFilesToExport[i].resourceFileName.c_str());
                notFound++;
                continue;
            }

            FileExportItem& thisFile = tgaFilesToExport[i];
            const StreamDBFile& thisStreamDBFile = streamDBFiles[thisFile.streamDBNumber];

            // get binary file data from streamdb
            std::vector<byte> fileData = GetBinaryFileFromStreamDB(thisFile, thisStreamDBFile);

            // check if decompression is necessary
            if (thisFile.streamDBSizeCompressed != thisFile.streamDBSizeDecompressed)
            {
                fileData = oodleDecompress(fileData, thisFile.streamDBSizeDecompressed);
                if (fileData.empty())
                {
                    printf("Failed to decompress: %s \n", thisFile.resourceFileName.c_str());
                    errorCount++;
                    continue;
                }
            }

            // construct DDS file header
            std::vector<byte> ddsFileHeader = ConstructDDSFileHeader(thisFile.tgaPixelWidth, thisFile.tgaPixelHeight, thisFile.streamDBSizeDecompressed);

            // parse filepath and create folders if necessary
            fs::path fullPath = BuildOutputPath(thisFile.resourceFileName);
            fs::path folderPath = fullPath;
            folderPath.remove_filename();

            if (!fs::exists(folderPath))
                if (!fs::create_directories(folderPath)) {}

            FILE* outFile = fopen(fullPath.string().c_str(), "wb");
            if (outFile == NULL)
            {
                printf("Failed to open file for writing: %s \n", fullPath.string().c_str());
                errorCount++;
                continue;
            }

            fwrite(ddsFileHeader.data(), ddsFileHeader.size(), 1, outFile);
            fwrite(fileData.data(), fileData.size(), 1, outFile);
            fclose(outFile);
        }
        printf("Wrote %llu TGA files, %d not found, %d errors. \n", tgaFilesToExport.size(), notFound, errorCount);
        return;
    }
    void FileExporter::ExportMD6Files(const std::vector<StreamDBFile>& streamDBFiles)
    {
        int notFound = 0;
        int errorCount = 0;

        std::vector<FileExportItem> md6FilesToExport = _MD6ExportList.GetFileExportItems();
        printf("Now exporting %llu MD6 files.\n", md6FilesToExport.size());
        
        for (int i = 0; i < md6FilesToExport.size(); i++)
        {
            // Don't try to export resources that weren't found
            if (md6FilesToExport[i].streamDBNumber == -1)
            {
                // printf("File not found, skipping: %s \n", md6FilesToExport[i].resourceFileName.c_str());
                notFound++;
                continue;
            }

            FileExportItem& thisFile = md6FilesToExport[i];
            const StreamDBFile& thisStreamDBFile = streamDBFiles[thisFile.streamDBNumber];

            // get binary file data from streamdb
            std::vector<byte> fileData = GetBinaryFileFromStreamDB(thisFile, thisStreamDBFile);

            // check if decompression is necessary
            if (thisFile.streamDBSizeCompressed != thisFile.streamDBSizeDecompressed)
            {
                fileData = oodleDecompress(fileData, thisFile.streamDBSizeDecompressed);
                if (fileData.empty())
                {
                    printf("Failed to decompress: %s \n", thisFile.resourceFileName.c_str());
                    errorCount++;
                    continue;
                }
            }

            // parse filepath and create folders if necessary
            fs::path fullPath = BuildOutputPath(thisFile.resourceFileName);
            fs::path folderPath = fullPath;
            folderPath.remove_filename();

            if (!fs::exists(folderPath))
                if (!fs::create_directories(folderPath)) {}

            FILE* outFile = fopen(fullPath.string().c_str(), "wb");
            if (outFile == NULL)
            {
                printf("Failed to open file for writing: %s \n", fullPath.string().c_str());
                errorCount++;
                continue;
            }

            fwrite(fileData.data(), fileData.size(), 1, outFile);
            fclose(outFile);
        }
        printf("Wrote %llu MD6 files, %d not found, %d errors. \n", md6FilesToExport.size(), notFound, errorCount);
        return;
    }
    void FileExporter::ExportLWOFiles(const std::vector<StreamDBFile>& streamDBFiles)
    {
        int notFound = 0;
        int errorCount = 0;

        std::vector<FileExportItem> lwoFilesToExport = _LWOExportList.GetFileExportItems();
        printf("Now exporting %llu LWO files.\n", lwoFilesToExport.size());

        for (int i = 0; i < lwoFilesToExport.size(); i++)
        {
            // Don't try to export resources that weren't found
            if (lwoFilesToExport[i].streamDBNumber == -1)
            {
                // printf("File not found, skipping: %s \n", lwoFilesToExport[i].resourceFileName.c_str());
                notFound++;
                continue;
            }

            FileExportItem& thisFile = lwoFilesToExport[i];
            const StreamDBFile& thisStreamDBFile = streamDBFiles[thisFile.streamDBNumber];

            // get binary file data from streamdb
            std::vector<byte> fileData = GetBinaryFileFromStreamDB(thisFile, thisStreamDBFile);

            // check if decompression is necessary
            if (thisFile.streamDBSizeCompressed != thisFile.streamDBSizeDecompressed)
            {
                fileData = oodleDecompress(fileData, thisFile.streamDBSizeDecompressed);
                if (fileData.empty())
                {
                    printf("Failed to decompress: %s \n", thisFile.resourceFileName.c_str());
                    errorCount++;
                    continue;
                }
            }

            // parse filepath and create folders if necessary
            fs::path fullPath = BuildOutputPath(thisFile.resourceFileName);
            fs::path folderPath = fullPath;
            folderPath.remove_filename();

            if (!fs::exists(folderPath))
                if (!fs::create_directories(folderPath)) {}

            FILE* outFile = fopen(fullPath.string().c_str(), "wb");
            if (outFile == NULL)
            {
                printf("Failed to open file for writing: %s \n", fullPath.string().c_str());
                errorCount++;
                continue;
            }

            fwrite(fileData.data(), fileData.size(), 1, outFile);
            fclose(outFile);
        }
        printf("Wrote %llu LWO files, %d not found, %d errors. \n", lwoFilesToExport.size(), notFound, errorCount);
        return;
    }

    // Constructs FileExporter, collects data needed for export.
    void FileExporter::Init(const ResourceFile& resourceFile, const std::vector<StreamDBFile>& streamDBFiles, std::string outputDirectory)
    {
        _OutDir = outputDirectory;
        _TGAExportList = FileExportList(resourceFile, streamDBFiles, 21);
        _MD6ExportList = FileExportList(resourceFile, streamDBFiles, 31);
        _LWOExportList = FileExportList(resourceFile, streamDBFiles, 67);
        std::vector<FileExportItem> tgaFilesToExport = _TGAExportList.GetFileExportItems();
        std::vector<FileExportItem> md6FilesToExport = _MD6ExportList.GetFileExportItems();
        std::vector<FileExportItem> lwoFilesToExport = _LWOExportList.GetFileExportItems();

        // debug tga export
        PrintMatchesToCSV(tgaFilesToExport);
        PrintUnmatchedToCSV(tgaFilesToExport);

        // debug md6 export
        PrintMatchesToCSV(md6FilesToExport);
        PrintUnmatchedToCSV(md6FilesToExport);

        // debug lwo export
        PrintMatchesToCSV(lwoFilesToExport);
        PrintUnmatchedToCSV(lwoFilesToExport);

        // get total size of files to export
        size_t totalExportSize = 0;

        for (const auto& fileExport : _TGAExportList.GetFileExportItems())
            totalExportSize += fileExport.streamDBSizeDecompressed;

        for (const auto& fileExport : _MD6ExportList.GetFileExportItems())
            totalExportSize += fileExport.streamDBSizeDecompressed;

        for (const auto& fileExport : _LWOExportList.GetFileExportItems())
            totalExportSize += fileExport.streamDBSizeDecompressed;

        if (fs::space(fs::current_path()).available < totalExportSize)
        {
            fprintf(stderr, "Error: Not enough space in disk.\n");
            fprintf(stderr, "Exporting from this file requires at least %.2lf MB of free space.\n", (double)totalExportSize / (1024 * 1024));
            exit(1);
        }

        return;
    }
}
