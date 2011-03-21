#ifndef PROGRESS_H
#define PROGRESS_H

void progress_show(int this, int total);
void progress_fin( int this, int total);
void progress_incomplete(void);
void progress_unknown(void);

#endif
