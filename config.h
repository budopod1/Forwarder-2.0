#ifndef CONFIG_H
#define CONFIG_H

#ifndef CONFIG_SRC

extern char *TARGETHOSTNAME;
extern char *TARGETPORT;
#define USE_SSL

extern char *OURHOSTNAME;

extern char *SERVEPORT;
extern int BACKLOG;
extern int MAX_RECV;

extern int REQUEST;
extern int RESPONSE;

#endif

#endif
