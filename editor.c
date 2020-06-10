#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/*** definitions ***/

#define CTRL_KEY(k) ((k) & 0x1f)

#define clear_scrn "\x1b[2J"
#define cursor_repos "\x1b[H"

#define EDITOR_VERSION "0.0.1"

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
/*** data ***/

struct editorConfig {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_term_attr;	
};

struct editorConfig E;

/*** terminal **/

//error handling when low-level error occurs
void die(const char *s) {
	write(STDOUT_FILENO, clear_scrn, 4);
	write(STDOUT_FILENO, cursor_repos, 3);

	perror(s);
	exit(1);
}

//disables raw mode after the program quits using atexit() in enablerawmode()
void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term_attr) == -1) die("tcsetattr");
}

//enables the raw mode text input. also sets the atexit() function to make sure that terminal
//attributes are set back correctly.
void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &E.orig_term_attr) == -1) die("tcgetattr");

	atexit(disableRawMode);

	struct termios raw = E.orig_term_attr;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_iflag &= ~(OPOST);
	raw.c_iflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

//reads in key input
int editorReadKey() {
	int num_read;
	char c;
	while ((num_read = read(STDIN_FILENO, &c, 1)) != 1) {
		if (num_read == -1 && errno != EAGAIN) die("read");
	}

	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[0] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}

		return '\x1b';
	} else {
		return c;
	}
}

//gets the current cursor position
int getCursorPosition(int *rows, int *cols) {
	char buffer[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buffer) - 1) {
		if (read(STDIN_FILENO, &buffer[i], 1) == 1) break;
		if (buffer[i] == 'R') break;
		i++;
	}
	buffer[i] = '\0';

	printf("\r\n&buffer[1]: '%s'\r\n", &buffer[1]);
	editorReadKey();

	if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;
	if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

//gets size of window using ioctl
int getWindowSize(int *rows, int *cols) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
}

/*** input ***/

//moves the cursor using arrow keys
void editorMoveCursor(int key) {
	switch (key) {
		case ARROW_LEFT:
			if (E.cx != 0) {
				E.cx--;
			}
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1) {
				E.cx++;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy != E.screenrows - 1) {
				E.cy++;
			}
			break;
	}
}

//processes key input
void editorProcessKeypress() {
	int c = editorReadKey();
	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, clear_scrn, 4);
			write(STDOUT_FILENO, cursor_repos, 3);
			exit(0);
			break;
		case HOME_KEY:
			E.cx = 0;
			break;
		
		case END_KEY:
			E.cx = E.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
		 		int times = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;

		case ARROW_LEFT:
		case ARROW_RIGHT:
		case ARROW_UP:
		case ARROW_DOWN:
			editorMoveCursor(c);
			break;
	}
}

/*** output ***/

//draws ~ to signify indvidual rows.
void editorDrawRows(struct abuf *ab) {
	for (int i = 0; i < E.screenrows; i++) {
		if (i == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Best Editor -- version %s", EDITOR_VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding  = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (i < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}
}

//function to refresh the editor
void editorRefreshScreen() {
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, cursor_repos, 3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** init ***/

void initEditor() {
	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
	enableRawMode();
	initEditor();

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	} 
	return 0;
}
