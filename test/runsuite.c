/*
 * A test driver for the psqlodbc regression tests.
 *
 * This program runs one regression tests from the exe/ directory,
 * and compares the output with the expected output in the expected/ directory.
 * Reports success or failure in TAP compatible fashion.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#define open _open
#define read _read
#define close _close
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strdup _strdup
#endif

static int rundiff(const char *testname, const char *inputdir);
static int runtest(const char *binname, const char *testname, int testno, const char *inputdir);

static char *slurpfile(const char *filename, size_t *len);

static void
bailout(const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);

	printf("Bail out! ");
	vprintf(fmt, argp);

	va_end(argp);

	exit(1);
}

#ifdef WIN32
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif

/* Given a test program's name, get the test name */
void
parse_argument(const char *in, char *testname, char *binname)
{
	const char *basename;
#ifdef WIN32
	const char *suffix = "-test.exe";
#else
	const char *suffix = "-test";
#endif
	size_t		len;

	/* if the input is a plain test name, construct the binary name from it */
	if (strchr(in, DIR_SEP) == NULL)
	{
		strcpy(testname, in);
		sprintf(binname, "exe%c%s-test", DIR_SEP, in);
		return;
	}

	/*
	 * Otherwise the input is a binary name, and we'll construct the test name
	 * from it.
	 */
	strcpy(binname, in);

	/* Find the last / or \ character */
	basename = strrchr(in, DIR_SEP) + 1;

	/* Strip -test or -test.exe suffix */
	if (strlen(basename) <= strlen(suffix))
	{
		strcpy(testname, basename);
		return;
	}

	len = strlen(basename) - strlen(suffix);
	if (strcmp(&basename[len], suffix) != 0)
	{
		strcpy(testname, basename);
		return;
	}

	memcpy(testname, basename, len);
	testname[len] = '\0';
}

int main(int argc, char **argv)
{
	char		binname[1000];
	char		testname[100];
	int			numtests;
	int			i, j;
	int			failures;
	const char	*inputdir = ".";
	int		sub_count = 1;

	if (argc < 2)
	{
		printf("Usage: runsuite <test binary> ...\n");
		exit(1);
	}
	if (strncmp(argv[argc - 1], "--inputdir=", 11) == 0)
	{
		sub_count++;
		inputdir = argv[argc - 1] + 11;
	}
	numtests = argc - sub_count;

	printf("TAP version 13\n");
	printf("1..%d\n", numtests);

	/*
	 * We accept either test binary name or plain test name.
	 */
	failures = 0;
	for (i = 1, j = 1; i <= numtests; i++, j++)
	{
		parse_argument(argv[j], testname, binname);
		if (runtest(binname, testname, i, inputdir) != 0)
			failures++;
	}

	exit(failures > 254 ? 254 : failures);
}

/* Return 0 on success, 1 on failure */
static int
runtest(const char *binname, const char *testname, int testno, const char *inputdir)
{
	char		cmdline[1024];
	int			rc;
	int			ret;
	int			diff;

	/*
	 * ODBCSYSINI=. tells unixodbc where to find the driver config file,
	 * odbcinst.ini
	 *
	 * ODBCINSTINI=./odbcinst.ini tells the same for iodbc. iodbc also requires
	 * ODBCINI=./odbc.ini to tell it where to find the datasource config.
	 *
	 * We wouldn't need to iodbc stuff when building with unixodbc and vice
	 * versa, but it doesn't hurt either.
	 */
#ifndef WIN32
	snprintf(cmdline, sizeof(cmdline),
			 "ODBCSYSINI=. ODBCINSTINI=./odbcinst.ini ODBCINI=./odbc.ini "
			 "%s > results/%s.out",
			 binname, testname);
#else
	snprintf(cmdline, sizeof(cmdline),
			 "%s > results\\%s.out",
			 binname, testname);
#endif
	rc = system(cmdline);

	diff = rundiff(testname, inputdir);
	if (rc != 0)
	{
		printf("not ok %d - %s test returned %d\n", testno, testname, rc);
		ret = 1;
	}
	else if (diff != 0)
	{
		printf("not ok %d - %s test output differs\n", testno, testname);
		ret = 1;
	}
	else
	{
		printf("ok %d - %s\n", testno, testname);
		ret = 0;
	}
	fflush(stdout);

	return ret;
}

#ifdef	WIN32
	static int	diff_call = 1, first_call = 1;
#endif /* WIN32 */

static int
call_diff(const char *inputdir, const char *expected_dir, const char *testname, int outputno, const char *result_dir, const char *outspec)
{
	char	no_str[8];
	char	cmdline[1024];
	int	diff_rtn;

	if (0 == outputno)
		*no_str = '\0';
	else
		snprintf(no_str, sizeof(no_str), "_%d", outputno);
#ifdef	WIN32
	if (diff_call)
		snprintf(cmdline, sizeof(cmdline),
			 "diff -c --strip-trailing-cr %s%s%s%s.out %s%s.out %s",
			 inputdir, expected_dir, testname, no_str, result_dir, testname, outspec);
	else	/* use fc command instead */
		snprintf(cmdline, sizeof(cmdline),
			 "fc /N %s%s%s%s.out %s%s.out %s",
			 inputdir, expected_dir, testname, no_str, result_dir, testname, outspec);
#else
	snprintf(cmdline, sizeof(cmdline),
			 "diff -c %s%s%s%s.out %s%s.out %s",
			 inputdir, expected_dir, testname, no_str, result_dir, testname, outspec);
#endif /* WIN32 */
	if ((diff_rtn = system(cmdline)) == -1)
		printf("# diff failed\n");

	return diff_rtn;
}

static int
rundiff(const char *testname, const char *inputdir)
{
	char		filename[1024];
	int		outputno, no_spec;
	char	   *result = NULL;
	size_t		result_len;
#ifdef	WIN32
	const char	*expected_dir = "\\expected\\";
	const char	*result_dir = "results\\";
#else
	const char	*expected_dir = "/expected/";
	const char	*result_dir = "results/";
#endif
	int		diff_rtn;
	int		i, j;
	const char	CR = '\r', LF = '\n';
	char		se, sr;

	snprintf(filename, sizeof(filename), "%s%s.out", result_dir, testname);
	result = slurpfile(filename, &result_len);

	outputno = 0;
	for (;;)
	{
		char	   *expected;
		size_t		expected_len;

		if (outputno == 0)
			snprintf(filename, sizeof(filename), "%s%s%s.out", inputdir, expected_dir, testname);
		else
			snprintf(filename, sizeof(filename), "%s%s%s_%d.out", inputdir, expected_dir, testname, outputno);
		expected = slurpfile(filename, &expected_len);
		if (expected == NULL)
		{
			if (outputno == 0)
				bailout("could not open file %s: %s\n", filename, strerror(ENOENT));
			break;
		}

		if (expected_len == result_len &&
			memcmp(expected, result, expected_len) == 0)
		{
			/* The files are equal. */
			free(result);
			free(expected);
			return 0;
		}
		/* Ignore the difference between CR LF, LF and CR line break */
		for (i = 0, j = 0, se = sr = '\0'; i < expected_len && j < result_len;
				se = expected[i], sr = result[j], i++, j++)
		{
			if (expected[i] == result[j])
				continue;
			if (result[j] == LF)
			{
				if (expected[i] == CR)
				{
					i++;
					if (expected[i] != LF)
						i--;
					continue;
				}
				else if (sr == CR && se == CR)
				{
					i--;
					continue;
				}
			}
			else if (expected[i] == LF)
			{
				if (result[j] == CR)
				{
					j++;
					if (result[j] != LF)
						j--;
					continue;
				}
				else if (sr == CR && se == CR)
				{
					j--;
					continue;
				}
			}
			break;
		}
		if (i >= expected_len && j >= result_len)
		{
			/* The files are equal. */
			free(result);
			free(expected);
			return 0;
		}

		free(expected);

		outputno++;
	}
	/* no matching output found */
	if (NULL != result)
		free(result);

	/*
	 * Try to run diff. If this fails on Windows system, use fc command
	 * instead.
	 */
#ifdef	WIN32
	if (first_call)
	{
		char	cmdline[1024];

		/*
		 *	test diff the same file
		 */
		snprintf(cmdline, sizeof(cmdline),
			 "diff -c --strip-trailing-cr results\\%s.out results\\%s.out",
			 testname, testname);
		first_call = 0;
		/*
		 *	If diff command exists, the system function would
		 *	return 0.
		 */
		if (system(cmdline) != 0)
			diff_call = 0;
	}
#endif
	/*
	 * We run the diff against all output files and print the smallest
	 * diff.
	 */
	no_spec = 0;
	if (outputno > 1)
	{
		const char	*tmpdiff = "tmpdiff.diffs";
		char	outfmt[32];
		int	fd, file_size;
		struct stat stbuf;
		int	min_size = -1;

		snprintf(outfmt, sizeof(outfmt), "> %s", tmpdiff);
		for (i = 0; i < outputno; i++)
		{
			call_diff(inputdir, expected_dir, testname, i, result_dir, outfmt);
			if ((fd = open(tmpdiff, O_RDONLY)) < 0)
				break;
			if (fstat(fd, &stbuf) == -1)
				break;
			if (file_size = stbuf.st_size, file_size == 0)
			{
				min_size = 0;
				no_spec = i;
			}
			else if (min_size < 0)
				min_size = file_size;
			else if (file_size < min_size)
			{
				no_spec = i;
				min_size = file_size;
			}
			close(fd);
		}
		remove(tmpdiff);
		if (min_size == 0)
			return 0;
	}
	diff_rtn = call_diff(inputdir, expected_dir, testname, no_spec, result_dir, ">> regression.diffs");

	return diff_rtn;
}


/*
 * Reads file to memory. The file is returned, or NULL if it doesn't exist.
 * Length is returned in *len.
 */
static char *
slurpfile(const char *filename, size_t *len)
{
	int			fd;
	struct stat stbuf;
	int			readlen;
	off_t		filelen;
	char	   *p;

#ifdef WIN32
	fd = open(filename, O_RDONLY | O_BINARY, 0);
#else
	fd = open(filename, O_RDONLY, 0);
#endif
	if (fd == -1)
	{
		if (errno == ENOENT)
			return NULL;

		bailout("could not open file %s: %s\n", filename, strerror(errno));
	}
	if (fstat(fd, &stbuf) < 0)
		bailout("fstat failed on file %s: %s\n", filename, strerror(errno));

	filelen = stbuf.st_size;
	p = malloc(filelen + 1);
	if (!p)
		bailout("out of memory reading file %s\n", filename);
	readlen = read(fd, p, filelen);
	if (readlen != filelen)
		bailout("read only %d bytes out of %d from %s\n", (int) readlen, (int) filelen, filename);
	p[readlen] = '\0';
	close(fd);

	*len = readlen;
	return p;
}
