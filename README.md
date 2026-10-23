# Labirinto TCP

Jogo de labirinto em terminal, com servidor e cliente em C++ (Linux, sockets POSIX). Todos os jogadores disputam o mesmo labirinto e o ranking é por tempo.

## Servidor

- Escuta na porta **12345** (TCP) e atende todos os clientes em uma única thread, usando `poll`.
- **Geração do labirinto:** usa o algoritmo de Wilson, com tamanho ímpar aleatório entre 7x7 e 13x13. O início é `S` em `(1,1)` e o fim é `E` em `(w-2, h-2)`.
- **Ciclo do labirinto:** a cada 1 minuto um novo labirinto é gerado e o ranking é zerado. Jogadores que estavam jogando recebem "O labirinto atualizou!", veem o ranking e são desconectados.
- **Máquina de estados por jogador (`GameState`):**
  1. `GS_NAMING`: pede o nome. Se o nome já está no ranking do labirinto atual, o jogador vê o ranking e é desconectado.
  2. `GS_PLAYING`: o jogador envia `w`, `a`, `s` ou `d` (maiúsculas ou minúsculas). O servidor valida a colisão com as paredes `#`, move o `P` e reenvia o mapa com o tempo decorrido.
  3. `GS_SHOW_RESULTS`: ao chegar no fim, o tempo é registrado no ranking e o ranking ordenado é enviado.
  4. `GS_END`: fecha o socket e remove o jogador.

## Cliente

Uso:

```bash
./cliente <ip> <porta>
# exemplo
./cliente 127.0.0.1 12345
```

- Conecta ao servidor e usa `poll` em dois descritores: o socket e o teclado (`STDIN`).
- Tudo que chega do servidor é impresso na tela, sem interpretação.
- Cada linha digitada é enviada ao servidor com `\n` no final (o servidor remove esse caractere do nome).
- Encerra quando o servidor fecha a conexão ou quando o usuário aperta `Ctrl+D`.


## Como jogar
 
1. Compile servidor e cliente com o Makefile:
```bash
   make
```
   Isso gera os binários `server` e `client` (a partir de `server.cpp` e `client.cpp`). Também é possível compilar um só: `make server` ou `make client`.
2. Rode o servidor:
```bash
   ./server
```
3. Em outro terminal, rode um ou mais clientes:
```bash
   ./client 127.0.0.1 12345
```
4. Digite seu nome e depois use `w`, `a`, `s`, `d` seguidos de Enter para se mover.
5. Chegue ao `E` no menor tempo possível.


## Legenda do mapa

| Símbolo | Significado |
|---------|-------------|
| `#`     | Parede      |
| ` `     | Caminho     |
| `S`     | Início      |
| `E`     | Fim         |
| `P`     | Jogador     |

## Protocolo

Texto puro sobre TCP. O servidor envia mensagens e mapas como texto, e o cliente envia o nome (terminado em `\n`) e depois um comando por linha, em que só o primeiro caractere é lido.
