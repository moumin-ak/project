#ifndef TCLIENT_H
#define TCLIENT_H

#include "net/TCPSocket.h"
#include "remote_filemanager.h"

#include <map>
#include <memory>
#include <algorithm>

class TClient
{
public:
    TClient();
    bool connect(const HostAddress& address, int port);
    void processCommands();

    bool login();
    bool signUp();
    bool uploadFile();
    bool downloadFile();
    bool listFiles();
    bool createDir();
    bool remove();

    std::string currentDir();

private:
    TCPSocket m_Connection { };
    std::vector<uint8_t> m_Buffer { };

    bool m_Stop { false };

    std::map<std::string, std::function<void(void)>> m_Commands {
        { "login"   , [this](){ login()       ; } },
        { "register", [this](){ signUp()      ; } },
        { "ls"      , [this](){ listFiles()   ; } },
        { "mkdir"   , [this](){ createDir()   ; } },
        { "rm"      , [this](){ remove()      ; } },
        { "upload"  , [this](){ uploadFile()  ; } },
        { "download", [this](){ downloadFile(); } },
        { "quit"    , [this](){ m_Stop = true ; } }
    };

    std::string readString(const std::string& message = "");
};

#endif // TCLIENT_H
