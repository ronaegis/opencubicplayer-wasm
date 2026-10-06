/* OpenCP Module Player - WASM Download Wrapper
 * Copyright (c) 2025 - WASM port using Emscripten fetch API
 *
 * This is a WASM-compatible implementation of filesel/download.c
 * It replaces curl process spawning with Emscripten's fetch API
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <emscripten/fetch.h>
#include "../types.h"

#include "../filesel/download.h"
#include "../boot/psetting.h"
#include "../filesel/dirdb.h"
#include "../filesel/filesystem.h"
#include "../filesel/filesystem-drive.h"
#include "../filesel/filesystem-textfile.h"

/* Extended download_request_t with emscripten_fetch handle */
struct wasm_download_request_t
{
	struct download_request_t base;
	emscripten_fetch_t *fetch;
	int fetch_done;
	int fetch_success;
};

static struct ocpfilehandle_t *download_request_resolve (struct download_request_t *req, const char *filename);

struct download_request_t *download_request_spawn (const struct configAPI_t *configAPI, void *tag, const char *URL)
{
	static unsigned int sequence = 0;
	struct wasm_download_request_t *req = calloc (sizeof (*req), 1);
	size_t l1, l2, l3, l4;

	if (!req)
	{
		return 0;
	}

	req->base.tag = tag;
	req->base.configAPI = configAPI;
	req->base.httpcode = -1;
	req->base.errcode = -1;

	req->base.tempheader_filename = malloc (l1 = (20 + 20 + 21));
	req->base.tempdata_filename   = malloc (l2 = (20 + 20 + 19));
	req->base.tempheader_filepath = malloc (l3 = (strlen (configAPI->TempPath) + 20 + 20 + 21));
	req->base.tempdata_filepath   = malloc (l4 = (strlen (configAPI->TempPath) + 20 + 20 + 19));

	if ((!req->base.tempheader_filename) || (!req->base.tempdata_filename) || (!req->base.tempheader_filepath) || (!req->base.tempdata_filepath))
	{
		free (req->base.tempheader_filename);
		free (req->base.tempdata_filename);
		free (req->base.tempheader_filepath);
		free (req->base.tempdata_filepath);
		free (req);
		return 0;
	}

	snprintf (req->base.tempheader_filename, l1, "ocp-headertemp-%d-%d.txt", getpid(), ++sequence);
	snprintf (req->base.tempdata_filename,   l2, "ocp-datatemp-%d-%d.dat",   getpid(),   sequence);
	snprintf (req->base.tempheader_filepath, l3, "%s%s", configAPI->TempPath, req->base.tempheader_filename);
	snprintf (req->base.tempdata_filepath,   l4, "%s%s", configAPI->TempPath, req->base.tempdata_filename);

	/* Initialize emscripten_fetch attributes */
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);

	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE;
	/* Browsers block custom User-Agent headers; rely on default UA and skip IndexedDB caching so we don't trip false 404s. */

	/* Store the request pointer so we can access it in the callback */
	attr.userData = req;

	/* Spawn the fetch request */
	req->fetch = emscripten_fetch(&attr, URL);
	if (!req->fetch)
	{
		free (req->base.tempheader_filename);
		free (req->base.tempdata_filename);
		free (req->base.tempheader_filepath);
		free (req->base.tempdata_filepath);
		free (req);
		return 0;
	}

	req->fetch_done = 0;
	req->fetch_success = 0;

	fprintf(stderr, "WASM: Download started for URL: %s\n", URL);

	return &req->base;
}

static int parse_http_date (const char *input, unsigned int *_Year, unsigned char *_Month, unsigned char *_Day, unsigned char *_Hour, unsigned char *_Minute, unsigned char *_Second)
{
	char *Date = (char *)input; // strtol endptr is non-const
	long Year, Month, Day, Hour, Minute, Second;

	while (*Date == ' ') Date++;

	if (strlen (Date) < 5) return -1;
	if (Date[3] == ',') /* IMF-fixdate, "Sun, 06 Nov 1994 08:49:37 GMT" */
	{
		Date += 4;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		if (!strncasecmp (Date, "Jan ", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb ", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar ", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr ", 4)) Month =  4; else
		if (!strncasecmp (Date, "May ", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun ", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul ", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug ", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep ", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct ", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov ", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec ", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0] == ' ') Date++;
		Year = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
	} else if (Date[3] == ' ') /* ANSI C's asctime() format, "Sun Nov  6 08:49:37 1994" */
	{
		while (Date[0] && (Date[0] != ' ')) Date++;
		if (Date[0] == ' ') Date++;
		if (!strncasecmp (Date, "Jan ", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb ", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar ", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr ", 4)) Month =  4; else
		if (!strncasecmp (Date, "May ", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun ", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul ", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug ", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep ", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct ", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov ", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec ", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		while (Date[0] == ' ') Date++;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Year = strtol (Date, &Date, 10);
	} else { /* RFC 850 format, "Sunday, 06-Nov-94 08:49:37 GMT" */
		while (Date[0] && (Date[0] != ' ')) Date++;
		if (Date[0] == ' ') Date++;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == '-') Date++;
		if (!strncasecmp (Date, "Jan-", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb-", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar-", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr-", 4)) Month =  4; else
		if (!strncasecmp (Date, "May-", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun-", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul-", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug-", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep-", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct-", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov-", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec-", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0] == '-') Date++;
		Year = strtol (Date, &Date, 10);
		if ((Year >= 0) && (Year < 100)) Year += 1900;
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
	}

	if (Year < 1970) return -1;
	if ((Day < 1) || (Day > 31)) return -1;
	if ((Hour < 0) || (Hour >= 24)) return -1;
	if ((Minute < 0) || (Minute >= 60)) return -1;
	if ((Second < 0) || (Second > 60)) return -1; /* do we care about leap-seconds ? */

	*_Year = Year;
	*_Month = Month;
	*_Day = Day;
	*_Hour = Hour;
	*_Minute = Minute;
	*_Second = Second;

	return 0;
}

static int download_parse_header_textfile (struct download_request_t *req, struct textfile_t *textfile)
{
	const char *line;

	int fresh_request = 1;

	while ((line = textfile_fgets (textfile)))
	{	/* header can contain multiple header sequences, we want the last one */

		if (!strlen (line))
		{
			fresh_request = 1;
		} else if (fresh_request)
		{
			char *end;
			long newcode;

			/* line should start with HTTP/ */
			if (strncmp (line, "HTTP/", 5))
			{
				req->errmsg = "invalid HTTP header syntax (1)";
				return -1;
			}

			/* contain version and the a space */
			line = strchr (line, ' ');
			if (!line)
			{
				req->errmsg = "invalid HTTP header syntax (2)";
				return -1;
			}
			line++;

			/* before ending with number */
			newcode = strtol (line, &end, 10);
			if (end == line)
			{
				req->errmsg = "invalid HTTP header syntax (3)";
				return -1;
			}

			if ((newcode <= 100) && (newcode >= 600))
			{
				req->errmsg = "invalid HTTP header syntax (4)";
				return -1;
			}
			req->httpcode = newcode;
			fresh_request = 0;
		} else if (!strncasecmp ("Last-Modified: ", line, 15))
		{
			if (parse_http_date (line + 15, &req->Year, &req->Month, &req->Day, &req->Hour, &req->Minute, &req->Second))
			{
				req->errmsg = "Failed to parse HTTP header date";
				return -1;
			}
		} else if (!strncasecmp ("content-length: ", line, 16))
		{
			req->ContentLength = strtoul (line + 16, 0, 10);
		}
	}

	switch (req->httpcode)
	{
		case 200:
			break;
		case 400: req->errmsg = "400 - Bad Request";              return -1;
		case 401: req->errmsg = "401 - Unauthorized";             return -1;
		case 403: req->errmsg = "403 - Forbidden";                return -1;
		case 404: req->errmsg = "404 - Not found";                return -1;
		case 408: req->errmsg = "408 - Request Timeout";          return -1;
		case 426: req->errmsg = "426 - Upgrade Required";         return -1;
		case 429: req->errmsg = "429 - Too Many Requests";        return -1;
		case 500: req->errmsg = "500 - Internal Server Error";    return -1;
		case 502: req->errmsg = "502 - Bad Gateway";              return -1;
		case 503: req->errmsg = "503 - Service Unavailable";      return -1;
		case 504: req->errmsg = "504 - Gateway Timeout";          return -1;
		case 521: req->errmsg = "512 - Web Server Is Down";       return -1;
		case 522: req->errmsg = "522 - Connection Timed Out";     return -1;
		case 523: req->errmsg = "523 - Origin Is Unreachable";    return -1;
		case 524: req->errmsg = "524 - A Timeout Occurred";       return -1;
		default:  req->errmsg = "Not the expected 200 HTTP code"; return -1;
	}
	return 0;
}

static int download_parse_header (struct download_request_t *req)
{
	struct ocpfilehandle_t *filehandle;
	struct textfile_t *textfile;
	int retval;

	filehandle = download_request_resolve (req, req->tempheader_filename);
	if (!filehandle)
	{
		req->errmsg = "Unable to open file";
		return -1;
	}

	textfile = textfile_start (filehandle);
	filehandle->unref (filehandle);
	filehandle = 0;

	if (!textfile)
	{
		req->errmsg = "failed to hand over HTTP header file";
		return -1;
	}

	retval = download_parse_header_textfile (req, textfile);

	textfile_stop (textfile);
	textfile = 0;

	return retval;
}

int download_request_iterate (struct download_request_t *_req)
{
	struct wasm_download_request_t *req = (struct wasm_download_request_t *)_req;

	if ((!req) || (!req->fetch))
	{
		return 0;
	}

	/* Check if fetch is still in progress */
	if (!req->fetch_done)
	{
		/* Update content length from fetch progress */
		if (req->fetch->dataOffset > 0)
		{
			req->base.ContentLength = req->fetch->dataOffset;
		}

		/* Check if fetch is complete (readyState 4 = DONE) */
		if (req->fetch->readyState == 4)
		{
			req->fetch_done = 1;

			/* Check HTTP status */
			if (req->fetch->status == 200)
			{
				req->base.httpcode = 200;
				req->base.ContentLength = req->fetch->numBytes;
				req->fetch_success = 1;

				/* Write data to temp file */
				int fd = open(req->base.tempdata_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (fd >= 0)
				{
					write(fd, req->fetch->data, req->fetch->numBytes);
					close(fd);

					/* Mark data directory for IDBFS sync (modland cache) */
					extern void idbfs_mark_dirty_data(void);
					idbfs_mark_dirty_data();
				}
				else
				{
					req->base.errmsg = "Failed to write downloaded data to temp file";
					req->fetch_success = 0;
				}

				/* Write headers to temp file */
				FILE *hf = fopen(req->base.tempheader_filepath, "w");
				if (hf)
				{
					fprintf(hf, "HTTP/1.1 %d OK\n", (int)req->fetch->status);

					/* Extract headers from fetch */
					size_t headersLength = emscripten_fetch_get_response_headers_length(req->fetch);
					if (headersLength > 0)
					{
						char *headersText = malloc(headersLength + 1);
						if (headersText)
						{
							emscripten_fetch_get_response_headers(req->fetch, headersText, headersLength + 1);

							char **headers = emscripten_fetch_unpack_response_headers(headersText);
							if (headers)
							{
								for (size_t i = 0; headers[i]; i += 2)
								{
									if (headers[i] && headers[i+1])
									{
										fprintf(hf, "%s: %s\n", headers[i], headers[i+1]);

										/* Parse Last-Modified header */
										if (!strcasecmp(headers[i], "Last-Modified"))
										{
											parse_http_date(headers[i+1], &req->base.Year, &req->base.Month, &req->base.Day, &req->base.Hour, &req->base.Minute, &req->base.Second);
										}
										/* Content-Length is already in req->base.ContentLength */
									}
								}

								emscripten_fetch_free_unpacked_response_headers(headers);
							}

							free(headersText);
						}
					}

					fclose(hf);
				}

				fprintf(stderr, "WASM: Download complete - %lu bytes\n", (unsigned long)req->fetch->numBytes);
			}
			else
			{
				/* HTTP error */
				req->base.httpcode = req->fetch->status;
				req->base.errcode = req->fetch->status;

				switch (req->fetch->status)
				{
					case 400: req->base.errmsg = "400 - Bad Request";              break;
					case 401: req->base.errmsg = "401 - Unauthorized";             break;
					case 403: req->base.errmsg = "403 - Forbidden";                break;
					case 404: req->base.errmsg = "404 - Not found";                break;
					case 408: req->base.errmsg = "408 - Request Timeout";          break;
					case 429: req->base.errmsg = "429 - Too Many Requests";        break;
					case 500: req->base.errmsg = "500 - Internal Server Error";    break;
					case 502: req->base.errmsg = "502 - Bad Gateway";              break;
					case 503: req->base.errmsg = "503 - Service Unavailable";      break;
					case 504: req->base.errmsg = "504 - Gateway Timeout";          break;
					default:  req->base.errmsg = "HTTP error";                     break;
				}

				fprintf(stderr, "WASM: Download failed - HTTP %d: %s\n", (int)req->fetch->status, req->base.errmsg);
			}
		}

		/* Still in progress */
		if (!req->fetch_done)
		{
			return 1;
		}
	}

	/* Fetch is done, return 0 */
	return 0;
}

static void download_request_real_free (struct download_request_t *_req)
{
	struct wasm_download_request_t *req = (struct wasm_download_request_t *)_req;

	/* Clean up fetch handle */
	if (req->fetch)
	{
		emscripten_fetch_close(req->fetch);
		req->fetch = 0;
	}

	/* Delete temp files */
	unlink (req->base.tempheader_filepath);
	unlink (req->base.tempdata_filepath);

	free (req->base.tempheader_filename);
	free (req->base.tempdata_filename);
	free (req->base.tempheader_filepath);
	free (req->base.tempdata_filepath);
	free (req);
}

void download_request_free (struct download_request_t *_req)
{
	struct wasm_download_request_t *req = (struct wasm_download_request_t *)_req;

	if (!req)
	{
		return;
	}

	if (req->base.wrapfilehandles)
	{
		req->base.free++;
		return;
	} else {
		download_request_real_free (_req); /* delay until files are no longer in use */
	}
}

struct download_wrap_ocpfilehandle_t
{
	struct ocpfilehandle_t head;
	struct ocpfilehandle_t *filehandle;
	struct download_request_t *owner;
};
static void download_wrap_ocpfilehandle_ref (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	f->head.refcount++;
}
static void download_wrap_ocpfilehandle_unref (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	if (!(--f->head.refcount))
	{
		f->head.origin->unref (f->head.origin);
		f->head.origin = 0;

		f->filehandle->unref (f->filehandle);
		f->filehandle = 0;

		f->owner->wrapfilehandles--;
		if (f->owner->free)
		{
			download_request_free (f->owner);
		}
		f->owner = 0;

		free (f);
	}
}
static int download_wrap_ocpfilehandle_seek_set (struct ocpfilehandle_t *_f, int64_t pos)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->seek_set (f->filehandle, pos);
}
static uint64_t download_wrap_ocpfilehandle_getpos (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->getpos (f->filehandle);
}
static int download_wrap_ocpfilehandle_eof (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->eof (f->filehandle);
}
static int download_wrap_ocpfilehandle_error (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->error (f->filehandle);
}
static int download_wrap_ocpfilehandle_read (struct ocpfilehandle_t *_f, void *dst, int len)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->read (f->filehandle, dst, len);
}
static int download_wrap_ocpfilehandle_ioctl (struct ocpfilehandle_t *_f, const char *cmd, void *ptr)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->ioctl (f->filehandle, cmd, ptr);
}
static uint64_t download_wrap_ocpfilehandle_filesize (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filesize (f->filehandle);
}
static int download_wrap_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filesize_ready (f->filehandle);
}
static const char *download_wrap_ocpfilehandle_filename_override (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filename_override (f->filehandle);
}
static struct ocpfilehandle_t *download_request_resolve (struct download_request_t *req, const char *filename)
{
	uint32_t headerfile_ref;
	struct ocpfile_t *file = 0;
	struct ocpfilehandle_t *filehandle = 0;
	struct download_wrap_ocpfilehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		return 0;
	}

	headerfile_ref = dirdbFindAndRef (req->configAPI->TempDir->dirdb_ref, filename, dirdb_use_file);
	file = req->configAPI->TempDir->readdir_file (req->configAPI->TempDir, headerfile_ref);
	dirdbUnref (headerfile_ref, dirdb_use_file);

	if (!file)
	{
		free (retval);
		return 0;
	}
	filehandle = file->open (file);
	if (!filehandle)
	{
		free (retval);
		return 0;
	}
	ocpfilehandle_t_fill
	(
		&retval->head,
		download_wrap_ocpfilehandle_ref,
		download_wrap_ocpfilehandle_unref,
		file, /* takes over the ref count */
		download_wrap_ocpfilehandle_seek_set,
		download_wrap_ocpfilehandle_getpos,
		download_wrap_ocpfilehandle_eof,
		download_wrap_ocpfilehandle_error,
		download_wrap_ocpfilehandle_read,
		download_wrap_ocpfilehandle_ioctl,
		download_wrap_ocpfilehandle_filesize,
		download_wrap_ocpfilehandle_filesize_ready,
		download_wrap_ocpfilehandle_filename_override,
		filehandle->dirdb_ref,
		1
	);
	retval->filehandle = filehandle;
	retval->owner = req;
	retval->owner->wrapfilehandles++;
	return &retval->head;
}

struct ocpfilehandle_t *download_request_getfilehandle (struct download_request_t *_req)
{
	struct wasm_download_request_t *req = (struct wasm_download_request_t *)_req;

	if (!req)
	{
		return 0;
	}
	if (!req->fetch_done)
	{
		return 0;
	}
	if (!req->fetch_success)
	{
		return 0;
	}
	return download_request_resolve (&req->base, req->base.tempdata_filename);
}

void download_request_cancel (struct download_request_t *_req)
{
	struct wasm_download_request_t *req = (struct wasm_download_request_t *)_req;

	if ((!req) || (!req->fetch))
	{
		return;
	}

	/* Close the fetch to cancel it */
	emscripten_fetch_close(req->fetch);
	req->fetch = 0;
	req->fetch_done = 1;
	req->fetch_success = 0;
}

char *urlencode(const char *src)
{
	const char *h = "0123456789abcdef";
	char *retval = malloc (strlen (src) * 3 + 1);
	const char *s;
	char *d;

	if (!retval)
	{
		return 0;
	}

	for (d = retval, s = src; *s; s++)
	{
		if ( ((*s >= '0') && (*s <= '9')) ||
		     ((*s >= 'a') && (*s <= 'z')) ||
		     ((*s >= 'A') && (*s <= 'Z')) ||
		     (*s == '/') )
		{
			*d = *s; d++;
		} else {
			*d = '%'; d++;
			*d = h[(*(unsigned char *)s) >> 4]; d++;
			*d = h[(*(unsigned char *)s) & 15]; d++;
		}
	}
	*d = 0;
	return retval;
}
