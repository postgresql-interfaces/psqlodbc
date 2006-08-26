#undef	_MEMORY_DEBUG_
#include	"psqlodbc.h"

/*
#undef	malloc
#undef	calloc
#undef	realloc
#undef	strdup
#undef	free
#undef	memcpy
#undef	strcpy
#undef	strncpy
#undef	strncpy_null
#undef	memset
*/
#include	"misc.h"
#ifdef	WIN32
#include	<malloc.h>
#endif /* WIN32 */
#ifdef	_DEBUG
#include	<stdlib.h>
#define	_CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif /* _DEBUG */
#include	<string.h>

typedef struct {
	size_t	len;
	void	*aladr;
} ALADR;

static int	alsize = 0;
static int	tbsize = 0;
static ALADR	*altbl = NULL;

CSTR	ALCERR	= "alcerr";
void * debug_alloc(size_t size)
{
	void * alloced;
	alloced = malloc(size);
inolog(" alloced=%x(%d)\n", alloced, size);
	if (alloced)
	{
		if (!alsize)
		{
			alsize = 100;
			altbl = (ALADR *) malloc(alsize * sizeof(ALADR));
		} 
		else if (tbsize >= alsize)
		{
			alsize *= 2;
			altbl = (ALADR *) realloc(altbl, alsize * sizeof(ALADR));
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = size;
		tbsize++; 
	}
	else
		mylog("%s:alloc %dbyte\n", ALCERR, size);
	return alloced;
}
void * debug_calloc(size_t n, size_t size)
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
			alsize *= 2;
			altbl = (ALADR *) realloc(altbl, alsize * sizeof(ALADR));
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = n * size;
		tbsize++; 
	}
	else
		mylog("%s:calloc %dbyte\n", ALCERR, size);
inolog("calloced = %x\n", alloced);
	return alloced;
}
void * debug_realloc(void * ptr, size_t size)
{
	void * alloced = realloc(ptr, size);
	if (!alloced)
	{
		mylog("%s:debug_realloc %x error\n", ALCERR, ptr);
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
		
	inolog("debug_realloc %x->%x\n", ptr, alloced);
	return alloced;
}
char * debug_strdup(const char * ptr)
{
	char * alloced = strdup(ptr);
	if (!alloced)
	{
		mylog("%s:debug_strdup %x error\n", ALCERR, ptr);
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
			alsize *= 2;
			altbl = (ALADR *) realloc(altbl, alsize * sizeof(ALADR));
		}
		altbl[tbsize].aladr = alloced;
		altbl[tbsize].len = strlen(ptr) + 1;
		tbsize++; 
	}
	inolog("debug_strdup %x->%x(%s)\n", ptr, alloced, alloced);
	return alloced;
}

void debug_free(void * ptr)
{
	int i, j;
	int	freed = 0;

	if (!ptr)
	{
		mylog("%s:debug_freeing null ptr\n", ALCERR);
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
			freed =  1;
			break;	
		}
	}
	if (! freed)
		mylog("%s:debug_freeing not found ptr %x\n", ALCERR, ptr);
	else
		inolog("debug_freeing ptr=%x\n", ptr);
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
				mylog("%s:%s:out_check found memory buffer overrun %x(%d)>=%x(%d)\n", ALCERR, name, out, len, altbl[i].aladr, altbl[i].len);
			}
			break;
		}
	}
	return ret;
}
char *debug_strcpy(char *out, const char *in)
{
	if (!out || !in)
	{
		mylog("%s:debug_strcpy null pointer out=%x,in=%x\n", ALCERR, out, in);
		return NULL;
	}
	out_check(out, strlen(in) + 1, "debug_strcpy");
	return strcpy(out, in);
}
char *debug_strncpy(char *out, const char *in, size_t len)
{
	CSTR	func = "debug_strncpy";

	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%x,in=%x\n", ALCERR, func, out, in);
		return NULL;
	}
	out_check(out, len, func);
	return strncpy(out, in, len);
}
char *debug_strncpy_null(char *out, const char *in, size_t len)
{
	CSTR	func = "debug_strncpy_null";

	if (!out || !in)
	{
		mylog("%s:%s null pointer out=%x,in=%x\n", ALCERR, func, out, in);
		return NULL;
	}
	out_check(out, len, func);
	return strncpy_null(out, in, len);
}

void *debug_memcpy(void *out, const void *in, size_t len)
{
	if (!out || !in)
	{
		mylog("%s:debug_memcpy null pointer out=%x,in=%x\n", ALCERR, out, in);
		return NULL;
	}
	out_check(out, len, "debug_memcpy");
	return memcpy(out, in, len);
}

void *debug_memset(void *out, int c, size_t len)
{
	if (!out)
	{
		mylog("%s:debug_memcpy null pointer out=%xx\n", ALCERR, out);
		return NULL;
	}
	out_check(out, len, "debug_memset");
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
			mylog("%s:leak = %x(%d)\n", ALCERR, altbl[i].aladr, altbl[i].len);
		}
	}
}
