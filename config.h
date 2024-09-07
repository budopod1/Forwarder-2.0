#ifndef CONFIG_H
#define CONFIG_H

#define SERVERNAME "Forwarder 2.0"

/*
near-native experience:
* en.wikipedia.org
* github.com
* opencv.org
* google.com
* desmos.com
* commons.wikimedia.org
* nytimes.org
Incomplete functionality:
* codepen.io
* web.archive.org
Does not work:
* youtube.com
*/
/*
#define TARGETHOSTNAME "google.com"
#define TARGETPORT "443"

#define USE_SSL
#define SEND_WWW
*/

#define OURHOSTNAME "cec60895-df5d-4c62-97c9-4b6c1f538a6a-00-2zvct7vdqwawt.kirk.replit.dev"

#define SPECIALURL "/forwarder"
#define VIEWURL "/view"
#define STYLEURL "/style.css"
#define SCRIPTURL "/script.js"
#define NEWTABURL "/new_tab"
#define CHANGEORIGINURL "/change_origin"
#define FAVICONURL "/favicon.ico"

#define SERVEPORT "8080"
#define BACKLOG 10
#define MAX_RECV 4096
#define MAX_THREAD_COUNT 10

#define REQUEST true
#define RESPONSE false

#endif
