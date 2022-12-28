#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define ROW 20
#define COL 10

struct game {
        struct {
                int x, y;
                int type;
                int b[8];
        } piece;

        unsigned char b[ROW][COL];

        int score, level, lines;
        int next_piece;

        timer_t timerid;
        struct itimerspec ts;
} *g;

int levels[] = {
        53, 49, 45, 41, 37, 33, 28, 22, 17, 11,
        10, 9, 8, 7, 6, 6, 5, 5, 4, 4, 3
};

int pieces[][8] = {
        { 0, 0, -1,  0, 1, 0,  0, 1 }, /* T piece */
        { 0, 0,  0, -1, 0, 1,  1, 1 }, /* L piece */
        { 0, 0,  1,  0, 0, 1,  1, 1 }, /* O piece */
        { 0, 0,  0, -1, 0, 1, -1, 1 }, /* J piece */
        { 0, 0,  0, -1, 0, 1,  0, 2 }, /* I piece */
        { 0, 0,  1,  0, 0, 1, -1, 1 }, /* S piece */
        { 0, 0, -1,  0, 0, 1,  1, 1 }  /* Z piece */
};

char *color[] = {
        "\e[48;2;255;255;0m",
        "\e[48;2;255;255;82m",
        "\e[48;2;255;255;122m",
        "\e[48;2;255;255;157m",
        "\e[48;2;255;255;191m",
        "\e[48;2;255;255;223m",
        "\e[48;2;255;255;255m",
};

void draw_score(void)
{
        printf("\e[%d;%dHscore: %d", ROW / 2, 2 * COL + 2, g->score);
        printf("\e[%d;%dHlevel: %d", ROW / 2 + 1, 2 * COL + 2, g->level);
        printf("\e[%d;%dHlines: %d", ROW / 2 + 2, 2 * COL + 2, g->lines);
}

void draw_background_cell(int y, int x)
{
        (void)g;
        printf("\e[%d;%dH\e[48;2;%d;%d;%dm  \e[0m",
               y + 1, 2 * x + 1,
               255 - y * 10 - 50, x * 20, 100);
}

void undraw_preview(void)
{
        for (int i = 0; i < 4; i++)
                printf("\e[%d;%dH%s  \e[m",
                       1 + pieces[g->next_piece][i * 2 + 1] + 1,
                       2 * (COL + 2 + pieces[g->next_piece][i * 2]) + 1,
                       "\e[m");
}

void draw_preview(void)
{
        for (int i = 0; i < 4; i++)
                printf("\e[%d;%dH%s  \e[m",
                       1 + pieces[g->next_piece][i * 2 + 1] + 1,
                       2 * (COL + 2 + pieces[g->next_piece][i * 2]) + 1,
                       color[g->next_piece]);
}

/* Erases the current piece. */
void undraw_current(void)
{
        for (int i = 0; i < 4; i++)
                draw_background_cell(g->piece.y + g->piece.b[i * 2 + 1],
                                     g->piece.x + g->piece.b[i * 2]);
}

/* Draws the current piece. */
void draw_current(void)
{
        for (int i = 0; i < 4; i++)
                printf("\e[%d;%dH%s  \e[m",
                       g->piece.y + g->piece.b[i * 2 + 1] + 1,
                       2 * (g->piece.x + g->piece.b[i * 2]) + 1,
                       color[g->piece.type]);
}

int will_collide(int yoff, int xoff)
{
        int y = g->piece.y + yoff, x = g->piece.x + xoff;

        for (int i = 0; i < 4; i++) {
                int Y = y + g->piece.b[i * 2 + 1],
                        X = x + g->piece.b[i * 2];
                if (Y < 0) continue;
                if (Y >= ROW || X < 0 || X >= COL || g->b[Y][X])
                        return 1;
        }

        return 0;
}

void undraw_shadow(void)
{
        int tmpy = g->piece.y;
        while (!will_collide(1, 0)) g->piece.y++;
        undraw_current();
        g->piece.y = tmpy;
}

void draw_shadow(void)
{
        int tmpy = g->piece.y;
        while (!will_collide(1, 0)) g->piece.y++;
        for (int i = 0; i < 4; i++) {
                int y = g->piece.y + g->piece.b[i * 2 + 1];
                int x = g->piece.x + g->piece.b[i * 2];
                printf("\e[%d;%dH\e[48;2;%d;%d;%dm  \e[0m",
                        y + 1, 2 * x + 1,
                        255 - y * 10 - 50 + 50,
                        x * 20 + 50,
                        100 + 50);
        }
        g->piece.y = tmpy;
}

void move_piece(int y, int x)
{
        if (will_collide(y, x)) return;
        undraw_current();
        undraw_shadow();
        g->piece.y += y;
        g->piece.x += x;
        draw_shadow();
        draw_current();
}

void pick_piece(void)
{
        int n = g->next_piece;

        undraw_preview();
        g->next_piece = rand() % 7;
        draw_preview();

        g->piece.x = 4;
        g->piece.y = 0;
        g->piece.type = n;

        memcpy(g->piece.b, pieces[n], sizeof *pieces);
        draw_shadow();
        draw_current();
}

void rotate_piece(void)
{
        if (g->piece.type == 2) return;

        int buf[sizeof g->piece.b];
        memcpy(buf, g->piece.b, sizeof buf);

        undraw_shadow();
        undraw_current();

        for (int i = 0; i < 4; i++) {
                int temp = g->piece.b[i * 2 + 1];
                g->piece.b[i * 2 + 1] = g->piece.b[i * 2];
                g->piece.b[i * 2] = -temp;
	}

        if (will_collide(0, 0)) {
                memcpy(g->piece.b, buf, sizeof buf);
                draw_shadow();
                draw_current();
                return;
        }

        draw_shadow();
        draw_current();
}

struct termios term, term_orig;

void terminate_gracefully(int arg)
{
        (void)arg;
        tcsetattr(0, TCSANOW, &term_orig); /* Reset terminal attributes */
        printf("\e[?1049l\e[?25h");        /* Reset screen state */
        printf("final score: %d\n", g->score);
        exit(0);                           /* Die */
}

void freeze_piece(void)
{
        for (int i = 0; i < 4; i++) {
                int y = g->piece.y + g->piece.b[i * 2 + 1];
                int x = g->piece.x + g->piece.b[i * 2];
                if (y < 0) terminate_gracefully(0);
                if (x >= COL || y >= ROW || x < 0) continue;
                if (g->b[y][x]) terminate_gracefully(0);
                g->b[y][x] = g->piece.type + 1;
        }

        int num_cleared = 0;

        /* Check win state */
        for (int y = 0; y < ROW; y++) {
                int full = 1;
                for (int x = 0; x < COL; x++)
                        if (!g->b[y][x])
                                full = 0;
                if (!full) continue;
                num_cleared++;

                /* Clear this line */
                for (int i = y; i >= 0; i--) {
                        if (i) memcpy(g->b[i], g->b[i - 1], sizeof *g->b);
                        for (int x = 0; x < COL; x++) {
                                draw_background_cell(i, x);
                                if (g->b[i][x])
                                        printf("\e[%d;%dH%s  \e[m",
                                               i + 1,
                                               2 * x + 1,
                                               color[g->b[i][x] - 1]);
                        }
                }

                y--;
        }

        switch (num_cleared) {
                case 1: g->score += 40 * (g->level + 1); break;
                case 2: g->score += 100 * (g->level + 1); break;
                case 3: g->score += 300 * (g->level + 1); break;
                case 4: g->score += 1200 * (g->level + 1); break;
        }

        if (g->lines % 10 > (g->lines + num_cleared) % 10) g->level++;
        if (g->level >= 20) g->level = 20;
        g->lines += num_cleared;

        if (num_cleared) {
                g->ts = (struct itimerspec){ .it_value.tv_nsec = ((float)levels[g->level] / 60.0) * 1000000000.0 };
        }

        draw_score();

        timer_settime(g->timerid, 0, &g->ts, 0);
        pick_piece();
}

void hard_drop(void)
{
        undraw_current();
        while (!will_collide(1, 0)) g->piece.y++;
        draw_current();
        freeze_piece();
        draw_current();
}

int cmd(char move)
{
        switch (move) {
        case 'w': rotate_piece(); break;
        case 's':
                  move_piece(1, 0);
                  timer_settime(g->timerid, 0, &g->ts, 0);
                  break;
        case 'a': move_piece(0, -1); break;
        case 'd': move_piece(0, 1); break;
        case ' ': hard_drop(); break;
        case 'q': terminate_gracefully(0); break;
        }

        return 0;
}

int setupterminal()
{
        if (tcgetattr(0, &term_orig)) {
                perror("tcgetattr");
                return 1;
        }

        term = term_orig;

        term.c_lflag &= ~ICANON;
        term.c_lflag &= ~ECHO;
        term.c_cc[VMIN] = 1;

        if (tcsetattr(0, TCSANOW, &term)) {
                perror("tcsetattr");
                return 1;
        }

        /* Swap the screen, clear it, make the cursor invisible. */
        printf("\e[?1049h\e[2J\e[?25l");

        return 0;
}

void timer_callback(union sigval arg)
{
        if (will_collide(1, 0)) freeze_piece();
        else move_piece(1, 0);

        timer_settime(g->timerid, 0, &g->ts, 0);
}

void game_init(void)
{
        /* Make a one second timer spec. */
        g->ts = (struct itimerspec){ .it_value.tv_nsec = ((float)levels[0] / 60.0) * 1000000000 };

        /* Create a thread-based timer. */
        timer_create(CLOCK_REALTIME,
                     &(struct sigevent){
                             .sigev_notify = SIGEV_THREAD,
                             .sigev_value = { .sival_ptr = g },
                             .sigev_notify_function = timer_callback,
                     }, &g->timerid);

        /*
         * Set the first timer; whenever the timer times out it will
         * just set the timer again.
         */
        timer_settime(g->timerid, 0, &g->ts, 0);

        /*
         * Handle SIGINT gracefully by resetting the terminal to its
         * original state. Some rather disconcerting persistent
         * changes to the terminal happen otherwise.
         */
        signal(SIGINT, terminate_gracefully);

        /* Initialize the actual game state. */
        for (int y = 0; y < ROW; y++)
                for (int x = 0; x < COL; x++)
                        draw_background_cell(y, x);

        g->next_piece = rand() % 7;
        pick_piece();
        draw_current();
        draw_score();
}

int main(void)
{
        srand(time(0));
        setvbuf(stdout, 0, _IONBF, 0);

        setupterminal();

        g = calloc(1, sizeof *g);
        game_init();

        while (1) {
                char move;
                if (!read(0, &move, 1)) return 1;
                if (cmd(move)) return 0;
        }
}
