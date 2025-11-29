# Projeto SO-25/26 - 1ª Fase - Pacmanist

## Descrição

**Pacmanist** é um jogo inspirado no clássico Pacman, desenvolvido como código base do projeto da disciplina de Sistemas Operativos (SO-25/26). 
O jogo implementa um sistema de agentes (Pacman e monstros) que se movem num tabuleiro, com o objetivo de coletar pontos enquanto evitam os monstros.

## Estrutura do Projeto

### Ficheiros Principais

- **`game.c`** - Ficheiro principal que contém o loop main do jogo, controlando a lógica do mesmo e a sequência de eventos.
- **`board.h`** - Definições das estruturas de dados do tabuleiro e dos agentes (Pacman e monstros).
- **`board.c`** - Implementação da lógica do tabuleiro e movimentação dos agentes.
- **`display.h`** / **`display.c`** - Interface gráfica que faz uso da biblioteca `ncurses` para desenhar o tabuleiro e UI, abstraindo a complexidade.

### Estrutura de Diretórios

```
SO-2526-Proj1/
├── Makefile
├── README.md
├── ncurses.suppression
├── bin/                    # Executáveis gerados
│   └── Pacmanist
├── obj/                    # Ficheiros objeto (.o)
├── include/                # Ficheiros de cabeçalho
│   ├── board.h
│   └── display.h
└── src/                    # Código fonte
    ├── board.c
    ├── display.c
    └── game.c
```

## Dependências

### NCurses Library

O projeto requer a biblioteca `NCurses` para a interface gráfica do terminal.

**Ubuntu/Debian:**
```bash
sudo apt-get install libncurses-dev
```

**CentOS/RHEL/Fedora:**
```bash
sudo yum install ncurses-devel
# ou para versões mais recentes:
sudo dnf install ncurses-devel
```

**macOS (usando Homebrew):**
```bash
brew install ncurses
```

## Compilação

O projeto utiliza um Makefile para automatizar o processo de compilação.

### Regras do Makefile Disponíveis

- **`make`** ou **`make all`** - Compila o projeto completo
- **`make pacmanist`** - Compila o executável principal
- **`make run`** - Compila e executa o jogo
- **`make clean`** - Remove os ficheiros objeto e executável
- **`make folders`** - Cria os diretórios necessários (`obj/`: que irá conter os *.o, e `bin/`: que irá conter o executável)

### Compilação Manual

```bash
# Compilar o projeto
make

# Ou compilar e executar diretamente
make run
```

### Configuração do Compilador

O projeto está configurado para:
- **Compilador:** GCC
- **Standard:** C17
- **Flags de Compilação:** `-g -Wall -Wextra -Werror -std=c17 -D_POSIX_C_SOURCE=200809L`
- **Linking:** `-lncurses`

## Execução

Após a compilação, o executável será gerado em `bin/Pacmanist`.

```bash
# Executar o jogo
./bin/Pacmanist

# Ou usar o Makefile
make run
```

## Requisitos do Sistema

- Sistema operativo Unix/Linux ou macOS
- GCC compiler
- NCurses library
- Make utility

## Debugging

### Ficheiro de Log

Para facilitar a depuração, o programa gera automaticamente um ficheiro `debug.log` que contém informações detalhadas sobre a execução do jogo. O log inclui:

- Teclas pressionadas pelo jogador (ex: `KEY A`, `KEY Q`)
- Atualizações do ecrã (`REFRESH`)
- Informações do nível (dimensões, tempo, ficheiros dos agentes)
- Estado atual do tabuleiro com as posições dos agentes (P=Pacman, M=Monster, W=Wall)

Este ficheiro é especialmente útil para rastrear o comportamento dos agentes, sequência de movimentos, e debug de colisões, etc.

### Valgrind

A biblioteca ncurses contem alguns [memory leaks](https://invisible-island.net/ncurses/ncurses.faq.html#config_leaks) a serem ignorados.
Para suprimir os reports do valgrind associados a estes leaks, e facilitar a intrepretação do output da ferramenta, o código base contem o ficheiro `ncurses.suppressions` que pode ser passado para o valgrind com a flag `--suppressions=<suppression_file>`.
Memory leaks associados ao ncurses (ou seja, ignorados pelo suppression file fornecido), não serão contabilizados na avaliação.

### GDB

Dado que a biblioteca ncurses captura totalmente o terminal, o uso de ferramentas como o gdb tem de ser adaptado.
Em vez de correr o executavel com o gdb como explicado no guião da [deteção de erros](https://github.com/tecnico-so/lab_detecao-erros), deve executar o Pacmanist normalmente e, numa segunda janela, fazer "attach" ao programa, atravez do seguinte comando:

```bash
gdb -p <Pacmaist_PID>
```

Note que o Pacmanist continuará a correr até ao momento de fazer attach, logo, se quiser fazer debug no inicio da aplicação, pode adicionar um delay no inicio do programa, para dar tempo de fazer attach. 

# GDB – Debug com dois terminais e redirecionamento TTY

A biblioteca `ncurses` captura o terminal onde o programa corre, por isso é necessário usar **dois terminais** para fazer debug no GDB. O processo recomendado é o seguinte:

## Debug usando dois terminais + `tty`

1. **Terminal 1 – abrir o gdb**
   ```bash
   sudo gdb ./bin/Pacmanist
   ```

2. **Definir os argumentos do programa dentro do gdb**
   ```gdb
   (gdb) set args ./tests/levels_example_1/
   ```

3. **Terminal 2 – descobrir o dispositivo do terminal**
   ```bash
   $ tty
   /dev/pts/X
   ```
   (substitua `X` pelo número que aparecer)

4. **Voltar ao Terminal 1 e redirecionar o I/O do programa**
   ```gdb
   (gdb) tty /dev/pts/X
   ```

   Agora o ncurses irá correr no **Terminal 2**, enquanto o GDB permanece no Terminal 1.

5. **Opcional: definir um breakpoint**
   ```gdb
   (gdb) break main
   ```

6. **Executar o programa**
   ```gdb
   (gdb) run
   ```

A partir deste momento:
- O GDB continua totalmente funcional no **Terminal 1**;
- O jogo aparece e aceita input no **Terminal 2**.

Isto permite inspeccionar variáveis, fazer stepping, breakpoints e analisar comportamento, sem que o ncurses destrua o terminal do GDB.

---

## Alternativa: Attach ao processo já em execução

```bash
gdb -p <Pacmanist_PID>
```