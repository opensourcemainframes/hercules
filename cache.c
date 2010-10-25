/* CACHE.C    (c) Copyright Greg Smith, 2002-2010                    */
/*            Dynamic cache manager for multi-threaded applications  */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

//FIXME ?? Dynamic resizing is disabled

#include "hstdinc.h"

#define _CACHE_C_
#define _HDASD_DLL_

#include "hercules.h"

static CACHEBLK cacheblk[CACHE_MAX_INDEX];

/*-------------------------------------------------------------------*/
/* Public functions                                                  */
/*-------------------------------------------------------------------*/
int cache_nbr (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].nbr;
}

int cache_busy (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].busy;
}

int cache_empty (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].empty;
}

int cache_waiters (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].waiters;
}

S64 cache_size (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].size;
}

S64 cache_hits (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].hits;
}

S64 cache_misses (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return cacheblk[ix].misses;
}

int cache_busy_percent (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return (cacheblk[ix].busy * 100) / cacheblk[ix].nbr;
}

int cache_empty_percent (int ix)
{
    if (cache_check_ix(ix)) return -1;
    return (cacheblk[ix].empty * 100) / cacheblk[ix].nbr;
}

int cache_hit_percent (int ix)
{
    S64 total;
    if (cache_check_ix(ix)) return -1;
    total = cacheblk[ix].hits + cacheblk[ix].misses;
    if (total == 0) return -1;
    return (int)((cacheblk[ix].hits * 100) / total);
}

int cache_lookup (int ix, U64 key, int *o)
{
    int i,p;
    if (o) *o = -1;
    if (cache_check_ix(ix)) return -1;
    /* `p' is the preferred index */
    p = (int)(key % cacheblk[ix].nbr);
    if (cacheblk[ix].cache[p].key == key) {
        i = p;
        cacheblk[ix].fasthits++;
    }
    else {
        if (cache_isbusy(ix, p) || cacheblk[ix].age - cacheblk[ix].cache[p].age < 20)
            p = -2;
        for (i = 0; i < cacheblk[ix].nbr; i++) {
            if (cacheblk[ix].cache[i].key == key) break;
            if (o && !cache_isbusy(ix, i)
             && (*o < 0 || i == p || cacheblk[ix].cache[i].age < cacheblk[ix].cache[*o].age))
                if (*o != p) *o = i;
        }
    }
    if (i >= cacheblk[ix].nbr) {
        i = -1;
        cacheblk[ix].misses++;
    }
    else
        cacheblk[ix].hits++;

    if (i < 0 && o && *o < 0) cache_adjust(ix,1);
    else cache_adjust(ix, 0);
    return i;
}

int cache_scan (int ix, CACHE_SCAN_RTN rtn, void *data)
{
int      i;                             /* Cache index               */
int      rc;                            /* Return code               */
int      answer = -1;                   /* Answer from routine       */

    if (cache_check_ix(ix)) return -1;
    for (i = 0; i < cacheblk[ix].nbr; i++) {
        rc = (rtn)(&answer, ix, i, data);
        if (rc != 0) break;
    }
    return answer;
}

int cache_lock(int ix)
{
    if (cache_check_cache(ix)) return -1;
    obtain_lock(&cacheblk[ix].lock);
    return 0;
}

int cache_unlock(int ix)
{
    if (cache_check_ix(ix)) return -1;
    release_lock(&cacheblk[ix].lock);
    if (cacheblk[ix].empty == cacheblk[ix].nbr)
        cache_destroy(ix);
    return 0;
}

int cache_wait(int ix)
{
    if (cache_check_ix(ix)) return -1;
    if (cacheblk[ix].busy < cacheblk[ix].nbr)
        return 0;
    if (cache_adjust(ix, 1))
        return 0;

    cacheblk[ix].waiters++; cacheblk[ix].waits++;

#if FALSE 
    {
#if defined( OPTION_WTHREADS )
        timed_wait_condition( &cacheblk[ix].waitcond, &cacheblk[ix].lock, CACHE_WAITTIME );
#else
    struct timeval  now;
    struct timespec tm;
        gettimeofday (&now, NULL);
        tm.tv_sec = now.tv_sec;
        tm.tv_nsec = (now.tv_usec + CACHE_WAITTIME) * 1000;
        tm.tv_sec += tm.tv_nsec / 1000000000;
        tm.tv_nsec = tm.tv_nsec % 1000000000;
        timed_wait_condition(&cacheblk[ix].waitcond, &cacheblk[ix].lock, &tm);
#endif
    }
#else
    wait_condition(&cacheblk[ix].waitcond, &cacheblk[ix].lock);
#endif
    cacheblk[ix].waiters--;
    return 0;
}

U64 cache_getkey(int ix, int i)
{
    if (cache_check(ix,i)) return (U64)-1;
    return cacheblk[ix].cache[i].key;
}

U64 cache_setkey(int ix, int i, U64 key)
{
    U64 oldkey;
    int empty;

    if (cache_check(ix,i)) return (U64)-1;
    empty = cache_isempty(ix, i);
    oldkey = cacheblk[ix].cache[i].key;
    cacheblk[ix].cache[i].key = key;
    if (empty && !cache_isempty(ix, i))
        cacheblk[ix].empty--;
    else if (!empty && cache_isempty(ix, i))
        cacheblk[ix].empty++;
    return oldkey;
}

U32 cache_getflag(int ix, int i)
{
    if (cache_check(ix,i)) return (U32)-1;
    return cacheblk[ix].cache[i].flag;
}

U32 cache_setflag(int ix, int i, U32 andbits, U32 orbits)
{
    U32 oldflags;
    int empty;
    int busy;

    if (cache_check(ix,i)) return (U32)-1;

    empty = cache_isempty(ix, i);
    busy = cache_isbusy(ix, i);
    oldflags = cacheblk[ix].cache[i].flag;

    cacheblk[ix].cache[i].flag &= andbits;
    cacheblk[ix].cache[i].flag |= orbits;

    if (!cache_isbusy(ix, i) && cacheblk[ix].waiters > 0)
        signal_condition(&cacheblk[ix].waitcond);
    if (busy && !cache_isbusy(ix, i))
        cacheblk[ix].busy--;
    else if (!busy && cache_isbusy(ix, i))
        cacheblk[ix].busy++;
    if (empty && !cache_isempty(ix, i))
        cacheblk[ix].empty--;
    else if (!empty && cache_isempty(ix, i))
        cacheblk[ix].empty++;
    return oldflags;
}

U64 cache_getage(int ix, int i)
{
    if (cache_check(ix,i)) return (U64)-1;
    return cacheblk[ix].cache[i].age;
}

U64 cache_setage(int ix, int i)
{
    U64 oldage;
    int empty;

    if (cache_check(ix,i)) return (U64)-1;
    empty = cache_isempty(ix, i);
    oldage = cacheblk[ix].cache[i].age;
    cacheblk[ix].cache[i].age = ++cacheblk[ix].age;
    if (empty) cacheblk[ix].empty--;
    return oldage;
}

void *cache_getbuf(int ix, int i, int len)
{
    if (cache_check(ix,i)) return NULL;
    if (len > 0
     && cacheblk[ix].cache[i].buf != NULL
     && cacheblk[ix].cache[i].len < len) {
        cacheblk[ix].size -= cacheblk[ix].cache[i].len;
        free (cacheblk[ix].cache[i].buf);
        cacheblk[ix].cache[i].buf = NULL;
        cacheblk[ix].cache[i].len = 0;
    }
    if (cacheblk[ix].cache[i].buf == NULL && len > 0)
        cache_allocbuf (ix, i, len);
    return cacheblk[ix].cache[i].buf;
}

void *cache_setbuf(int ix, int i, void *buf, int len)
{
    void *oldbuf;
    if (cache_check(ix,i)) return NULL;
    oldbuf = cacheblk[ix].cache[i].buf;
    cacheblk[ix].size -= cacheblk[ix].cache[i].len;
    cacheblk[ix].cache[i].buf = buf;
    cacheblk[ix].cache[i].len = len;
    cacheblk[ix].size += len;
    return oldbuf;
}

int cache_getlen(int ix, int i)
{
    if (cache_check(ix,i)) return -1;
    return cacheblk[ix].cache[i].len;
}

int cache_getval(int ix, int i)
{
    if (cache_check(ix,i)) return -1;
    return cacheblk[ix].cache[i].value;
}

int cache_setval(int ix, int i, int val)
{
    int rc;
    if (cache_check(ix,i)) return -1;
    rc = cacheblk[ix].cache[i].value;
    cacheblk[ix].cache[i].value = val;
    return rc;
}

int cache_release(int ix, int i, int flag)
{
    void *buf;
    int   len;
    int   empty;
    int   busy;

    if (cache_check(ix,i)) return -1;

    empty = cache_isempty(ix, i);
    busy = cache_isbusy(ix, i);

    buf = cacheblk[ix].cache[i].buf;
    len = cacheblk[ix].cache[i].len;

    memset (&cacheblk[ix].cache[i], 0, sizeof(CACHE));

    if ((flag & CACHE_FREEBUF) && buf != NULL) {
        free (buf);
        cacheblk[ix].size -= len;
        buf = NULL;
        len = 0;
    }

    cacheblk[ix].cache[i].buf = buf;
    cacheblk[ix].cache[i].len = len;

    if (cacheblk[ix].waiters > 0)
        signal_condition(&cacheblk[ix].waitcond);

    if (!empty) cacheblk[ix].empty++;
    if (busy) cacheblk[ix].busy--;

    return 0;
}

DLL_EXPORT int cachestats_cmd(int argc, char *argv[], char *cmdline)
{
    int ix, i;
    char buf[128];

    UNREFERENCED(cmdline);
    UNREFERENCED(argc);
    UNREFERENCED(argv);

    for (ix = 0; ix < CACHE_MAX_INDEX; ix++) {
        if (cacheblk[ix].magic != CACHE_MAGIC) {          
            MSGBUF(buf, "Cache[%d] ....... not created", ix);
            WRMSG(HHC02294, "I", buf);
            continue;
        }
        MSGBUF( buf, "Cache............ %10d", ix);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "nbr ............. %10d", cacheblk[ix].nbr);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "busy ............ %10d", cacheblk[ix].busy);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "busy%% ........... %10d",cache_busy_percent(ix));
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "empty ........... %10d", cacheblk[ix].empty);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "waiters ......... %10d", cacheblk[ix].waiters);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "waits ........... %10d", cacheblk[ix].waits);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "buf size ........ %10" I64_FMT "d", cacheblk[ix].size);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "hits ............ %10" I64_FMT "d", cacheblk[ix].hits);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "fast hits ....... %10" I64_FMT "d", cacheblk[ix].fasthits);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "misses .......... %10" I64_FMT "d", cacheblk[ix].misses);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "hit%% ............ %10d", cache_hit_percent(ix));
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "age ............. %10" I64_FMT "d", cacheblk[ix].age);
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "last adjusted ... %s", cacheblk[ix].atime == 0 ? "none\n" : ctime(&cacheblk[ix].atime));
        buf[strlen(buf)-1] = '\0';
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "last wait ....... %s", cacheblk[ix].wtime == 0 ? "none\n" : ctime(&cacheblk[ix].wtime));
        buf[strlen(buf)-1] = '\0';
        WRMSG(HHC02294, "I", buf);
        MSGBUF( buf, "adjustments ..... %10d", cacheblk[ix].adjusts);
        WRMSG(HHC02294, "I", buf);

        if (argc > 1)
          for (i = 0; i < cacheblk[ix].nbr; i++)
          {
            MSGBUF( buf, "[%4d] %16.16" I64_FMT "x %8.8x %10p %6d %10" I64_FMT "d",
              i, cacheblk[ix].cache[i].key, cacheblk[ix].cache[i].flag,
              cacheblk[ix].cache[i].buf, cacheblk[ix].cache[i].len,
              cacheblk[ix].cache[i].age);
            WRMSG(HHC02294, "I", buf);
          }
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/* Private functions                                                 */
/*-------------------------------------------------------------------*/
static int cache_create (int ix)
{
    cache_destroy (ix);
    cacheblk[ix].magic = CACHE_MAGIC;
//FIXME See the note in cache.h about CACHE_DEFAULT_L2_NBR
    cacheblk[ix].nbr = ix != CACHE_L2 ? CACHE_DEFAULT_NBR : CACHE_DEFAULT_L2_NBR;
    cacheblk[ix].empty = cacheblk[ix].nbr;
    initialize_lock (&cacheblk[ix].lock);
    initialize_condition (&cacheblk[ix].waitcond);
    cacheblk[ix].cache = calloc (cacheblk[ix].nbr, sizeof(CACHE));
    if (cacheblk[ix].cache == NULL) {
        WRMSG (HHC00011, "E", "cache()", ix, (int)(cacheblk[ix].nbr * (int)sizeof(CACHE)), errno, strerror(errno));
        return -1;
    }
    return 0;
}

static int cache_destroy (int ix)
{
    int i;
    if (cacheblk[ix].magic == CACHE_MAGIC) {
        destroy_lock (&cacheblk[ix].lock);
        destroy_condition (&cacheblk[ix].waitcond);
        if (cacheblk[ix].cache) {
            for (i = 0; i < cacheblk[ix].nbr; i++)
                cache_release(ix, i, CACHE_FREEBUF);
            free (cacheblk[ix].cache);
        }
    }
    memset(&cacheblk[ix], 0, sizeof(CACHEBLK));
    return 0;
}

static int cache_check_ix(int ix)
{
    if (ix < 0 || ix >= CACHE_MAX_INDEX) return -1;
    return 0;
}

static int cache_check_cache(int ix)
{
    if (cache_check_ix(ix)
     || (cacheblk[ix].magic != CACHE_MAGIC && cache_create(ix)))
        return -1;
    return 0;
}

static int cache_check(int ix, int i)
{
    if (cache_check_ix(ix) || i < 0 || i >= cacheblk[ix].nbr)
        return -1;
    return 0;
}

static int cache_isbusy(int ix, int i)
{
    return ((cacheblk[ix].cache[i].flag & CACHE_BUSY) != 0);
}

static int cache_isempty(int ix, int i)
{
    return (cacheblk[ix].cache[i].key  == 0
         && cacheblk[ix].cache[i].flag == 0
         && cacheblk[ix].cache[i].age  == 0);
}

static int cache_adjust(int ix, int n)
{
#if 0
    time_t now;
    int    busypct, hitpct, nbr, empty, sz;

    now = time(NULL);
    busypct = cache_busy_percent(ix);
    hitpct = cache_hit_percent(ix);
    nbr = cacheblk[ix].nbr;
    empty = cache_empty(ix);
    sz = cacheblk[ix].size;

    if (n == 0) {
        /* Normal adjustments */
        if (now - cacheblk[ix].atime < CACHE_ADJUST_INTERVAL) return 0;
        cacheblk[ix].atime = now;

        /* Increase cache if a lot of busy entries */
        if (((nbr <= CACHE_ADJUST_NUMBER || sz < CACHE_ADJUST_SIZE) && busypct >= CACHE_ADJUST_BUSY1)
         || busypct > CACHE_ADJUST_BUSY2)
            return cache_resize(ix, CACHE_ADJUST_RESIZE);

        /* Decrease cache if too many empty entries */
        if (nbr > CACHE_ADJUST_NUMBER && empty >= CACHE_ADJUST_EMPTY)
            return cache_resize(ix, -CACHE_ADJUST_RESIZE);

        /* Increase cache if hit percentage is too low */
        if (hitpct > 0) {
            if ((nbr <= CACHE_ADJUST_NUMBER && hitpct < CACHE_ADJUST_HIT1)
             || hitpct < CACHE_ADJUST_HIT2)
                return cache_resize(ix, CACHE_ADJUST_RESIZE);
        }

        /* Decrease cache if hit percentage is ok and not many busy */
        if (hitpct >= CACHE_ADJUST_HIT3 && busypct <= CACHE_ADJUST_BUSY3
         && cacheblk[ix].size >= CACHE_ADJUST_SIZE)
            return cache_resize(ix, -CACHE_ADJUST_RESIZE);
    } else {
        /* All cache entries are busy */
        if (nbr <= CACHE_ADJUST_NUMBER)
            return cache_resize(ix, CACHE_ADJUST_RESIZE);

        /* Increase cache if previous wait within this interval */
        if (now - cacheblk[ix].wtime <= CACHE_ADJUST_WAITTIME) {
            return cache_resize(ix, CACHE_ADJUST_RESIZE);
        }
        cacheblk[ix].wtime = now;
    }
#else
    UNREFERENCED(ix);
    UNREFERENCED(n);
#endif
    return 0;
}

#if 0
static int cache_resize (int ix, int n)
{
    CACHE *cache;
    int    i;

    if (n == 0) return 0;
    else if (n > 0) {
        /* Increase cache size */
        cache = realloc (cacheblk[ix].cache, (cacheblk[ix].nbr + n) * sizeof(CACHE));
        if (cache == NULL) {
            WRMSG (HHC00011, "E", "realloc() increase", ix, (cacheblk[ix].nbr + n) * sizeof(CACHE), errno, strerror(errno));
            return 0;
        }
        cacheblk[ix].cache = cache;
        for (i = cacheblk[ix].nbr; i < cacheblk[ix].nbr +n; i++)
            memset(&cacheblk[ix].cache[i], 0, sizeof(CACHE));
        cacheblk[ix].nbr += n;
        cacheblk[ix].empty += n;
        cacheblk[ix].adjusts++;
    } else if (n < 0) {
        /* Decrease cache size */
        for (i = cacheblk[ix].nbr - 1; i >= cacheblk[ix].nbr + n; i--)
            if (cache_isbusy(ix, i)) break;
            else cache_release(ix, i, CACHE_FREEBUF);
        n = cacheblk[ix].nbr - i + 1;
        if (n == 0) return 0;
        cache = realloc (cacheblk[ix].cache, (cacheblk[ix].nbr - n) * sizeof(CACHE));
        if (cache == NULL) {
            WRMSG (HHC00011, "E", "realloc() decrease", ix, (cacheblk[ix].nbr - n) * sizeof(CACHE), errno, strerror(errno));
            return 0;
        }
        cacheblk[ix].cache = cache;
        cacheblk[ix].nbr -= n;
        cacheblk[ix].empty -= n;
        cacheblk[ix].adjusts++;
    }
    return 1;
}
#endif

static void cache_allocbuf(int ix, int i, int len)
{
    cacheblk[ix].cache[i].buf = calloc (len, 1);
    if (cacheblk[ix].cache[i].buf == NULL) {
        WRMSG (HHC00011, "E", "calloc()", ix, len, errno, strerror(errno));
        WRMSG (HHC00012, "W");
        for (i = 0; i < cacheblk[ix].nbr; i++)
            if (!cache_isbusy(ix, i)) cache_release(ix, i, CACHE_FREEBUF);
        cacheblk[ix].cache[i].buf = calloc (len, 1);
        if (cacheblk[ix].cache[i].buf == NULL) {
            WRMSG (HHC00011, "E", "calloc()", ix, len, errno, strerror(errno));
            return;
        }
    }
    cacheblk[ix].cache[i].len = len;
    cacheblk[ix].size += len;
}
