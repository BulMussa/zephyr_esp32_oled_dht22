#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#define WEB_SERVER_PORT 80

/* Start the HTTP server on WEB_SERVER_PORT. Returns 0 or a negative errno code. */
int web_server_start(void);

#endif /* WEB_SERVER_H_ */
