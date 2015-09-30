
#include "log.h"
#include "TClient.h"
#include "filesystem/TFile.h"
#include "remote_filemanager.h"
#include "filesystem/TFileSystem.h"

#include <string.h>

TClient::TClient()
{

}

bool TClient::connect(const HostAddress &address, int port)
{
    if (!m_Connection.connect(address, port))
    {
        ERROR("Could not connect to " << address);
        return false;
    }
    HostAddress addr(m_Connection.descriptor());
    TRACE("Connected to: " << addr);
    return true;
}

void TClient::processCommands()
{
    WARNING("Available commands:");
    for (const auto& command : m_Commands)
        INFO(command.first);

    while (!m_Stop)
    {
        std::string command = readString(currentDir());
        auto commandFunc = m_Commands.find(command);
        // TRACE("Command = " << command);
        if (commandFunc != m_Commands.end())
            commandFunc->second();
        else
            ERROR("Unknown command");
    }

    INFO("Bye...");
}

bool TClient::login()
{
    auto username = readString("User name");
    auto password = readString("Password");

    TPacketHeader::createFileop(m_Buffer, TCommands::LOGIN, username, password);
    m_Connection.write(m_Buffer);
    /* auto bytesRead =*/ m_Connection.read(m_Buffer);
    if (TPacketHeader::mapToBuffer(m_Buffer)->result == TCommandResult::RES_OK)
    {
        INFO("Logged in");
        return true;
    }
    else
    {
        auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
        ERROR(std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize));
        return false;
    }
}

bool TClient::signUp()
{
    auto username = readString("User name");
    auto password = readString("Password");

    TPacketHeader::createFileop(m_Buffer, TCommands::REGISTER, username, password);
    m_Connection.write(m_Buffer);
    /* auto bytesRead =*/ m_Connection.read(m_Buffer);
    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    if (TPacketHeader::mapToBuffer(m_Buffer)->result == TCommandResult::RES_OK)
    {
        INFO(std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize));
        return true;
    }
    else
    {
        ERROR(std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize));
        return false;
    }
}

bool TClient::uploadFile()
{
    std::string fileName = readString("Enter file to upload");
    TFile file;
    if (!file.open(fileName))
    {
        ERROR(file.lastError());
        return false;
    }

    std::string serverFilename = TFileSystem::fileName(fileName);
    TPacketHeader::createFileop(m_Buffer, TCommands::UPLOAD_FILE, serverFilename);
    m_Connection.write(m_Buffer);
    /*auto bytesRead =*/ m_Connection.read(m_Buffer); // TODO: check this
    if (TPacketHeader::mapToBuffer(m_Buffer)->result == TCommandResult::RES_OK)
    {
        m_Buffer.resize(sizeof(TPacketHeader));
        TPacketHeader::mapToBuffer(m_Buffer)->dataSize = file.size();
        m_Connection.write(m_Buffer);
        while (file.read(m_Buffer))
            m_Connection.write(m_Buffer);
    }
    else
    {
        auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
        ERROR(std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize));
        return false;
    }

    return true;
}

bool TClient::downloadFile()
{
    std::string fileName = readString("Enter file to download");
    TPacketHeader::createFileop(m_Buffer, TCommands::DOWNLOAD_FILE, fileName);
    /* int bytesWritten =*/ m_Connection.write(m_Buffer);
    WARNING("Download file " << fileName);

    auto bytesRead = m_Connection.read(m_Buffer);
    if (bytesRead < 0)
    {
        ERROR(strerror(errno));
        return false;
    }
    if (TPacketHeader::mapToBuffer(m_Buffer)->result == TCommandResult::RES_OK)
    {
        uint64_t fileSize = TPacketHeader::mapToBuffer(m_Buffer)->dataSize;
        uint64_t readSize = 0;
        TRACE("Downloading file. Size = " << fileSize);

        TFile file;
        file.open(fileName, O_WRONLY | O_CREAT);
        if (static_cast<size_t>(bytesRead) > sizeof(TPacketHeader))
        {
            file.write(std::vector<uint8_t>(m_Buffer.begin() + sizeof(TPacketHeader), m_Buffer.end()));
            bytesRead = bytesRead - sizeof(TPacketHeader);
            readSize += bytesRead;
        }
        size_t chunkSize = 1024 * 20;
        while (readSize < fileSize)
        {
            // TRACE("Reading");
            bytesRead = m_Connection.read(m_Buffer, chunkSize);
            readSize += bytesRead;
            // TRACE("Read " << readSize << " / " << fileSize);
            file.write(m_Buffer);
        }
        TRACE("Read size " << readSize);
    }
    else
    {
        auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
        ERROR(std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize));
        return false;
    }

    return true;
}

bool TClient::listFiles()
{
//    WARNING("List files in " << dirName);
    TPacketHeader::createFileop(m_Buffer, TCommands::LIST_FILES, "./");
    /*int bytesWritten =*/ m_Connection.write(m_Buffer);
    std::string filenames;
    uint8_t prevByte = 'a';
    do
    {
        /*int bytesRead =*/ m_Connection.read(m_Buffer);
        filenames.assign(m_Buffer.begin(), m_Buffer.end());
        if (m_Buffer.size() < 2 && m_Buffer.back() == '\n' && prevByte == '\n')
            break;
        prevByte = m_Buffer.back();
        std::cout << filenames;
    } while (!(m_Buffer.back() == '\n' && *(m_Buffer.end()-2) == '\n'));
    INFO(std::endl << "done");
    return true;
}

bool TClient::createDir()
{
    TPacketHeader::createFileop(m_Buffer, TCommands::CREATE_DIR, readString("Enter directory name"));
    /*int bytesWritten =*/ m_Connection.write(m_Buffer);
    /*int bytesRead =*/ m_Connection.read(m_Buffer);

    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    auto resultDescription = std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize);
    if (hdr->result == TCommandResult::RES_OK)
        INFO(resultDescription);
    else
        ERROR(resultDescription);
    return hdr->result == TCommandResult::RES_OK;
}

bool TClient::remove()
{
    TPacketHeader::createFileop(m_Buffer, TCommands::DELETE_DIR, readString("Enter file name"));
    /*int bytesWritten =*/ m_Connection.write(m_Buffer);
    /*int bytesRead =*/ m_Connection.read(m_Buffer);

    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    auto resultDescription = std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize);
    if (hdr->result == TCommandResult::RES_OK)
        INFO(resultDescription);
    else
        ERROR(resultDescription);
    return hdr->result == TCommandResult::RES_OK;
}

std::string TClient::currentDir()
{
    TPacketHeader::createFileop(m_Buffer, TCommands::GET_CURRENT_DIR);
    /*int bytesWritten =*/ m_Connection.write(m_Buffer);
    /*int bytesRead =*/ m_Connection.read(m_Buffer);

    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    return std::string(reinterpret_cast<char*>(hdr->data), hdr->dataSize);
}

std::string TClient::readString(const std::string &message)
{
    if (!message.empty())
        std::cout << message << " -> ";

    std::string command;
    std::cin >> command;
    return command;
}

