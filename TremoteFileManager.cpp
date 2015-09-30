
#include "TRemoteFileManager.h"
#include "remote_filemanager.h"
#include "filesystem/TFileSystem.h"

#include <string.h>

TRemoteFileManager::TRemoteFileManager()
{
    m_Buffer.reserve(1024 * 1024 * 4); // 4 MB
    onClientConnected = [this](const TSocket::SharedPtr& sock) {
        sock->userData = std::make_shared<TUser>();
        sock->setReading(true);
        sock->onReadReady = [this](TStream& stream) {
            return onDataAvailable(dynamic_cast<TSocket*>(&stream));
        };
    };
}

int TRemoteFileManager::onDataAvailable(TSocket* sock)
{
    // TRACE("Read data from " << sock->descriptor());
    int bytesRead = sock->read(m_Buffer);
    // TRACE("Data: " << std::string(m_Buffer.begin(), m_Buffer.end()));
    if (bytesRead < 0)
    {
        ERROR(strerror(errno));
        return bytesRead;
    }

    if (bytesRead == 0)
    {
        sock->closeLater();
        return bytesRead;
    }

    auto header = TPacketHeader::mapToBuffer(m_Buffer);
    if (!TUser::fromSock(*sock)->loggedIn && header->command != TCommands::LOGIN && header->command != TCommands::REGISTER)
    {
        TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_FAIL, std::string("Log in please"));
        sock->write(m_Buffer);
        return bytesRead;
    }

    switch (header->command)
    {
    case TCommands::LOGIN:
        logIn(*sock);
        break;
    case TCommands::REGISTER:
        signUp(*sock);
        break;
    case TCommands::LIST_FILES:
        commandListFiles(*sock);
        break;
    case TCommands::CREATE_DIR:
        createDirectory(*sock);
        break;
    case TCommands::DELETE_DIR:
        removeDirectory(*sock);
        break;
    case TCommands::DOWNLOAD_FILE:
        commandDownloadFile(*sock);
        break;
    case TCommands::UPLOAD_FILE:
        commandUploadFile(*sock);
        break;
    case TCommands::GET_CURRENT_DIR:
        getCurrentDir(*sock);
        break;
    default:
        commandUnknown(*sock);
        break;
    }

    return bytesRead; //! @todo
}

bool TRemoteFileManager::logIn(TSocket &sock)
{
    TRACE("Login ");
    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    auto username = hdr->fileop.name1();
    auto password = hdr->fileop.name2();
    std::string dir = m_BaseDir + '/' + username;
    if (TFileSystem::exists(dir) && TFile(dir+"/pass").readStr() == password)
    {
        TUser* user = TUser::fromSock(sock);
        user->loggedIn = true;
        user->name = username;
        user->currentDir = username;
        TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_OK, std::string("Logged in"));

        sock.setWriting(true);
        std::vector<uint8_t> buff(m_Buffer.begin(), m_Buffer.end());
        sock.onWriteReady = [buff](TStream& stream) {
            stream.write(buff);
            stream.setWriting(false);
            return 1;
        };
        return true;
    }

    TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_FAIL, std::string("Incorrect user name or password"));
    sock.write(m_Buffer);
    return false;
}

bool TRemoteFileManager::signUp(TSocket &sock)
{
    TRACE("signUp");
    auto result = TCommandResult::RES_OK;
    std::string resultDescription = "Ok";
    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    auto username = hdr->fileop.name1();
    auto password = hdr->fileop.name2();

    std::string dir = m_BaseDir + '/' + username;

    if (TFileSystem::exists(dir))
    {
        result = TCommandResult::RES_FAIL;
        resultDescription = "User already exists";
    }
    else if (!TFileSystem::mkdir(m_BaseDir + '/' + username))
    {
        result = TCommandResult::RES_FAIL;
        resultDescription = "Can not create directory for new user: " + std::string(strerror(errno));
    }
    else if (TFile(dir+"/pass", O_WRONLY | O_CREAT | O_TRUNC).write(password) < 0)
    {
        result = TCommandResult::RES_FAIL;
        resultDescription = "Can not save password for new user: " + std::string(strerror(errno));
    }

    TPacketHeader::createResponse(m_Buffer, result, resultDescription);
    sock.write(m_Buffer);

    if (result == TCommandResult::RES_OK)
    {
        TUser* user = TUser::fromSock(sock);
        user->loggedIn = true;
        user->name = username;
        user->currentDir = username;
    }

    return result == TCommandResult::RES_OK;
}

void TRemoteFileManager::getCurrentDir(TSocket &sock)
{
    TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_OK, TUser::fromSock(sock)->currentDir);
    sock.write(m_Buffer);
}

void TRemoteFileManager::createDirectory(TSocket &sock)
{
    auto result = TCommandResult::RES_OK;
    std::string resultDescription = "Created";
    std::string dir = TPacketHeader::mapToBuffer(m_Buffer)->fileop.name1();
    if (!TFileSystem::mkdir(userDir(*TUser::fromSock(sock)) + '/' + dir))
    {
        result = TCommandResult::RES_FAIL;
        resultDescription = strerror(errno);
    }

    TPacketHeader::createResponse(m_Buffer, result, resultDescription);
    sock.write(m_Buffer);
}

void TRemoteFileManager::removeDirectory(TSocket &sock)
{
    auto result = TCommandResult::RES_OK;
    std::string resultDescription = "Removed";
    std::string dir = TPacketHeader::mapToBuffer(m_Buffer)->fileop.name1();
    if (!TFileSystem::removeDir(userDir(*TUser::fromSock(sock)) + '/' + dir))
    {
        result = TCommandResult::RES_FAIL;
        resultDescription = strerror(errno);
    }

    TPacketHeader::createResponse(m_Buffer, result, resultDescription);
    sock.write(m_Buffer);
}

void TRemoteFileManager::commandDownloadFile(TSocket &sock)
{
    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    std::string fileName = hdr->fileop.name1();
    INFO("Download request for " << fileName);
    fileName = userDir(*TUser::fromSock(sock)) + '/' + fileName;

    auto file = std::make_shared<TFile>();
    if (!file->open(fileName))
    {
        TRACE(file->lastError());
        TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_FAIL, file->lastError());
        sock.write(m_Buffer);
        return;
    }

    sock.setWriting(true);
    sock.onWriteReady = [this, file](TStream& sock) {
        int bytesRead = file->read(m_Buffer);
        if (bytesRead < 0)
        {
            ERROR(strerror(errno));
            return 1;
        }

        if (bytesRead == 0)
        {
            sock.setWriting(false);
            sock.onWriteReady = [](TStream&){ return 1; };
            return 1;
        }
        /* int bytesWritten =*/ sock.write(m_Buffer);

        // TRACE("Bytes read: " << bytesRead << "\tBytes written: " << byresWritten);
        return 1;
    };

    if (file->isOk())
    {
        auto user = TUser::fromSock(sock);
        m_Buffer.resize(sizeof(TPacketHeader));
        auto pckt = TPacketHeader::mapToBuffer(m_Buffer);
        pckt->result = TCommandResult::RES_OK;
        pckt->dataSize = file->size();
        user->req.fileSize = file->size();
        user->req.readSize = 0;
        sock.write(m_Buffer);
    }
}

void TRemoteFileManager::commandListFiles(TSocket &sock)
{
    auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
    std::string dirName = hdr->fileop.name1();
    INFO("List files in " << dirName);

    std::list<std::string> files;
    TFileSystem::listDir(userDir(*TUser::fromSock(sock)) , files);

    for (auto filename : files)
    {
        std::string f = filename + "\n";
        // TRACE(filename);
        sock.write(std::vector<uint8_t>(f.begin(), f.end()));
    }
    std::string filesEnd = "\n";
    sock.write(std::vector<uint8_t>(filesEnd.begin(), filesEnd.end()));
}

void TRemoteFileManager::commandUploadFile(TSocket &sock)
{
        auto hdr = TPacketHeader::mapToBuffer(m_Buffer);
        std::string fileName = hdr->fileop.name1();
        INFO("Upload request for " << fileName);

        fileName = userDir(*TUser::fromSock(sock)) + '/' + fileName;
        auto file = std::make_shared<TFile>();
        if (file->open(fileName, O_WRONLY | O_CREAT))
        {
            TRACE("Saving to " << fileName);
            TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_OK, std::string(""));
            sock.onReadReady = [this, file](TStream& sock) {
                sock.read(m_Buffer);
                TUser* user = TUser::fromSock(sock);
                user->req.fileSize = TPacketHeader::mapToBuffer(m_Buffer)->dataSize;;
                user->req.readSize = 0;
                sock.onReadReady = [this, file](TStream& sock) {
                    TUser* user = TUser::fromSock(sock);
                    size_t chunkSize = 1024 * 20;

                    if (user->req.fileSize - user->req.readSize < chunkSize)
                        chunkSize = user->req.fileSize - user->req.readSize;

                    auto bytesRead = sock.read(m_Buffer, chunkSize);
                    user->req.readSize += bytesRead;
                    // TRACE("Read " << user->req.readSize << " / " << user->req.fileSize);
                    file->write(m_Buffer);
                    if (bytesRead == 0 || user->req.readSize >= user->req.fileSize)
                    {
                        file->closeLater();
                        sock.onReadReady = [this](TStream &sock){
                            onDataAvailable(dynamic_cast<TSocket*>(&sock));
                            return 1;
                        };
                        TRACE("File uploaded");
                        return 1;
                    }
                    // TRACE("Read " << bytesRead);
                    return 1;
                };
                return 1;
            };
        }
        else
            TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_FAIL, file->lastError());

        sock.write(m_Buffer);
}

void TRemoteFileManager::commandUnknown(TSocket &sock)
{
    ERROR("Unknown command");
    TPacketHeader::createResponse(m_Buffer, TCommandResult::RES_FAIL, std::string("Unknown command"));
    sock.write(m_Buffer);
}

std::string TRemoteFileManager::userDir(const TUser &user)
{
    return m_BaseDir + '/' + user.currentDir;
}

