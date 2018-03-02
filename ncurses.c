#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>

struct entity {
	unsigned x, y;
	int health, attack;
	char name;

	enum {
		ENTITY_PLAYER,
		ENTITY_GOBLIN
	} kind;
};

struct game {
	struct entity *e;
	unsigned num_entity;
	bool running;
	struct entity p;
	WINDOW *win;
};

#define GENERIC_GOBLIN(X,Y) (struct entity){ \
	.x = (X), \
	.y = (Y), \
	.name = 'g', \
	.kind = ENTITY_GOBLIN, \
	.health = 20, \
	.attack = 20 \
}

void init(void)
{
	initscr();
	clear();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	curs_set(0);
	start_color();
	init_pair(1, COLOR_GREEN, COLOR_BLACK);
}

void update(struct game *g)
{
	erase();
	refresh();
	wclear(g->win);
	box(g->win, 0, 0);
	wmove(g->win, 0, 4);
	wprintw(g->win, "test");
	wmove(g->win, 1, 1);
	wprintw(g->win, "health: %d", g->p.health);
	wrefresh(g->win);

	mvaddch(g->p.y, g->p.x, g->p.name);
	for (unsigned i = 0; i < g->num_entity; i++) {
		mvaddch(g->e[i].y, g->e[i].x, g->e[i].name);
		if (g->e[i].x == g->p.x && g->e[i].y == g->p.y)
			g->p.health -= g->e[i].attack;
	}

	int ch = getch();
	switch (ch) {
	case KEY_UP:    g->p.y--; break;
	case KEY_DOWN:  g->p.y++; break;
	case KEY_LEFT:  g->p.x--; break;
	case KEY_RIGHT: g->p.x++; break;
	case 'q':       g->running = false; break;
	}

	if (g->p.health <= 0) {
		erase();
		refresh();
		wclear(g->win);
		box(g->win, 0, 0);
		wmove(g->win, 0, 4);
		wprintw(g->win, "test");
		wmove(g->win, 1, 1);
		wprintw(g->win, "health: %d", g->p.health);
		wmove(g->win, 2, 1);
		wprintw(g->win, "You have died! Press q to exit...");
		wrefresh(g->win);
		g->running = false;
		for (int ch = 0; (ch = getch()) != 'q';);
	}

}

void add_entity(struct game *g, struct entity e)
{
	g->e = realloc(g->e, (g->num_entity + 1) * sizeof *g->e);
	g->e[g->num_entity++] = e;
}

struct game *new_game(void)
{
	struct game *g = malloc(sizeof *g);
	memset(g, 0, sizeof *g);
	g->running = true;
	return g;
}

int main(void)
{
	init();

	struct game *g = new_game();

	g->p = (struct entity){
		.x = 5,
		.y = 5,
		.name = '@',
		.kind = ENTITY_PLAYER,
		.health = 100,
		.attack = 5
	};

	add_entity(g, GENERIC_GOBLIN(20, 20));
	add_entity(g, GENERIC_GOBLIN(10, 15));
	add_entity(g, GENERIC_GOBLIN(15, 3));

	g->win = newwin(5, 80, 19, 0);
	wbkgd(g->win, COLOR_PAIR(1));

	while (g->running)
		update(g);

	endwin();
	return 0;
}
