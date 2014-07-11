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

char * getQueryParam(const char * queryString, const char *name) { 

	/* Todo: Will break on abc=1&bc=1. fix */

	char *pos1 = strstr(queryString, name);
	char *value = malloc(4096);
	int i;

	if (pos1) {
		pos1 += strlen(name);

		if (*pos1 == '=') { // Make sure there is an '=' where we expect it
			pos1++;
			i=0;
			while (*pos1 && *pos1 != '&') {
				if (*pos1 == '%') { // Convert it to a single ASCII character and store at our Valueination
					value[i]= (char)ToHex(pos1[1]) * 16 + ToHex(pos1[2]);
					pos1 += 3;
				} else if( *pos1=='+' ) { // If it's a '+', store a space at our Valueination
					value[i] = ' ';
					pos1++;
				} else {
					value[i] = *pos1++; // Otherwise, just store the character at our Valueination
				}
				i++;
			}

			value[i] = '\0';
			return value;
		} 

	}

	strcpy(value, UNDEFINED);	// If param not found, then use default parameter
	return value;
} 

char * getQueryPath(const char * queryString)
{ 
	char * queryPath;
	queryPath = malloc(strlen(queryString)+1);
	memcpy(queryPath,queryString,strlen(queryString));
	int i;
	for (i=0;i<strlen(queryPath) && (queryPath[i] != '?') && (queryPath[i] != '\0');i++) {
	}
	if (queryPath[i] == '?') {
		queryPath[i] = '\0';
	}
	return queryPath;
}

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


long nprintf (int client, const char *format, ...) {

		/* Need to figure out a better method for memory management */
		char *buf = malloc(MAX_BUFFER_SIZE*MAX_DPRINTF_SIZE);

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


char ** readHeaders(int client) {
	char buf[MAX_BUFFER_SIZE];
	char *headers[MAX_HEADERS];
	int numchars = 1;

	/* buf[0] = 'A'; buf[1] = '\0'; */
	int i=0;
	headers[0]=NULL;
	while ((numchars > 0) && strcmp("\n", buf)) {
		numchars = getLine(client, buf, sizeof(buf));
		headers[i]=malloc(numchars+1);
		memcpy(headers[i],buf,numchars);
		i++;
	}
	return headers;
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

long writeLongString(int client,const char* longString)
{
	char buf[1024];
	int remain = strlen(longString);
	long sent=0;
	while (remain)
	{
		int toCpy = remain > sizeof(buf) ? sizeof(buf) : remain;
		memcpy(buf, longString, toCpy);
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
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}
