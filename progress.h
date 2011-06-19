#ifndef PROGRESS_H
#define PROGRESS_H

void progress_show(long this, long total, long bps);
void progress_fin( long this, long total);
void progress_incomplete(void);
void progress_unknown(long sofar, long bps);

#endif
