/* This is a Swiss army knife of utilities that make
 * writing a network application a wee bit easier
 * Rohana Rezel
 * Riolet Corporation
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdarg.h>
#include "nopeutils.h"


/**********************************************************************/
/* Return the value of a query parameter.
 * Parameters: the query string
 *             the name of the parameter
 * Returns: the value of the query parameter */
/**********************************************************************/

char * getQueryParam(const char * queryString, const char *name) { 

	/* Todo: Will break on abc=1&bc=1. fix */

	char *pos1 = strstr(queryString, name);
	char *value = malloc(MAX_BUFFER_SIZE*sizeof(char));
	int i;

	if (pos1) {
		pos1 += strlen(name);

		if (*pos1 == '=') {
			pos1++;
			i=0;
			while (*pos1 && *pos1 != '&') {
				if (*pos1 == '%') {
					value[i]= (char)ToHex(pos1[1]) * 16 + ToHex(pos1[2]);
					pos1 += 3;
				} else if( *pos1=='+' ) {
					value[i] = ' ';
					pos1++;
				} else {
					value[i] = *pos1++;
				}
				i++;
			}

			value[i] = '\0';
			return value;
		} 

	}

	strcpy(value, UNDEFINED);
	return value;
} 

/**********************************************************************/
/* Return the query path. For example the query path for
 * http://nopedotc.com/faq is /faq
 * Note the / at the beginning
 * Parameters: the query string
 * Returns: the query path */
/**********************************************************************/

char * getQueryPath(const char * queryString)
{ 
	char * queryPath;
	queryPath = dupstr(queryString);
	printf("String length %d\n",strlen(queryString));
	u_int i;
	printf("String length %d\n",strlen(queryPath));
	for (i=0;i<strlen(queryString) && (queryPath[i] != '?') && (queryPath[i] != '\0');i++) {
	}

	queryPath[i] = '\0';

	return queryPath;
}

/**********************************************************************/
/* There's a bunch of headers that are exchanged at the beginning
 * between the web browser and the server. If you are ok with just
 * using the default, you may use this function
 * Parameters: the client
 * Returns: an array of headers */
/**********************************************************************/
char ** sendAndReceiveHeaders(int client)
{
	char **headers=readHeaders(client);
	int i=0;
	while (headers[i]!=NULL) {
		printf("Header -> %s",headers[i]);
		i++;
	}
	writeStandardHeaders(client);
	return headers;
}

/* Deprecated. Use nprintf instead. */
void docwrite(int client,const char* string) 
{
	char buf[MAX_BUFFER_SIZE];

	if (strlen(string)<MAX_BUFFER_SIZE)
	{
		sprintf(buf, "%s", string);
		send(client, buf, strlen(buf), 0);
	} else 
	{
		writeLongString(client,string);
	}
}

/* Just like fprintf, but writing to the socket instead of a
 * file.
 */
long nprintf (int client, const char *format, ...) {

		/* Need to figure out a better method for memory management */
		char *buf = malloc(MAX_BUFFER_SIZE*MAX_DPRINTF_SIZE*sizeof(char));

		va_list arg;
		long done;
		va_start (arg, format);
		vsprintf (buf, format, arg);
		va_end (arg);

		if (strlen(buf)<MAX_BUFFER_SIZE) {
			done = (int) send(client, buf, strlen(buf), 0);
		} else {
			done = writeLongString(client,buf);
		}

		free(buf);
		return done;

}

/**********************************************************************/
/* There's a bunch of headers that browsers send us.
 * This function reads 'em.
 * Parameters: the client
 * Returns: an array of headers */
/**********************************************************************/

char ** readHeaders(int client) {
	char buf[MAX_BUFFER_SIZE];
	char **headers;
	int numchars;
	int i=0;

	printf("Mallocing %ld \n",sizeof(char*)*MAX_HEADERS);
	headers=malloc(sizeof(char*)*MAX_HEADERS);


	numchars = getLine(client, buf, sizeof(buf));
	while ((numchars > 0) && strcmp("\n", buf)) {
		headers[i]=malloc((numchars+1)*sizeof(char));
		memcpy(headers[i],buf,numchars);
		i++;
		numchars = getLine(client, buf, sizeof(buf));
	}
	headers[i]=NULL;
	return headers;
}

/* Free thy mallocs */
void freeHeaders(char **headers) {
	int i=0;
	while (headers[i]!=NULL) {
		free(headers[i]);
		i++;
	}
	free(headers);
}

char * getHeader(char **headers, char *header) {
	int i;
	for (i=0;headers[i]!=NULL;i++) {
		/* Work in Progress */
	}
}

void writeStandardHeaders(int client)
{
	char buf[MAX_BUFFER_SIZE];

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE *pFile)
{
	char buf[1024];

	int result = fread (buf,1,1024,pFile);

	while (result!=0) {
		send(client, buf, result, 0);
		result = fread (buf,1,1024,pFile);
	} 
}

/* Serve and entire file. */
void serveFile(int client, const char *filename, const char * type)
{

	FILE *resource = NULL;
	char buf[1024];

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: %s\r\n",type);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);

	resource = fopen(filename, "r");
	if (resource == NULL)
		notFound(client);
	else
	{
		cat(client, resource);
	}
	fclose(resource);
}

/* Write strings that are two big for our buffer */
long writeLongString(int client,const char* longString)
{
	char buf[MAX_BUFFER_SIZE];
	u_int maxSize = sizeof(buf);
	u_int remain = strlen(longString);
	u_long sent=0;
	while (remain)
	{
		u_int toCpy = remain > maxSize ? maxSize : remain;
		strncpy(buf, longString, toCpy);
		longString += toCpy;
		remain -= toCpy;
		sent += send(client, buf, strlen(buf), 0);
	}
	return sent;
}



/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int getLine(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
		n = recv(sock, &c, 1, 0);
		/* DEBUG printf("%02X\n", c); */
		if (n > 0)
		{
			if (c == '\r')
			{
				n = recv(sock, &c, 1, MSG_PEEK);
				/* DEBUG printf("%02X\n", c); */
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
			c = '\n';
	}
	buf[i] = '\0';

	return(i);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void notFound(int client)
{
	nprintf(client, "HTTP/1.0 404 NOT FOUND\r\n");
	nprintf(client, "Content-Type: text/html\r\n");
	nprintf(client, "\r\n");
	nprintf(client, "<HTML><TITLE>Not Found</TITLE>\r\n");
	nprintf(client, "<BODY><P>The server could not fulfill\r\n");
	nprintf(client, "your request because the resource specified\r\n");
	nprintf(client, "is unavailable or nonexistent.\r\n");
	nprintf(client, "</BODY></HTML>\r\n");
}

char * dupstr (const char *s)
{
  size_t len = strlen (s) + 1;
  char *newstr = malloc (len*sizeof(char));

  if (newstr == NULL)
    return NULL;

  memcpy (newstr, s, len);
  newstr[len]=NULL;
  return newstr;
}

void mvhpen8(int client, const char * title, const char * head) {
	mvhpOpen(client,"en","utf-8",title,head);
}
void mvhpOpen(int client, const char * language,const char * charset, const char * title, const char * head){
	nprintf(client,"<!DOCTYPE html>\r\n<html lang=\"%s\">\r\n  <head>\r\n    <meta charset=\"%s\">\r\n    <title>%s</title>\r\n %s</head>\r\n  <body> \r\n",language,charset,title,head);
}

void hClose(int client){
	nprintf(client,"</body>\r\n</html>");
}

void hto(int client, const char * tag, const char *attribs, ...) {
	nprintf(client,"<%s ",tag);
	va_list arg;
	va_start (arg, attribs);
	attrprintf (client, attribs, arg);
	va_end (arg);
	nprintf(client," >",tag);
}

void htc(int client, const char * tag) {
	nprintf(client,"</div>");
}

void htoc(int client, const char *tag,const char *text,const char *attribs, ...) {
	nprintf(client,"<%s ",tag);
	va_list arg;
	va_start (arg, attribs);
	attrprintf (client, attribs, arg);
	va_end (arg);
	nprintf(client," >");
	nprintf(client,"%s",text);
	nprintf(client,"</%s>",tag);
}

void hts(int client, const char *tag,const char *attribs, ...) {
	nprintf(client,"<%s ",tag);
	va_list arg;
	va_start (arg, attribs);
	attrprintf (client, attribs, arg);
	va_end (arg);
	nprintf(client," />");
}

char * hscan(int client, const char * reqStr, const char *msg,...) {
	char * qpath=getQueryPath(reqStr);
	char * qparam=getQueryParam(reqStr,"q");
	hto(client,"div","");
	hto(client,"form","action",qpath);
	nprintf(client,"%s",msg);
	hts(client,"input","type,name","text","q");
	hts(client,"input","type","submit");
	htc(client,"form");
	htc(client,"div");

	return qparam;
}

int attrprintf(int client, const char *attribs, va_list args) {

	char buf[MAX_BUFFER_SIZE]; /* TODO: Dynamic allocation */

	int attrLen=strlen(attribs);

	if (attrLen==0) {
		return true;
	} else {
		printf ("Attrib string length %d\n",attrLen);
	}

	int i;
	int j=0;
	for (i=0;i<=attrLen;i++) {
		if (i<attrLen) {
			if (attribs[i] != ',') {
				buf[j]=attribs[i];
				j++;
				continue;
			}
		}
	    buf[j]=NULL;
		printf("buf %s\n",buf);
	    j=0;
		char * v = va_arg(args, char *);
		nprintf(client," %s=\"%s\" ",buf,v);
	}

	return true;
}

bool route(Request request, const char * path) {
	char * queryPath = getQueryPath(request.reqStr);
	if (strcmp(queryPath,path)==0) {
		return true;
	} else {
		return false;
	}
}

bool routeh(Request request, const char * path) {
	char * queryPath = getQueryPath(request.reqStr);
	if (strcmp(queryPath,path)==0) {
		char ** headers = sendAndReceiveHeaders(request.client);
		if (headers)
			freeHeaders(headers);
		return true;
	} else {
		return false;
	}
}

bool routef(Request request, const char * path, void (* function)(int,char *, char*)) {
	char * queryPath = getQueryPath(request.reqStr);
	if (strcmp(queryPath,path)==0) {
		function(request.client,request.reqStr,request.method);
		return true;
	} else {
		return false;
	}
}

bool routefh(Request request, const char * path, void (* function)(int,char *, char*)) {
	char * queryPath = getQueryPath(request.reqStr);
	if (strcmp(queryPath,path)==0) {
		char ** headers = sendAndReceiveHeaders(request.client);
		function(request.client,request.reqStr,request.method);
		if (headers)
			freeHeaders(headers);
		return true;
	} else {
		return false;
	}
}

