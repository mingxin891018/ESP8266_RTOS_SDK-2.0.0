#ifndef http_url_h
#define http_url_h

struct post_data
{
    char postData[10 * 1024];
};
typedef struct post_data PostData, *PostData_pointer;

struct url
{
    char url[500];
    PostData postData;
};

typedef struct url URL, *PURL;
extern char cookie[100];
extern int parse_url(char *host, char *path, char *url);
extern int organize_url(char *url, char *host, char *path);
extern int http_request(char *request, PURL url);
extern void num_to_string(char *str, int i);
extern int Init_URL(PURL Purl, char *url, char *postData);
extern int rmfile(char *str);
extern int str_to_int(const char *str);
extern int getDownloadFilename(char *path,char *filename);
extern int getContentLength(char *buf);
extern int getHostPort(char *host);


#endif //