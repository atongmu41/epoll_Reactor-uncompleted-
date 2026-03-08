#include "TcpServer.h"

int main(int argc, char* argv[]) {
  int port = 5005;
  if (argc >= 2) {
    port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
      std::cerr << "invalid port: " << argv[1] << "\n";
      return -1;
    }
  }

  TcpServer server(static_cast<uint16_t>(port));
  if (!server.start()) return -1;
  server.loop();
  return 0;
}
