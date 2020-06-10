#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** data ***/
struct termios orig_termios;

/*** terminal **/
void die(const char *s) {
	perror(s);
	exit(1);
}

//disables raw mode after the program quits using atexit() in enablerawmode()
void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr");
}

//enables the raw mode text input. also sets the atexit() function to make sure that terminal
//attributes are set back correctly.
void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");

	atexit(disableRawMode);

	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_iflag &= ~(OPOST);
	raw.c_iflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** init ***/
int main() {
	enableRawMode();

	while (1) {
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
		if (iscntrl(c)) {
			printf("%d\r\n", c);
	        } else {
			printf("%d ('%c')\r\n", c, c);
		}
		if (c == 'q') break;
	} 
	return 0;
}
