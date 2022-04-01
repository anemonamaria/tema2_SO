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
	int error;
	int cursor;
	int eof;
	int fd;
	int rdwr;
	int charRead;
	int pid;
	char buffer[BUFSIZE];
} SO_FILE;

int so_fflush(SO_FILE *stream) {
	if (stream->rdwr == 2) {
		int rc = 0, p = 0;

		if(!stream)
			return SO_EOF;



		while ((stream->cursor -= rc) > 0) {
			if ((rc = write(stream->fd, stream->buffer + p, stream->cursor)) < 0)
				return SO_EOF;
			p += rc;
		}
		return 0;
	}
	return 0;

}

int so_fseek(SO_FILE *stream, long offset, int whence) {
	int ret = 0;

	if(!stream)
		return SO_EOF;

	so_fflush(stream);

	if (stream->rdwr == 2) {
		stream->cursor = stream->cursor + lseek(stream->fd, 0, SEEK_CUR);
	}
	else
		stream->cursor = BUFSIZE;

	if ((ret = lseek(stream->fd, offset, whence)) < 0)
		return -1;
	stream->cursor = 0;


	return 0;
}

long so_ftell(SO_FILE *stream) {
	long ret;

	if(!stream)
		return SO_EOF;

	if ((ret = lseek(stream->fd, 0, SEEK_CUR)) < 0) {
		stream->error = 1;
		return SO_EOF;
	}

	if (stream->rdwr == 2)
		return ret + stream->cursor;
	else if (stream->rdwr == 1)
		return ret + (stream->cursor - BUFSIZE);
	return stream->cursor;
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
	int rc, ret;

	if(so_feof(stream) == 1)
		return SO_EOF;

	if (stream->cursor == stream->charRead || stream->cursor == 0) {
		stream->cursor = 0;
		rc = read(stream->fd, &stream->buffer, BUFSIZE);
		if (rc == 0) {
			stream->eof = 1;
			return SO_EOF;
		} else if (rc == -1) {
			stream->error = 1;
			return SO_EOF;
		} else {
			stream->charRead = rc;
			stream->rdwr = 1;
		}
	}
	ret = (unsigned char) stream->buffer[stream->cursor];
	stream->cursor = stream->cursor + 1;
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
	f->eof = 0;
	f->error = 0;
	f->cursor = 0;
	f->charRead = 0;
	f->rdwr = 0;

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
	int rc, size = 0;
	char *aux;

	if(so_feof(stream) == 1)
		return SO_EOF;
	if (stream->cursor == BUFSIZE) {
		rc = so_fflush(stream);
		if(rc == SO_EOF)
			return SO_EOF;
	}
	stream->buffer[stream->cursor] = c;
	stream->cursor++;
	stream->rdwr = 2;
	return c;
}

int so_feof(SO_FILE *stream) {
	return stream->eof;
}

int so_ferror(SO_FILE *stream) {
	return stream->error;
}

// todo
SO_FILE *so_popen(const char *command, const char *type) {
	pid_t pid;
	int fd, wr = 0, rc = 0;
	SO_FILE *f;
	int fileDes[2];

	if (strcmp(type, "w") == 0)
		wr = 1;
	rc = pipe(fileDes);
	if (rc < 0)
		return NULL;
	pid = fork();
	switch (pid) {
	case -1:
		return NULL;
	case 0:
		if (wr) {
			dup2(fileDes[0], 0);
			close(fileDes[1]);
		} else {
			dup2(fileDes[1], 1);
			close(fileDes[0]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(-1);
	default:
		if (wr) {
			fd = fileDes[1];
			close(fileDes[0]);
		} else {
			fd = fileDes[0];
			close(fileDes[1]);
		}
		break;
	}
	f = malloc(sizeof(SO_FILE));
	f->fd = fd;
	f->pid = pid;
	f->eof = 0;
	f->error = 0;
	f->cursor = 0;
	f->charRead = 0;
	f->rdwr = 0;
	return f;
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

int so_fileno(SO_FILE *stream) {
	return stream->fd;
}