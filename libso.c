#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "so_stdio.h"

#define BUFSIZE 4096

typedef struct _so_file {
	int fd;
	int pid;
	char buffer[BUFSIZE];
	int lastOperation;
	int bytesRead;
	int filePos;
	int err;
	int eof;
} SO_FILE;

int so_fflush(SO_FILE *stream) {
	if (stream->lastOperation == 2) {
		int ret = 0, pos = 0;

		if(!stream)
			return SO_EOF;

		while ((stream->filePos -= ret) > 0) {
			if ((ret = write(stream->fd, stream->buffer + pos, stream->filePos)) < 0)
				return SO_EOF;
			pos += ret;
		}
		return 0;
	}
	return 0;

}

int so_fseek(SO_FILE *stream, long offset, int whence) {
	if(!stream)
		return SO_EOF;

	so_fflush(stream);

	if (stream->lastOperation == 2) {
		stream->filePos = stream->filePos + lseek(stream->fd, 0, SEEK_CUR);
	}
	else
		stream->filePos = BUFSIZE;

	if (lseek(stream->fd, offset, whence) < 0)
		return -1;
	stream->filePos = 0;


	return 0;
}

long so_ftell(SO_FILE *stream) {
	long ret;

	if(!stream)
		return SO_EOF;

	if ((ret = lseek(stream->fd, 0, SEEK_CUR)) < 0) {
		stream->err = 1;
		return SO_EOF;
	}

	if (stream->lastOperation == 2)
		return ret + stream->filePos;
	else if (stream->lastOperation == 1)
		return ret + (stream->filePos - BUFSIZE);
	return stream->filePos;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	size_t i, j;
	int aux;
	unsigned char *p = ptr;

	if(!stream)
		return SO_EOF;

	for (i = 0; i < nmemb; i++)
		for (j = 0; j < size; j++) {
			if ((aux = so_fgetc(stream)) != SO_EOF)
				*p = aux;
			else if (i != 0)
				break;
			else
				return 0;
			p++;
		}
	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	int i, j;
	unsigned char *p = ptr;

	if(!stream)
		return SO_EOF;

	for (i = 0; i < nmemb; i++)
		for (j = 0; j < size; j++) {
			if (so_fputc(*p, stream) == SO_EOF)
				return SO_EOF;
			else
				p++;
		}
	return nmemb;
}

int so_fgetc(SO_FILE *stream) {
	int rd, ret;

	if(so_feof(stream) == 1)
		return SO_EOF;

	if (stream->filePos == stream->bytesRead || stream->filePos == 0) {
		stream->filePos = 0;
		rd = read(stream->fd, &stream->buffer, BUFSIZE);
		if (rd == 0) {
			stream->eof = 1;
			return SO_EOF;
		} else if (rd == -1) {
			stream->err = 1;
			return SO_EOF;
		} else {
			stream->bytesRead = rd;
			stream->lastOperation = 1;
		}
	}
	ret = (unsigned char) stream->buffer[stream->filePos];
	stream->filePos = stream->filePos + 1;
	return ret;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *f;

	if(strcmp(pathname, "") == 0)
		return NULL;

	f = malloc(sizeof(SO_FILE));

	if(!f)
		return NULL;

	f->fd = -1;
	f->bytesRead = 0;
	f->filePos = 0;
	f->lastOperation = 0;
	f->eof = 0;
	f->err = 0;

	if (mode != NULL) {
		if (strcmp(mode, "r") == 0)
			f->fd = open(pathname, O_RDONLY);
		if (strcmp(mode, "r+") == 0)
			f->fd = open(pathname, O_RDWR);
		if (strcmp(mode, "w") == 0)
			f->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (strcmp(mode, "w+") == 0)
			f->fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (strcmp(mode, "a") == 0)
			f->fd = open(pathname, O_APPEND | O_WRONLY, 0644);
		if (strcmp(mode, "a+") == 0)
			f->fd = open(pathname, O_APPEND | O_RDWR, 0644);
		if (f->fd < 0) {
			free(f);
			return NULL;
		}
	}

	return f;
}

int so_fputc(int c, SO_FILE *stream) {
	int ret;

	if(so_feof(stream) == 1)
		return SO_EOF;
	if (stream->filePos == BUFSIZE) {
		ret = so_fflush(stream);
		if(ret == SO_EOF)
			return SO_EOF;
	}
	stream->lastOperation = 2;
	stream->buffer[stream->filePos] = c;
	stream->filePos++;
	return c;
}

int so_feof(SO_FILE *stream) {
	return stream->eof;
}

int so_ferror(SO_FILE *stream) {
	return stream->err;
}

SO_FILE *so_popen(const char *command, const char *type) {
	int fd, argument;
	pid_t pid;
	SO_FILE *f;
	int channel[2];

	if (pipe(channel) < 0)
		return NULL;

	pid = fork();

	switch (pid) {
		case -1:
			return NULL;
		case 0:
			if (strcmp(type, "w") == 0) {
				argument = 0;
			} else {
				argument = 1;
			}
			dup2(channel[argument], argument);
			close(channel[1 - argument]);
			execl("/bin/sh", "sh", "-c", command, NULL);
			exit(-1);
		default:
			if (strcmp(type, "w") == 0) {
				argument = 1;
			} else {
				argument = 0;
			}
			fd = channel[argument];
			close(channel[1 - argument]);
			break;
	}

	f = malloc(sizeof(SO_FILE));

	if(!f)
		return NULL;

	f->fd = fd;
	f->pid = pid;
	f->eof = 0;
	f->err = 0;
	f->filePos = 0;
	f->bytesRead = 0;
	f->lastOperation = 0;

	return f;
}

int so_fileno(SO_FILE *stream) {
	return stream->fd;
}

int so_fclose(SO_FILE *stream)
{
	int ret, ret2;

	if(!stream)
		return SO_EOF;

	ret2 = so_fflush(stream);

	if((ret = close(stream->fd)) < 0) {
		free(stream);
		return SO_EOF;
	}

	free(stream);

	if (ret2 == SO_EOF)
		return SO_EOF;

	return ret;
}

int so_pclose(SO_FILE *stream) {
	int status = -1;
	pid_t pid = stream->pid;

	if(!stream)
		return SO_EOF;

	so_fflush(stream);

	if (so_fclose(stream) == SO_EOF)
		return SO_EOF;

	if (waitpid(pid, &status, 0) < 0)
		return SO_EOF;

	return WEXITSTATUS(status);
}