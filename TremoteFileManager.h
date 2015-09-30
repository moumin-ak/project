#ifndef TREMOTEFILEMANAGER_H
#define TREMOTEFILEMANAGER_H

#include "net/TServer.h"

struct TRequest
{
    size_t fileSize;
    size_t readSize;
};

class TUser
{
public:
    std::string name = "guest";
    std::string password = "guest";
    std::string currentDir = "guest";
    bool loggedIn = false;

    TRequest req { };

    static TUser* fromSock(const TStream& sock)
    {
        return static_cast<TUser*>(sock.userData.get());
    }

    //~TUser() { TRACE("User " << name << " deleted"); }
};

class TRemoteFileManager: public TServer
{
public:
    TRemoteFileManager();

    int onDataAvailable(TSocket *sock);
    bool logIn(TSocket& sock);
    bool signUp(TSocket& sock);
    void getCurrentDir(TSocket& sock);
    void createDirectory(TSocket& sock);
    void removeDirectory(TSocket& sock);
    void commandDownloadFile(TSocket& sock);
    void commandListFiles(TSocket& sock);
    void commandUploadFile(TSocket& sock);

    void commandUnknown(TSocket& sock);

private:
    std::vector<uint8_t> m_Buffer { };
    std::string m_BaseDir = "/tmp";

    std::string userDir(const TUser& user);
};

#endif // TREMOTEFILEMANAGER_H
