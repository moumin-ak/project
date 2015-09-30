
#include "TClient.h"

int main()
{
    TClient client;
    client.connect(HostAddress("127.0.0.1", 12345), 12345);
    client.processCommands();

    return 0;
}



