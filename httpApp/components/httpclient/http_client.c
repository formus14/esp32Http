/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include "http_client.h"
#include "stringx.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"


const unsigned int BUF_SIZE = 1024; //max single read buf size
const unsigned int RECV_BUF_MAX_SIZE = 10240; //max total read buf size


/*
	Handles redirect if needed for get requests
*/
http_response_t *handle_redirect_get(struct http_response* hresp, char* custom_headers)
{
	if(hresp->status_code_int > 300 && hresp->status_code_int < 399)
	{
        http_response_free(hresp);  //free the 1st http response pointer if http is redirected.
        
        DPRINT("redirect get\n");
		char *token = strtok(hresp->response_headers, "\r\n");
		while(token != NULL)
		{
			if(str_contains(token, "Location:"))
			{
				/* Extract url */
				char *location = str_replace("Location: ", "", token);
				return http_get(location, custom_headers);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	else
	{
		/* We're not dealing with a redirect, just return the same structure */
		return hresp;
	}
    
    return hresp;
}

/*
	Handles redirect if needed for head requests
*/
http_response_t *handle_redirect_head(struct http_response* hresp, char* custom_headers)
{
	if(hresp->status_code_int > 300 && hresp->status_code_int < 399)
	{
		char *token = strtok(hresp->response_headers, "\r\n");
		while(token != NULL)
		{
			if(str_contains(token, "Location:"))
			{
				/* Extract url */
				char *location = str_replace("Location: ", "", token);
				return http_head(location, custom_headers);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	else
	{
		/* We're not dealing with a redirect, just return the same structure */
		return hresp;
	}
    
    return hresp;
}

/*
	Handles redirect if needed for post requests
*/
http_response_t *handle_redirect_post(struct http_response* hresp, char* custom_headers, char *post_data)
{
	if(hresp->status_code_int > 300 && hresp->status_code_int < 399)
	{
        
        DPRINT("debug");
		char *token = strtok(hresp->response_headers, "\r\n");
		while(token != NULL)
		{
			if(str_contains(token, "Location:"))
			{
				/* Extract url */
				char *location = str_replace("Location: ", "", token);
				return http_post(location, custom_headers, post_data);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	else
	{
        
        DPRINT("debug");
		/* We're not dealing with a redirect, just return the same structure */
		return hresp;
	}
    
    return hresp;
}

/*
	Makes a HTTP request and returns the response
*/
http_response_t *http_req(char *http_headers, parsed_url_t_2  *purl)
{
	/* Parse url */
	if(purl == NULL)
	{
		printf("Unable to parse url\n");
		return NULL;
	}


	/* Allocate memeory for htmlcontent */
	struct http_response *hresp = (http_response_t *)malloc(sizeof(struct http_response));
    
	if(hresp == NULL)
	{
		printf("Unable to allocate memory for htmlcontent.\n");
		return NULL;
	}
	hresp->body = NULL;
	hresp->request_headers = NULL;
	hresp->response_headers = NULL;
	hresp->status_code = NULL;
	hresp->status_text = NULL;


    /*Build socket infos*/
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    int socket_fd;

    int err = getaddrinfo(purl->host, purl->port, &hints, &res);
    
    if(err != 0 || res == NULL) {
        printf("DNS lookup failed err=%d res=%p\n", err, res);
        return NULL;
    }
    
    /* Code to print the resolved IP.
    Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
    //addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    //printf("DNS lookup succeeded. IP=%s\n", inet_ntoa(*addr));

    socket_fd = socket(res->ai_family, res->ai_socktype, 0);
    if(socket_fd < 0) {
        printf("... Failed to allocate socket.\n");
        freeaddrinfo(res);
        return NULL;
    }
    printf("... allocated socket\r\n");

    if(connect(socket_fd, res->ai_addr, res->ai_addrlen) != 0) {
        printf("... socket connect failed\n");
        close(socket_fd);
        freeaddrinfo(res);
        return NULL;
    }

    printf("... connected\n");
    freeaddrinfo(res);

	/* Send headers to server */
	int sent = 0;
    int tmpres;
	while(sent < strlen(http_headers))
	{
	    tmpres = write(socket_fd, http_headers+sent, strlen(http_headers)-sent);
		if(tmpres == -1)
		{
			printf("Can't send headers");            
            close(socket_fd);
			return NULL;
		}
		sent += tmpres;
    }

    printf("... socket send success\n");

    /* Recieve HTTP response into response*/
	char *response = (char*)malloc(1);
	char BUF[BUF_SIZE];
	int recived_len = 0;
    response[0] = '\0';
	while((recived_len = read(socket_fd, BUF, BUF_SIZE-1)) > 0)
	{
        BUF[recived_len] = '\0';
		response = (char*)realloc(response, strlen(response) + strlen(BUF) + 1);
		sprintf(response, "%s%s", response, BUF);
        
        if(strlen(response)>RECV_BUF_MAX_SIZE){
            printf("recv buf is full\n");
            break;
        }
	}
    //printf("\n------------------\n%s\n------------------\n",response);
    
	if (recived_len < 0)
    {
		free(http_headers);
	    close(socket_fd);
        printf("Unabel to recieve\n");
		return NULL;
    }

	/* Reallocate response */
	response = (char*)realloc(response, strlen(response) + 1);

	/* Close socket */
	close(socket_fd);
    
    printf("close fd\n");

    
    DPRINT("debug");
    /* Parse status code and text */
	char *status_line = get_until(response, "\r\n");
    
    DPRINT("debug");
	status_line = str_replace("HTTP/1.1 ", "", status_line);
    
    DPRINT("debug");
    
	char *status_code = str_ndup(status_line, 4);
    
    DPRINT("debug");
	//status_code = str_replace(" ", "", status_code); //HTTP/1.1 response does not contain ',' anymore.   
	char *status_text = str_replace(status_code, "", status_line);
    
    DPRINT("debug");
    status_text = str_replace(" ", "", status_text);
    
    DPRINT("debug");

    hresp->status_code = status_code;
	hresp->status_code_int = atoi(status_code);
	hresp->status_text = status_text;

	/* Parse response headers */
	char *headers = get_until(response, "\r\n\r\n");
	hresp->response_headers = headers;
    
    DPRINT("debug");

	/* Assign request headers */
	hresp->request_headers = http_headers;

	/* Assign request url */
	hresp->request_uri = purl;
    DPRINT("debug");

	/* Parse body */
	char *body = strstr(response, "\r\n\r\n");
    
    DPRINT("debug");
	body = str_replace("\r\n\r\n", "", body);
    
    DPRINT("debug");
	hresp->body = body;
    
    DPRINT("debug");

	/* Return response */
	return hresp;
}

/*
	Makes a HTTP GET request to the given url
*/
http_response_t *http_get(char *url, char *custom_headers)
{
	/* Parse url */
	parsed_url_t_2  *purl = parse_url(url);
	if(purl == NULL)
	{
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*)malloc(1024);

	/* Build query/headers */
	if(purl->path != NULL)
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "GET /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "GET /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->host);
		}
	}
	else
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "GET /?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "GET / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if(purl->username != NULL)
	{
		/* Format username:password pair */
		char *upwd = (char*)malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*)realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*)malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*)realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*)realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);

        free(upwd);
        free(auth_header);
	}

	/* Add custom headers, and close */
	if(custom_headers != NULL)
	{
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
	}
	else
	{
		sprintf(http_headers, "%s\r\n", http_headers);
	}
	http_headers = (char*)realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	http_response_t *hresp = http_req(http_headers, purl);
        
	/* Handle redirect */
	return handle_redirect_get(hresp, custom_headers);
}

/*
	Makes a HTTP POST request to the given url
*/
http_response_t *http_post(char *url, char *custom_headers, char *post_data)
{
    
    DPRINT("debug");
	/* Parse url */
	parsed_url_t_2  *purl = parse_url(url);
	if(purl == NULL)
	{
		printf("Unable to parse url");
		return NULL;
	}
    
    DPRINT("debug");

	/* Declare variable */
	char *http_headers = (char*)malloc(1024);
    
    DPRINT("debug");

    printf("url: %s\n", purl->host);
    if(purl->path != NULL)
        printf("url: %s\n", purl->path);
    if(purl->query!=NULL)
        printf("query: %s\n", purl->query);


	/* Build query/headers */
	if(purl->path != NULL)
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "POST /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->path, purl->query, purl->host, strlen(post_data));
		}
		else
		{
			sprintf(http_headers, "POST /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->path, purl->host, strlen(post_data));
		}
	}
	else
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "POST /?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->query, purl->host, strlen(post_data));
		}
		else
		{
			sprintf(http_headers, "POST / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->host, strlen(post_data));
		}
	}

    DPRINT("debug");
	/* Handle authorisation if needed */
	if(purl->username != NULL)
	{
		/* Format username:password pair */
		char *upwd = (char*)malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*)realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*)malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*)realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*)realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}
    DPRINT("debug");

	if(custom_headers != NULL)
	{
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
		sprintf(http_headers, "%s\r\n%s", http_headers, post_data);
	}
	else
	{
		sprintf(http_headers, "%s\r\n%s", http_headers, post_data);
	}
	http_headers = (char*)realloc(http_headers, strlen(http_headers) + 1);

    DPRINT("debug");

	/* Make request and return response */
	http_response_t *hresp = http_req(http_headers, purl);
    DPRINT("debug");

	/* Handle redirect */
	return handle_redirect_post(hresp, custom_headers, post_data);
    
}

/*
	Makes a HTTP HEAD request to the given url
*/
http_response_t *http_head(char *url, char *custom_headers)
{
	/* Parse url */
	parsed_url_t_2  *purl = parse_url(url);
	if(purl == NULL)
	{
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*)malloc(1024);

	/* Build query/headers */
	if(purl->path != NULL)
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "HEAD /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "HEAD /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->host);
		}
	}
	else
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "HEAD/?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "HEAD / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if(purl->username != NULL)
	{
		/* Format username:password pair */
		char *upwd = (char*)malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*)realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*)malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*)realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*)realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	if(custom_headers != NULL)
	{
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
	}
	else
	{
		sprintf(http_headers, "%s\r\n", http_headers);
	}
	http_headers = (char*)realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	http_response_t *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return handle_redirect_head(hresp, custom_headers);
}

/*
	Do HTTP OPTIONs requests
*/
http_response_t *http_options(char *url)
{
	/* Parse url */
	parsed_url_t_2  *purl = parse_url(url);
	if(purl == NULL)
	{
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*)malloc(1024);

	/* Build query/headers */
	if(purl->path != NULL)
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "OPTIONS /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "OPTIONS /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->host);
		}
	}
	else
	{
		if(purl->query != NULL)
		{
			sprintf(http_headers, "OPTIONS/?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->query, purl->host);
		}
		else
		{
			sprintf(http_headers, "OPTIONS / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if(purl->username != NULL)
	{
		/* Format username:password pair */
		char *upwd = (char*)malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*)realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*)malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*)realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*)realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	/* Build headers */
	sprintf(http_headers, "%s\r\n", http_headers);
	http_headers = (char*)realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
    http_response_t *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return hresp;
}

/*
	Free memory of http_response
*/
void http_response_free(http_response_t *hresp)
{
	if(hresp != NULL)
	{
		if(hresp->request_uri != NULL) parsed_url_free_2(hresp->request_uri);
		if(hresp->body != NULL) free(hresp->body);
		if(hresp->status_code != NULL) free(hresp->status_code);
		if(hresp->status_text != NULL) free(hresp->status_text);
		if(hresp->request_headers != NULL) free(hresp->request_headers);
		if(hresp->response_headers != NULL) free(hresp->response_headers);
		free(hresp);
	}
}