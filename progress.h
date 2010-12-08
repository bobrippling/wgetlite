#ifndef PROGRESS_H
#define PROGRESS_H

void progress_show(int this, int total, const char *file);
void progress_fin(void);
void progress_unknown(void);

#endif
