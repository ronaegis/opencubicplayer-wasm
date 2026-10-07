/* Build CPMDLAND.DAT with the player's own catalog parser and saver.
 * Compile this on the host, not with emcc. The browser loads the result.
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../filesel/modland.com/modland-com.c"

struct configAPI_t configAPI;

static int parse_listing(FILE *in, struct modland_com_initialize_t *state)
{
	char *line = 0;
	size_t cap = 0;
	ssize_t got;
	unsigned long lines = 0;

	while ((got = getline(&line, &cap, in)) != -1)
	{
		char *end;
		long filesize;

		while (got > 0 && (line[got - 1] == '\n' || line[got - 1] == '\r'))
		{
			line[--got] = 0;
		}
		if (!got)
		{
			continue;
		}
		filesize = strtol(line, &end, 10);
		if (end == line || filesize <= 0)
		{
			continue;
		}
		while (*end == '\t' || *end == ' ')
		{
			end++;
		}
		if (modland_com_add_data_line(state, end, filesize))
		{
			free(line);
			return -1;
		}
		lines++;
		if ((lines % 100000) == 0)
		{
			fprintf(stderr, "modland-makedb: %lu lines\n", lines);
		}
	}
	free(line);
	fprintf(stderr, "modland-makedb: parsed %lu lines\n", lines);
	return ferror(in) ? -1 : 0;
}

int main(int argc, char **argv)
{
	struct modland_com_initialize_t state = {0};
	struct configAPI_t config;
	FILE *in;
	int year, month, day;
	unsigned int files;

	if (argc != 6)
	{
		fprintf(stderr, "usage: %s allmods.txt data-dir/ year month day\n", argv[0]);
		return 1;
	}
	year = atoi(argv[3]);
	month = atoi(argv[4]);
	day = atoi(argv[5]);
	if (year < 1980 || month < 1 || month > 12 || day < 1 || day > 31)
	{
		fprintf(stderr, "modland-makedb: bad date %s-%s-%s\n", argv[3], argv[4], argv[5]);
		return 1;
	}
	if (argv[2][strlen(argv[2]) - 1] != '/')
	{
		fprintf(stderr, "modland-makedb: data directory must end with /\n");
		return 1;
	}

	in = fopen(argv[1], "rb");
	if (!in)
	{
		perror(argv[1]);
		return 1;
	}
	if (parse_listing(in, &state))
	{
		fprintf(stderr, "modland-makedb: parse failed\n");
		fclose(in);
		return 1;
	}
	fclose(in);

	modland_com.database.year = year;
	modland_com.database.month = month;
	modland_com.database.day = day;
	if (modland_com_sort())
	{
		fprintf(stderr, "modland-makedb: sort failed\n");
		return 1;
	}

	memset(&config, 0, sizeof(config));
	config.DataHomePath = argv[2];
	{
		char existing[1024];
		int save_result;
		if (snprintf(existing, sizeof(existing), "%sCPMDLAND.DAT", argv[2]) >= (int)sizeof(existing))
		{
			fprintf(stderr, "modland-makedb: output path is too long\n");
			return 1;
		}
		unlink(existing);
		modland_com_filedb_load(&config);
		if (modland_com_filedb_save_start())
		{
			fprintf(stderr, "modland-makedb: save start failed\n");
			return 1;
		}
		do
		{
			save_result = modland_com_filedb_save_iterate();
		} while (save_result == 1);
		if (save_result != 0)
		{
			fprintf(stderr, "modland-makedb: save failed\n");
			return 1;
		}
	}
	files = modland_com.database.fileentries_n;
	modland_com_filedb_close();

	memset(&modland_com.database, 0, sizeof(modland_com.database));
	if (!modland_com_filedb_load(&config) || modland_com_sort() || modland_com.database.fileentries_n != files || !files)
	{
		fprintf(stderr, "modland-makedb: reload check failed (%u files)\n", modland_com.database.fileentries_n);
		return 1;
	}
	fprintf(stderr, "modland-makedb: wrote %sCPMDLAND.DAT (%u files, %u dirs, %d skipped)\n",
		argv[2], files, modland_com.database.direntries_n, state.invalid_entries);
	return 0;
}
