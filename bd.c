/*
Copyright (c) 2024, Daniel Austin MBCS <me@dan.me.uk>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

/* globals */
typedef struct {
	uint32_t		year;
	uint32_t		month;
	uint32_t		day;
	time_t			ts;
	time_t			delta;
	char			days_to_go[128];
	char			next_bday[16];
	char			name[128];
	int			age;
	struct bd_entry_t	*next;
} bd_entry_t;

bd_entry_t	*bd_entry_head = NULL;
char		config_file[256];

/* functions */
void Usage(char *progname)
{
	printf("Usage: %s [-h] [-config ~/.bd.conf] [[-add dd/mm/yyyy \"name\" | -remove \"name\"] ...]\n\n",
		progname);
	printf("\t-h				Show this help\n");
	printf("\t-config ~/.bd.conf		Use configuration file (default = ~/.bd.conf)\n");
	printf("\t-add    dd/mm/yyyy \"name\"	Add a birthday on dd/mm/yyyy for name\n");
	printf("\t-remove \"name\"		Remove a birthday for name\n");
	exit(0);
}

void ShowError(char *fmt, ...)
{
	va_list		va;

	va_start(va, fmt);
	printf("ERROR: ");
	vprintf(fmt, va);
	printf("\n");
	va_end(va);
	exit(1);
}

void FixupConfigFile(void)
{
	char	*c, *home;
	char	buf[256];

	if (config_file[0] == '\0')
		ShowError("No configuration file specified");
	if (config_file[0] == '~' && config_file[1] == '/')
	{
		/* find home dir */
		home = getenv("HOME");
		c = config_file;
		c++;
		snprintf(buf, sizeof(buf), "%s%s",
			home, c);
		snprintf(config_file, sizeof(config_file), "%s", buf);
	}
}

void FriendlyDelta(char *buf, int len, uint32_t delta)
{
	int	total_days, days, weeks;

	total_days = days = (int)(delta / 86400) + 1;
	weeks = (int)(days / 7);
	days -= (7 * weeks);
	if (total_days >= 7)
		snprintf(buf, len, "%2d week%s,%s %2d day%s (%3d day%s)",
			weeks, (weeks==1)?"":"s",
			(weeks==1)?" ":"",
			days, (days==1)?" ":"s",
			total_days, (total_days==1)?"":"s");
	else
		snprintf(buf, len, "          %2d day%s (%3d day%s)",
			days, (days==1)?" ":"s",
			total_days, (total_days==1)?" ":"s");
}

void InsertBirthday(bd_entry_t *entry)
{
	bd_entry_t	*prev, *cur;

	entry->next = NULL;
	if (!bd_entry_head)
	{
		/* first entry */
		bd_entry_head = entry;
		return;
	}
	prev = NULL;
	cur = bd_entry_head;
	while (cur)
	{
		if (entry->delta < cur->delta)
		{
			/* insert before this entry */
			entry->next = (struct bd_entry_t *)cur;
			if (prev)
			{
				prev->next = (struct bd_entry_t *)entry;
				return;
			} else {
				/* insert at the head */
				bd_entry_head = entry;
				return;
			}
		}
		prev = cur;
		cur = (bd_entry_t *)cur->next;
	}
	if (prev)
	{
		prev->next = (struct bd_entry_t *)entry;
		return;
	} else {
		printf("error - shouldnt reach here (1)\n");
	}
}

void AddBirthday(char *dob, char *name)
{
	uint32_t	day, month, year;
	char		*s_day, *s_month, *s_year;
	FILE		*fp;

	s_day = dob;
	s_month = strchr(dob, '/');
	if (!s_month)
		ShowError("Malformed date provided");
	*s_month = '\0';
	s_month++;
	s_year = strchr(s_month, '/');
	if (!s_year)
		ShowError("Malformed date provided");
	*s_year = '\0';
	s_year++;
	day = strtoul(s_day, NULL, 10);
	month = strtoul(s_month, NULL, 10);
	year = strtoul(s_year, NULL, 10);
	if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900)
		ShowError("Malformed date provided");
	FixupConfigFile();
	fp = fopen(config_file, "a");
	if (!fp)
		ShowError("Unable to open configuration file for writing [%s]", config_file);
	fprintf(fp, "%d\t%d\t%d\t%s\n", year, month, day, name);
	fclose(fp);
	printf("Added: %02d/%02d/%04d %s\n", day, month, year, name);
}

void RemoveBirthday(char *name)
{
	uint32_t	day, month, year;
	char		*s_day, *s_month, *s_year, *s_name, *c;
	char		tmpfile[256];
	char		buf[1024];
	FILE		*fp, *fp2;
	bool		found = false;

	FixupConfigFile();
	fp = fopen(config_file, "r");
	if (!fp)
		ShowError("Unable to open configuration file for reading [%s]", config_file);
	
	snprintf(tmpfile, sizeof(tmpfile), "%s.XXXXXXXX", config_file);
	c = mktemp(tmpfile);
	if (!c)
	{
		fclose(fp);
		ShowError("Unable to create temporary file [%s]", tmpfile);
	}
	fp2 = fopen(tmpfile, "w");
	if (!fp2)
	{
		fclose(fp);
		ShowError("Unable to create temporary file");
	}
	while (!feof(fp))
	{
		if (!fgets(buf, sizeof(buf), fp))
			continue;
		c = strchr(buf, '\r');
		if (c)
			*c = '\0';
		c = strchr(buf, '\n');
		if (c)
			*c = '\0';
		if (buf[0] == '#' || buf[0] == '\0')
		{
			/* blank line or comment */
			fprintf(fp2, "%s\n", buf);
			continue;
		}
		s_year = strtok(buf, "\t");
		s_month = strtok(NULL, "\t");
		s_day = strtok(NULL, "\t");
		s_name = strtok(NULL, "\t");
		if (!s_year || !s_month || !s_day || !s_name)
		{
			fclose(fp2);
			fclose(fp);
			unlink(tmpfile);
			ShowError("Unable to parse configuration file.");
		}
		year = strtoul(s_year, NULL, 10);
		month = strtoul(s_month, NULL, 10);
		day = strtoul(s_day, NULL, 10);
		if (!strcasecmp(name, s_name))
		{
			found = true;
			continue;
		}
		fprintf(fp2, "%d\t%d\t%d\t%s\n", year, month, day, s_name);
	}
	fclose(fp2);
	fclose(fp);
	if (found)
	{
		unlink(config_file);
		rename(tmpfile, config_file);
		chmod(config_file, S_IRUSR | S_IWUSR | S_IXUSR);
		printf("Removed: %02d/%02d/%04d %s\n", day, month, year, name);
	} else {
		unlink(tmpfile);
		ShowError("Unable to find: %s", name);
	}
}

void LoadConfig(void)
{
	FILE		*fp;
	uint32_t	line;
	char		buf[1024];
	char		*c;
	char		*dy, *dm, *dd, *dn;
	uint32_t	d_y, d_m, d_d;
	uint32_t	this_year;
	time_t		ts;
	struct tm	tm;
	struct tm	*this_tm;
	bd_entry_t	*entry = NULL;

	ts = time(NULL);
	this_tm = localtime(&ts);
	this_year = (this_tm->tm_year + 1900);

	FixupConfigFile();
	fp = fopen(config_file, "r");
	if (!fp)
		ShowError("Unable to open configuration file [%s].", config_file);
	line = 0;
	while (!feof(fp))
	{
		if (!fgets(buf, sizeof(buf), fp))
			break;
		line++;
		c = strchr(buf, '\r');
		/* remove CR and LF */
		if (c)
			*c = '\0';
		c = strchr(buf, '\n');
		if (c)
			*c = '\0';
		/* ignore comments from # until end of the line */
		c = strchr(buf, '#');
		if (c)
			*c = '\0';
		/* ignore blank lines */
		if (buf[0] == '\0')
			continue;
		/* line should contain: <year>\t<month>\t<day>\t<name> */
		dy = strtok(buf, "\t");
		dm = strtok(NULL, "\t");
		dd = strtok(NULL, "\t");
		dn = strtok(NULL, "\t");
		if (!dy || !dm || !dd || !dn)
		{
			fclose(fp);
			ShowError("Malformed config file on line #%d", line);
		}
		d_y = strtoul(dy, NULL, 10);
		d_m = strtoul(dm, NULL, 10);
		d_d = strtoul(dd, NULL, 10);
		if (d_y < 1900 || d_m < 1 || d_d < 1 || d_y > 2021 || d_m > 12 || d_d > 31)
		{
			fclose(fp);
			ShowError("Invalid date on line #%d", line);
		}
		entry = malloc(sizeof(bd_entry_t));
		if (!entry)
		{
			fclose(fp);
			ShowError("Unable to allocate memory.");
		}
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_mday = d_d;
		tm.tm_mon = (d_m - 1);
		tm.tm_year = (d_y - 1900);
		tm.tm_wday = 0;
		tm.tm_yday = 0;
		tm.tm_isdst = 0;
		tm.tm_zone = NULL;
		tm.tm_gmtoff = 0;
		entry->ts = mktime(&tm);
		entry->age = (this_year - d_y);
		entry->year = d_y;
		entry->month = d_m;
		entry->day = d_d;
		tm.tm_year = (this_year - 1900);
		entry->delta = mktime(&tm);
		if (entry->delta < ts)
		{
			/* birthday already passed this year */
			tm.tm_year++;
			entry->delta = mktime(&tm);
			entry->delta = (entry->delta - ts);
		} else {
			/* birthday is this year after now */
			entry->delta = (entry->delta - ts);
			entry->age--;
		}
		strftime(entry->next_bday, sizeof(entry->next_bday), "%d/%b/%Y", &tm);
		snprintf(entry->name, sizeof(entry->name), "%s", dn);
		FriendlyDelta(entry->days_to_go, sizeof(entry->days_to_go), entry->delta);
		InsertBirthday(entry);
	}
	fclose(fp);
}

void ShowBirthdays(void)
{
	bd_entry_t	*entry = NULL;

	entry = bd_entry_head;
	while (entry)
	{
		printf("%20s: %s until %s (%d yo)\n",
			entry->name,
			entry->days_to_go,
			entry->next_bday,
			(entry->age + 1));
		entry = (bd_entry_t *)entry->next;
	}
}

int main(int argc, char **argv)
{
	int	i;
	char	*dob, *name;

	printf("$Id: bd.c 780 2021-09-13 20:31:16Z dan $\n");
	snprintf(config_file, sizeof(config_file), "~/.bd.conf");
	/* parse cmdline */
	for (i = 1; i < argc; i++)
	{
		if (!strcasecmp(argv[i], "-h") || !strcasecmp(argv[i], "-help") || !strcasecmp(argv[i], "--help"))
			Usage(argv[0]);
		if (!strcasecmp(argv[i], "-config"))
		{
			if (i >= (argc - 1))
				ShowError("Missing configuration filename");
			i++;
			snprintf(config_file, sizeof(config_file), "%s", argv[i]);
			continue;
		}
		if (!strcasecmp(argv[i], "-add") || !strcasecmp(argv[i], "--add"))
		{
			if (i >= (argc - 2))
				ShowError("Missing birthday or name to add");
			i++;
			dob = argv[i];
			i++;
			name = argv[i];
			if (strlen(dob) != 10)
				ShowError("Invalid date of birth, must be in the format dd/mm/yyyy - use leading zeros if needed");
			if (name[0] == '\0')
				ShowError("Name can't be blank");
			AddBirthday(dob, name);
			continue;
		}
		if (!strcasecmp(argv[i], "-remove") || !strcasecmp(argv[i], "-delete"))
		{
			if (i >= (argc - 1))
				ShowError("Missing name to remove");
			i++;
			name = argv[i];
			if (name[0] == '\0')
				ShowError("Name can't be blank");
			RemoveBirthday(name);
			continue;
		}
		ShowError("Unknown command line option: %s", argv[i]);
	}

	LoadConfig();
	ShowBirthdays();

	exit(0);
}
