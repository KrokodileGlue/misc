#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>

struct point {
        int x, y;
};

struct game {
        int col, row;
        bool has_bombs;         /* Flag used to prevent unfair starts */
        struct point c;         /* Cursor */

        struct tile {
                bool bomb;
                bool revealed;
                bool flagged;
                int bombs;      /* Number of bombs in surrounding tiles */
        } **b;

        struct point unvisited[10000];
        unsigned nunvisited;

        struct point visited[10000];
        unsigned nvisited;
};

void
tile_render(struct game *g, int y, int x)
{
        printf("\e[%d,%dH", y + 1, x + 1);

        if (g->b[y][x].flagged) printf("\e[48;5;202m");

        if (!g->b[y][x].revealed) {
                printf(" \e[m");
                return;
        }

        if (g->b[y][x].bomb) printf("\e[41mX\e[m");
        else if (g->b[y][x].bombs) printf("\e[48;5;%dm%d\e[m",
                                          g->b[y][x].bombs + 1,
                                          g->b[y][x].bombs);
        else printf(".\e[m");

        fflush(stdout);
}

int
tile_reveal(struct game *g, int y, int x)
{
        g->b[y][x].revealed = true;
        tile_render(g, y, x);
        return g->b[y][x].bomb;
}

void
tile_visit(struct game *g, int y, int x)
{
        tile_reveal(g, y, x);

        usleep(1000000 / (100 + g->nvisited));

        for (int Y = y - 1; Y <= y + 1; Y++)
                for (int X = x - 1; X <= x + 1; X++) {
                        if (Y < 0 || Y >= g->row || X < 0 || X >= g->col)
                                continue;
                        if (g->b[Y][X].bomb)
                                continue;
                        tile_reveal(g, Y, X);
                        if (g->b[Y][X].bombs)
                                continue;
                        bool found = false;
                        for (unsigned i = 0; i < g->nunvisited; i++)
                                if (g->unvisited[i].x == X && g->unvisited[i].y == Y)
                                        found = true;
                        for (unsigned i = 0; i < g->nvisited; i++)
                                if (g->visited[i].x == X && g->visited[i].y == Y)
                                        found = true;
                        if (found) continue;
                        g->unvisited[g->nunvisited++] = (struct point){
                                .x = X,
                                .y = Y,
                        };
                }
}

void
tile_flood(struct game *g, int y, int x)
{
        if (g->b[y][x].revealed) return;

        g->nunvisited = 0;
        g->nvisited = 0;
        g->unvisited[g->nunvisited++] = (struct point){
                .x = x,
                .y = y,
        };

        while (g->nunvisited) {
                struct point point = g->unvisited[0];
                memmove(g->unvisited, g->unvisited + 1, (g->nunvisited - 1) * sizeof *g->unvisited);
                g->nunvisited--;
                g->visited[g->nvisited++] = point;
                tile_visit(g, point.y, point.x);
        }
}

int
checklose(struct game *g)
{
        /*
         * Check whether there are any revealed bombs.
         */
        for (int y = 0; y < g->row; y++)
                for (int x = 0; x < g->col; x++)
                        if (g->b[y][x].bomb && g->b[y][x].revealed)
                                return 1;

        return 0;
}

int
lose(struct game *g)
{
        if (!checklose(g)) return 0;

        for (int y = 0; y < g->row; y++)
                for (int x = 0; x < g->col; x++)
                        tile_reveal(g, y, x);

        return 1;
}

int
setupterminal(struct game *g)
{
        /* First, set up the window size handler. */
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        g->row = w.ws_row - 2, g->col = w.ws_col;

        /* Then configure the input buffering behavior. */
        struct termios term, term_orig;

        if (tcgetattr(0, &term_orig)) {
                perror("tcgetattr");
                return 1;
        }

        term = term_orig;

        term.c_lflag &= ~ICANON;
        term.c_lflag |= ECHO;
        term.c_cc[VMIN] = 1;
        term.c_cc[VTIME] = 0;

        if (tcsetattr(0, TCSANOW, &term)) {
                perror("tcsetattr");
                return 1;
        }

        return 0;
}

void
game_init(struct game *g)
{
        setupterminal(g);

        g->b = realloc(g->b, g->row * sizeof *g->b);
        for (int y = 0; y < g->row; y++)
                g->b[y] = realloc(g->b[y], g->col * sizeof **g->b);

        for (int y = 0; y < g->row; y++)
                for (int x = 0; x < g->col; x++)
                        tile_render(g, y, x);
}

int
checkwin(struct game *g)
{
        for (int y = 0; y < g->row; y++)
                for (int x = 0; x < g->col; x++)
                        if (!g->b[y][x].bomb && !g->b[y][x].revealed)
                                return 0;
        return 1;
}

int
win(struct game *g)
{
        if (!checkwin(g)) return 0;

        for (int y = 0; y < g->row; y++)
                for (int x = 0; x < g->col; x++)
                        tile_reveal(g, y, x);

        puts("\e[43mYOU WIN :))))))))\e[m");

        return 1;
}

void
set_up_us_the_bomb(struct game *g)
{
        int nx = rand() % g->col, ny = rand() % g->row;

        if ((nx == g->c.x && ny == g->c.y) || g->b[ny][nx].bomb) {
                /* Dangerous :) */
                set_up_us_the_bomb(g);
                return;
        }

        g->b[ny][nx].bomb = true;

        for (int y = ny - 1; y <= ny + 1; y++)
                for (int x = nx - 1; x <= nx + 1; x++) {
                        if (x < 0 || x >= g->col || y < 0 || y >= g->row)
                                continue;
                        g->b[y][x].bombs++;
                }
}

void set_up_us_the_bombs(struct game *g)
{
        for (int i = 0; i < g->col * g->row / 5; i++)
                set_up_us_the_bomb(g);
}

int
cmd(struct game *g, char move)
{
        int y = g->c.y, x = g->c.x;

        switch (move) {
        case 'w': y--; break;
        case 's': y++; break;
        case 'a': x--; break;
        case 'd': x++; break;
        case 'f':
                g->b[y][x].flagged = !g->b[y][x].flagged;
                break;
        case 'e':
                if (!g->has_bombs) {
                        set_up_us_the_bombs(g);
                        g->has_bombs = true;
                }

                tile_flood(g, y, x);

                if (lose(g)) return 1;
                if (win(g)) return 1;

                break;
        }

        if (x >= 0 && x < g->col) g->c.x = x;
        if (y >= 0 && y < g->row) g->c.y = y;

        return 0;
}

int
main(void)
{
        srand(time(0));

        struct game *g = calloc(1, sizeof *g);
        game_init(g);

        printf("\e[2J");
        fflush(stdout);

        while (1) {
                printf("\e[%d,%dH", g->c.y + 1, g->c.x + 1);
                fflush(stdout);

                char move;
                if (!read(0, &move, 1)) break;

                for (int x = 0; x < g->col; x++) tile_render(g, g->c.y, x);

                if (cmd(g, move)) return 0;
        }
}
