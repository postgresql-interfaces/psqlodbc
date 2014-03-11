#undef	_MEMORY_DEBUG_
#include	"psqlodbc.h"

#ifdef	WIN32
#ifdef	_DEBUG
/* #include	<stdlib.h> */
#define	_CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#else
#include	<malloc.h>
#endif /* _DEBUG */
#endif /* WIN32 */
#include	<string.h>

#include	"misc.h"
typedef struct {
	size_t	len;
	void	*aladr;
} ALADR;

static int	alsize = 0;
static int	tbsize = 0;
static ALADR	*altbl = NULL;

CSTR	ALCERR	= "alcerr";
void * pgdebug_alloc(size_t size)
{
	void * alloced;
	alloced = malloc(size);
inolog(" alloced=%p(%d)\n", alloced, size);
	if (alloced)
	{
		if (!alsize)
		{
			alsize = 100;
			altbl = (ALADR *) malloc(alsize * sizeof(ALADR));
		}
		else if (tbsize >= alsize)
		{
			ALADR *al;
			alsize *= 2;
			if (al = (ALADR *) realloc(altbl, alsize * sizeof(ALADR)), NULL == al)
				return alloced;
			altbl = al;
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = size;
		tbsize++;
	}
	else
		mylog("%s:alloc %dbyte\n", ALCERR, size);
	return alloced;
}
void * pgdebug_calloc(size_t n, size_t size)
{
	void * alloced = calloc(n, size);

	if (alloced)
	{
		if (!alsize)
		{
			alsize = 100;
			altbl = (ALADR *) malloc(alsize * sizeof(ALADR));
		}
		else if (tbsize >= alsize)
		{
			ALADR *al;
			alsize *= 2;
			if (al = (ALADR *) realloc(altbl, alsize * sizeof(ALADR)), NULL == al)
				return alloced;
			altbl = al;
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = n * size;
		tbsize++;
	}
	else
		mylog("%s:calloc %dbyte\n", ALCERR, size);
inolog("calloced = %p\n", alloced);
	return alloced;
}
void * pgdebug_realloc(void * ptr, size_t size)
{
	void * alloced = realloc(ptr, size);
	if (!alloced)
	{
		mylog("%s:%s %p error\n", ALCERR, __FUNCTION__, ptr);
	}
	else if (!ptr)
	{
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = size;
		tbsize++;
	}
	else /* if (alloced != ptr) */
	{
		int	i;
		for (i = 0; i < tbsize; i++)
		{
			if (altbl[i].aladr == ptr)
			{
				altbl[i].aladr = alloced;
				altbl[i].len = size;
				break;
			}
		}
	}

	inolog("%s %p->%p\n", __FUNCTION__, ptr, alloced);
	return alloced;
}
char * pgdebug_strdup(const char * ptr)
{
	char * alloced = strdup(ptr);
	if (!alloced)
	{
		mylog("%s:%s %p error\n", ALCERR, __FUNCTION__, ptr);
	}
	else
	{
		if (!alsize)
		{
			alsize = 100;
			altbl = (ALADR *) malloc(alsize * sizeof(ALADR));
		}
		else if (tbsize >= alsize)
		{
			ALADR *al;
			alsize *= 2;
			if (al = (ALADR *) realloc(altbl, alsize * sizeof(ALADR)), NULL == al)
				return alloced;
			altbl = al;
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = strlen(ptr) + 1;
		tbsize++;
	}
	inolog("%s %p->%p(%s)\n", __FUNCTION__, ptr, alloced, alloced);
	return alloced;
}

void pgdebug_free(void * ptr)
{
	int i, j;
	int	freed = 0;

	if (!ptr)
	{
		mylog("%s:%sing null ptr\n", ALCERR, __FUNCTION__);
		return;
	}
	for (i = 0; i < tbsize; i++)
	{
		if (altbl[i].aladr == ptr)
		{
			for (j = i; j < tbsize - 1; j++)
			{
				altbl[j].aladr = altbl[j + 1].aladr;
				altbl[j].len = altbl[j + 1].len;
			}
			tbsize--;
			freed = 1;
			break;
		}
	}
	if (! freed)
	{
		mylog("%s:%sing not found ptr %p\n", ALCERR, __FUNCTION__, ptr);
		return;
	}
	else
		inolog("%sing ptr=%p\n", __FUNCTION__, ptr);
	free(ptr);
}

static BOOL out_check(void *out, size_t len, const char *name)
{
	BOOL	ret = TRUE;
	int	i;

	for (i = 0; i < tbsize; i++)
	{
		if ((UInt4)out < (UInt4)(altbl[i].aladr))
			continue;
		if ((UInt4)out < (UInt4)(altbl[i].aladr) + altbl[i].len)
		{
			if ((UInt4)out + len > (UInt4)(altbl[i].aladr) + altbl[i].len)
			{
				ret = FALSE;
				mylog("%s:%s:out_check found memory buffer overrun %p(%d)>=%p(%d)\n", ALCERR, name, out, len, altbl[i].aladr, altbl[i].len);
			}
			break;
		}
	}
	return ret;
}
char *pgdebug_strcpy(char *out, const char *in)
{
	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%p,in=%p\n", ALCERR, __FUNCTION__, out, in);
		return NULL;
	}
	out_check(out, strlen(in) + 1, __FUNCTION__);
	return strcpy(out, in);
}
char *pgdebug_strncpy(char *out, const char *in, size_t len)
{
	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%p,in=%p\n", ALCERR, __FUNCTION__, out, in);
		return NULL;
	}
	out_check(out, len, __FUNCTION__);
	return strncpy(out, in, len);
}
char *pgdebug_strncpy_null(char *out, const char *in, size_t len)
{
	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%p,in=%p\n", ALCERR, __FUNCTION__, out, in);
		return NULL;
	}
	out_check(out, len, __FUNCTION__);
	return strncpy_null(out, in, len);
}

void *pgdebug_memcpy(void *out, const void *in, size_t len)
{
	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%p,in=%p\n", ALCERR, __FUNCTION__, out, in);
		return NULL;
	}
	out_check(out, len, __FUNCTION__);
	return memcpy(out, in, len);
}

void *pgdebug_memset(void *out, int c, size_t len)
{
	if (!out)
	{
		mylog("%s:%s null pointer out=%p\n", ALCERR, __FUNCTION__, out);
		return NULL;
	}
	out_check(out, len, __FUNCTION__);
	return memset(out, c, len);
}

void debug_memory_check(void)
{
	int i;

	if (0 == tbsize)
	{
		mylog("no memry leak found and max count allocated so far is %d\n", alsize);
		free(altbl);
		alsize = 0;
	}
	else
	{
		mylog("%s:memory leak found check count=%d alloc=%d\n", ALCERR, tbsize, alsize);
		for (i = 0; i < tbsize; i++)
		{
			mylog("%s:leak = %p(%d)\n", ALCERR, altbl[i].aladr, altbl[i].len);
		}
	}
}
