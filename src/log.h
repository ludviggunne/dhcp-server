#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

void log_info (const char *fmt, ...);
void log_error (const char *fmt, ...);
void log_errno (const char *fmt, ...);

#endif
