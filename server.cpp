#if defined(__linux) || defined(__linux__)
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

struct Vec2D {
  int x, y;
};

enum GameState { GS_NAMING, GS_PLAYING, GS_SHOW_RESULTS, GS_END };

struct Game {
  Vec2D pos;
  GameState state;
  time_t startTime;
  std::string name;
  char input_buffer[50];
  std::set<int> viewed;
};

std::map<std::string, time_t> scoreboard;
std::map<int, std::unique_ptr<Game>> games;
std::vector<struct pollfd> fds;
time_t currentTimestamp = 0;
time_t last_update = 0;

std::vector<char> current_board;
Vec2D current_startpos = {0, 0};
Vec2D current_endpos = {0, 0};
Vec2D current_boardsize = {0, 0};

int random_number(int a, int b) {
  if (a > b)
    std::swap(a, b);

  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dis(a, b);

  return dis(gen);
}

void update_board() {
  // 1. Define um tamanho ímpar (ex: 7x7 até 13x13)
  int w = random_number(3, 6) * 2 + 1;
  int h = random_number(3, 6) * 2 + 1;

  current_boardsize.x = w;
  current_boardsize.y = h;
  int total_cells = w * h;

  // Inicializa tudo com paredes ('#')
  current_board.assign(total_cells, '#');

  auto get_index = [w](int x, int y) { return y * w + x; };

  // As células úteis do labirinto ficam nas coordenadas ímpares (1, 3, 5...)
  std::vector<Vec2D> unvisited_cells;
  for (int y = 1; y < h; y += 2) {
    for (int x = 1; x < w; x += 2) {
      unvisited_cells.push_back({x, y});
    }
  }

  std::vector<bool> in_maze(total_cells, false);

  static std::random_device rd;
  static std::mt19937 gen(rd());

  // 2. Escolhe aleatoriamente a primeira célula e a adiciona ao labirinto
  std::shuffle(unvisited_cells.begin(), unvisited_cells.end(), gen);
  Vec2D first = unvisited_cells.back();
  unvisited_cells.pop_back();

  in_maze[get_index(first.x, first.y)] = true;
  current_board[get_index(first.x, first.y)] = ' ';

  int dx[] = {0, 0, 2, -2};
  int dy[] = {-2, 2, 0, 0};

  // 3. Loop principal de Wilson: continua até que todas as células façam parte
  // do labirinto
  while (!unvisited_cells.empty()) {
    // Pega a primeira célula não visitada disponível para iniciar o Random Walk
    Vec2D start_walk = unvisited_cells.back();

    std::vector<Vec2D> walk;
    walk.push_back(start_walk);

    // Executa a caminhada aleatória até tocar em uma célula que já está no
    // labirinto
    while (!in_maze[get_index(walk.back().x, walk.back().y)]) {
      Vec2D current = walk.back();

      // Embaralha as 4 direções possíveis (passos de 2)
      std::vector<int> dirs = {0, 1, 2, 3};
      std::shuffle(dirs.begin(), dirs.end(), gen);

      Vec2D next = {-1, -1};
      for (int d : dirs) {
        int nx = current.x + dx[d];
        int ny = current.y + dy[d];
        if (nx > 0 && nx < w - 1 && ny > 0 && ny < h - 1) {
          next = {nx, ny};
          break;
        }
      }

      // Se a caminhada esbarrar nela mesma (formando um loop), apaga o loop
      auto found = std::find_if(walk.begin(), walk.end(), [&](const Vec2D &p) {
        return p.x == next.x && p.y == next.y;
      });

      if (found != walk.end()) {
        walk.erase(found + 1, walk.end()); // Apaga o laço gerado
      } else {
        walk.push_back(next);
      }
    }

    // 4. Incorpora o caminho limpo ao labirinto oficial e abre as paredes
    for (size_t i = 0; i < walk.size(); ++i) {
      Vec2D cell = walk[i];
      int idx = get_index(cell.x, cell.y);

      in_maze[idx] = true;
      current_board[idx] = ' ';

      // Remove da lista de não visitadas
      unvisited_cells.erase(
          std::remove_if(
              unvisited_cells.begin(), unvisited_cells.end(),
              [&](const Vec2D &p) { return p.x == cell.x && p.y == cell.y; }),
          unvisited_cells.end());

      // Abre a parede entre a célula atual e a próxima no caminho
      if (i + 1 < walk.size()) {
        Vec2D next_cell = walk[i + 1];
        int wx = (cell.x + next_cell.x) / 2;
        int wy = (cell.y + next_cell.y) / 2;
        current_board[get_index(wx, wy)] = ' ';
      }
    }
  }

  // 5. Define os pontos de Início (S) e Fim (E)
  current_startpos = {1, 1};
  current_endpos = {w - 2, h - 2};

  current_board[get_index(current_startpos.x, current_startpos.y)] = 'S';
  current_board[get_index(current_endpos.x, current_endpos.y)] = 'E';

  std::cout << "Labirinto de Wilson gerado! Tamanho: " << w << "x" << h
            << std::endl;
}

std::string get_ranking_string() {
  std::string output = "RANKING:\n";
  if (scoreboard.empty()) {
    output += "(Nenhum jogador completou este labirinto)\n";
    return output;
  }

  std::vector<std::pair<std::string, time_t>> sorted_scoreboard(
      scoreboard.begin(), scoreboard.end());

  std::sort(
      sorted_scoreboard.begin(), sorted_scoreboard.end(),
      [](const auto &a, const auto &b) { return a.second < b.second; });

  for (const auto &v : sorted_scoreboard) {
    output += v.first + " : " + std::to_string(v.second) + "s\n";
  }

  return output;
}

int main(int argc, char *argv[]) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    std::cout << "Erro ao criar o socket: " << errno << std::endl;
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(12345);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cout << "Erro no bind: " << errno << std::endl;
    close(sockfd);
    return -1;
  }

  if (listen(sockfd, 10) < 0) {
    std::cout << "Erro no listen: " << errno << std::endl;
    close(sockfd);
    return -1;
  }

  struct pollfd svfd = {sockfd, POLLIN | POLLPRI, 0};
  fds.push_back(svfd);

  while (1) {
    // atualizar o relogio do servidor
    time(&currentTimestamp);
    if (currentTimestamp - last_update >= 1 * 60) {
      if (last_update != 0) {
        std::string update_msg =
            "\nO labirinto atualizou!\n" + get_ranking_string();
        for (auto it = fds.begin() + 1; it != fds.end();) {
          int cfd = it->fd;
          if (games.count(cfd) &&
              (games[cfd]->state == GS_PLAYING ||
               games[cfd]->state == GS_SHOW_RESULTS)) {
            send(cfd, update_msg.data(), update_msg.length(), 0);
            close(cfd);
            games.erase(cfd);
            std::cout << "Cliente desconectado na troca de labirinto: " << cfd
                      << std::endl;
            it = fds.erase(it);
          } else {
            ++it;
          }
        }
      }

      update_board();
      last_update = currentTimestamp;
      scoreboard.clear();
    }

    // iterar as sockets
    poll(fds.data(), fds.size(), 10);

    // cria a socket de connecção
    if (fds[0].revents & POLLIN) {
      struct sockaddr_in client_addr;
      memset(&client_addr, 0, sizeof(client_addr));
      socklen_t addr_size = sizeof(client_addr);
      int clientfd =
          accept(fds[0].fd, (struct sockaddr *)&client_addr, &addr_size);
      struct pollfd client_pollfd = {clientfd, POLLIN, 0};
      fds.push_back(client_pollfd);
      std::unique_ptr<Game> newGame = std::make_unique<Game>();
      newGame->state = GS_NAMING;
      newGame->pos = current_startpos;
      newGame->startTime = 0;
      newGame->name = "";
      games[clientfd] = std::move(newGame);

      std::cout << "Novo cliente conectado!\n";
    }

    // gerencia cada jogo individualmente
    for (auto it = fds.begin() + 1; it != fds.end(); it++) {
      int n_recv = 0;
      auto &game = games[it->fd];
      if (it->revents & (POLLIN | POLLPRI)) {
        n_recv =
            recv(it->fd, game->input_buffer, sizeof(game->input_buffer), 0);
      }

      if (n_recv < 0) {
        close(it->fd);
        games.erase(it->fd);
        std::cout << "Cliente desconectado: " << it->fd << std::endl;
        it = fds.erase(it) - 1;
        continue;
      }

      bool send_state = false;
      std::string msg;
      std::string output;
      switch (game->state) {
      case GS_NAMING:
        if (game->startTime == 0 && game->name == "") {
          msg = "Digite o seu nome: ";
          send(it->fd, msg.c_str(), msg.length(), 0);
          game->startTime = time(NULL);
        }

        if (n_recv != 0) {
          game->name = game->input_buffer;
          game->name.pop_back();
          std::cout << "Nome recebido: " << game->name << std::endl;
          memset(game->input_buffer, 0, sizeof(game->input_buffer));
          if (scoreboard.find(game->name) != scoreboard.end()) {
            game->state = GS_SHOW_RESULTS;
            output = "Você já jogou esse labirinto! Aguarde a hora virar para "
                     "um novo labirinto!\n";
            send(it->fd, output.c_str(), output.length(), 0);
            break;
          }
          game->startTime = 0;
          game->state = GS_PLAYING;
          msg = "Bem vindo " + game->name +
                "! Digite WASD para explorar o labirinto!\n";
          send(it->fd, msg.c_str(), msg.length(), 0);
        }

        break;
      case GS_PLAYING:
        if (game->startTime == 0) {
          game->startTime = time(NULL);
          send_state = true;
        }

        if (n_recv != 0) {
          switch (game->input_buffer[0]) {
          case 'a':
          case 'A':
            if (game->pos.x > 0 &&
                current_board[game->pos.x - 1 +
                              current_boardsize.x * game->pos.y] != '#') {
              game->pos.x--;

              send_state = true;
            }
            break;
          case 'd':
          case 'D':
            if (game->pos.x < current_boardsize.x &&
                current_board[game->pos.x + 1 +
                              current_boardsize.x * game->pos.y] != '#') {
              game->pos.x++;
              send_state = true;
            }
            break;
          case 's':
          case 'S':
            if (game->pos.y < current_boardsize.y &&
                current_board[game->pos.x +
                              current_boardsize.x * (game->pos.y + 1)] != '#') {
              game->pos.y++;
              send_state = true;
            }
            break;
          case 'w':
          case 'W':
            if (game->pos.y > 0 &&
                current_board[game->pos.x +
                              current_boardsize.x * (game->pos.y - 1)] != '#') {
              game->pos.y--;
              send_state = true;
            }
            break;
          }
          send_state = true;

          if (game->pos.x == current_boardsize.x - 2 &&
              game->pos.y == current_boardsize.y - 2) {
            time_t current_time = time(NULL);
            time_t elapsed_seconds = current_time - game->startTime;
            scoreboard.insert({game->name, elapsed_seconds});
            send_state = false;
            game->state = GS_SHOW_RESULTS;
          }
        }

        if (send_state) {
          time_t current_time = time(NULL);
          time_t elapsed_seconds = current_time - game->startTime;
          std::string output =
              "\n--- TEMPO: " + std::to_string(elapsed_seconds) + "s ---\n";

          for (int y = 0; y < current_boardsize.y; ++y) {
            for (int x = 0; x < current_boardsize.x; ++x) {
              if (game->pos.x == x && game->pos.y == y) {
                output += 'P';
              } else {
                output += current_board[y * current_boardsize.x + x];
              }
            }
            output += "\n";
          }

          send(it->fd, output.data(), output.length(), 0);
        }
        break;
      case GS_SHOW_RESULTS: {
        output = get_ranking_string();
        send(it->fd, output.data(), output.length(), 0);
        game->state = GS_END;
        break;
      }
      case GS_END:
        close(it->fd);
        games.erase(it->fd);
        std::cout << "Cliente desconectado!\n";
        it = fds.erase(it) - 1;
        break;
      }
    }
  }

  for (auto &p : fds) {
    close(p.fd);
  }

  return 0;
}
