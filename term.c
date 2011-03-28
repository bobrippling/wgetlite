#include <stdio.h>
#include <sys/ioctl.h>

static volatile int got_sigwinch = 1;
static int term_x = 0, term_y = 0;

void term_winch(int unused)
{
	(void)unused;
	got_sigwinch = 1;
}

void term_xy(int *px, int *py)
{
	if(got_sigwinch){
#ifdef TIOCGSIZE
		struct ttysize ts;
# define COLS ts.ts_cols
# define ROWS ts.ts_lines
# define FLAG TIOCGSIZE

#elif defined(TIOCGWINSZ)
		struct winsize ts;
# define COLS ts.ws_col
# define ROWS ts.ws_row
# define FLAG TIOCGWINSZ

#endif

		if(ioctl(1, FLAG, &ts) == -1){
			term_x = 80;
			term_y = 25;
		}else{
			term_x = COLS;
			term_y = ROWS;
		}

		got_sigwinch = 0;
	}

	*px = term_x;
	*py = term_y;
}
