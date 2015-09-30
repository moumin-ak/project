#ifndef REMOTE_FILEMANAGER_H
#define REMOTE_FILEMANAGER_H

#include <map>
#include <vector>
#include <string>
#include <cstdint>

enum class TCommands
{
    LOGIN,
    REGISTER,
    GET_CURRENT_DIR,
    CREATE_DIR,
    DELETE_DIR,
    COPY_DIR,
    LIST_FILES,
    UPLOAD_FILE,
    DOWNLOAD_FILE,

    UNKNOWN
};

//std::map<TCommands, std::string> CommandStr = {
//    { TCommands::LIST_FILES   , "LIST_FILES"    },
//    { TCommands::UPLOAD_FILE  , "UPLOAD_FILE"   },
//    { TCommands::DOWNLOAD_FILE, "DOWNLOAD_FILE" }
//};

enum class TCommandResult
{
    RES_OK,
    RES_FAIL
};

struct TPacketHeader
{
    TCommands command;
    TCommandResult result;
    uint64_t dataSize;
    union
    {
        uint8_t data[];
        struct
        {
            uint32_t name1Len;
            uint32_t name2Len;
            char name[];

            inline std::string name1()
            {
                return std::string(name, name1Len);
            }
            inline std::string name2()
            {
                return std::string(&name[name1Len], name2Len);
            }
        } fileop;
    };

    template <typename BufferContainerType>
    static inline TPacketHeader* mapToBuffer(BufferContainerType& buff)
    {
        return reinterpret_cast<TPacketHeader*>(buff.data());
    }

    template <typename BufferContainertype, typename DataContainerType>
    static inline TPacketHeader* createResponse(BufferContainertype& buff, TCommandResult result, const DataContainerType& data)
    {
        buff.resize(sizeof(TPacketHeader) + data.size());
        TPacketHeader* pckt = mapToBuffer(buff);
        pckt->result = result;
        pckt->dataSize = data.size();
        std::copy(std::begin(data), std::end(data), pckt->data);
        return mapToBuffer(buff);
    }

    template <typename BufferContainerType>
    static inline TPacketHeader* createFileop(BufferContainerType& buffer, TCommands command, const std::string& fileName1 = "", const std::string& fileName2 = "")
    {
        buffer.resize(sizeof(TPacketHeader) + fileName1.size() + fileName2.size());
        TPacketHeader *hdr = TPacketHeader::mapToBuffer(buffer);
        hdr->command = command;
        hdr->dataSize = buffer.size() - sizeof(TPacketHeader); // FIXME: this is incorrect
        hdr->fileop.name1Len = fileName1.size();
        hdr->fileop.name2Len = fileName2.size();
        std::copy(std::begin(fileName1), std::end(fileName1), hdr->fileop.name);
        std::copy(std::begin(fileName2), std::end(fileName2), &hdr->fileop.name[hdr->fileop.name1Len]);
        return mapToBuffer(buffer);
    }
};


#endif // REMOTE_FILEMANAGER_H

