#if defined(__linux) || defined(__linux__)
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Uso correto: " << argv[0] << " <ip> <porta>" << std::endl;
    return -1;
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    std::cerr << "Não foi possivel criar a socket" << std::endl;
  }

  struct sockaddr_in sv_addr;
  memset(&sv_addr, 0, sizeof(sv_addr));
  if (inet_aton(ip, &sv_addr.sin_addr) == 0) {
    std::cerr << "IP inválido!" << std::endl;
    close(sockfd);
    return -1;
  }
  sv_addr.sin_port = htons(port);
  sv_addr.sin_family = AF_INET;

  if (connect(sockfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) == -1) {
    std::cerr << "Erro ao conectar!" << std::endl;
    close(sockfd);
    return -1;
  }

  char buffer[1024];
  std::string buf;
  std::string resp;

  struct pollfd fds[2] = {{STDIN_FILENO, POLLIN, 0}, {sockfd, POLLIN, 0}};

  while (1) {
    if (poll(fds, 2, -1) < 0) {
      break;
    }

    // Dados vindos do servidor
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
      ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);
      if (n <= 0) {
        std::cout << "\nConexão encerrada pelo servidor." << std::endl;
        break;
      }
      std::cout.write(buffer, n);
      std::cout.flush();
    }

    // Dados vindos do teclado
    if (fds[0].revents & POLLIN) {
      std::string line;
      if (!std::getline(std::cin, line)) {
        break;
      }
      line += '\n';
      send(sockfd, line.data(), line.size(), 0);
    }
  }

  close(sockfd);

  return 0;
}
