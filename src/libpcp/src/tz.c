/*
 * Copyright (c) 1995-2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"

static char	*envtz;		/* buffer in env */
static int	envtzlen;

static char	*savetz;		/* real $TZ from env */
static char	**savetzp;

static int	nzone;				/* table of zones */
static int	curzone = -1;
static char	**zone;

#if !defined(HAVE_UNDERBAR_ENVIRON)
#define _environ environ
#endif

extern char **_environ;

static void
_pushTZ(void)
{
    char	**p;

    savetzp = NULL;
    for (p = _environ; *p != NULL; p++) {
	if (strncmp(*p, "TZ=", 3) == 0) {
	    savetz = *p;
	    *p = envtz;
	    savetzp = p;
	    break;
	}
    }
    if (*p == NULL)
	putenv(envtz);
    tzset();
}

static void
_popTZ(void)
{
    if (savetzp != NULL)
	*savetzp = savetz;
    else
	putenv("TZ=");
    tzset();
}

/*
 * Construct TZ=... subject to the constraint that the length of the
 * timezone part is not more than PM_TZ_MAXLEN bytes
 * Assumes TZ= is in the start of tzbuffer and this is not touched.
 * And finally set TZ in the environment.
 */
static char *
__pmSquashTZ(char *tzbuffer)
{
    time_t	now = time(NULL);
    struct tm	*t;
    char	*tzn;

#ifndef IS_MINGW
    time_t	offset; 

    tzset();
    t = localtime(&now);

#ifdef HAVE_ALTZONE
    offset = (t->tm_isdst > 0) ? altzone : timezone;
#elif defined HAVE_STRFTIME_z
    {
	char tzoffset[6]; /* +1200\0 */

	strftime (tzoffset, sizeof (tzoffset), "%z", t);
	offset = -strtol (tzoffset, NULL, 10);
	offset = ((offset/100) * 3600) + ((offset%100) * 60);
    }
#else
    {
	struct tm	*gmt = gmtime(&now);
	offset = (gmt->tm_hour - t->tm_hour) * 3600 +
		 (gmt->tm_min - t->tm_min) * 60;
    }
#endif

    tzn = tzname[(t->tm_isdst > 0)];

    if (offset != 0) {
	int hours = offset / 3600;
	int mins = abs ((offset % 3600) / 60);
	int len = (int) strlen(tzn);

	if (mins == 0) {
	    /* -3 for +HH in worst case */
	    if (len > PM_TZ_MAXLEN-3) len = PM_TZ_MAXLEN-3;
	    snprintf(tzbuffer+3, PM_TZ_MAXLEN, "%*.*s%+d", len, len, tzn, hours);
	}
	else {
	    /* -6 for +HH:MM in worst case */
	    if (len > PM_TZ_MAXLEN-6) len = PM_TZ_MAXLEN-6;
	    snprintf(tzbuffer+3, PM_TZ_MAXLEN, "%*.*s%+d:%02d", len, len, tzn, hours, mins);
	}
    }
    else {
	strncpy(tzbuffer+3, tzn, PM_TZ_MAXLEN);
	tzbuffer[PM_TZ_MAXLEN+4-1] = '\0';
    }
    putenv(tzbuffer);
    return tzbuffer+3;

#else	/* IS_MINGW */
    /*
     * Use the native Win32 API to extract the timezone.  This is
     * a Windows timezone, we want the POSIX style but there's no
     * API, really.  What we've found works, is the same approach
     * the MSYS dll takes - we set TZ their way (below) and then
     * use tzset, then extract.  Note that the %Z and %z strftime
     * parameters do not contain abbreviated names/offsets (they
     * both contain Windows timezone, and both are the same with
     * no TZ).  Note also that putting the Windows name into the
     * environment as TZ does not do anything good (see the tzset
     * MSDN docs).
     */
#define is_upper(c) ((unsigned)(c) - 'A' <= 26)

    TIME_ZONE_INFORMATION tz;
    static char wildabbr[] = "GMT";
    char tzbuf[256], tzoff[64];
    char *cp, *dst, *off;
    wchar_t *src;
    div_t d;

    GetTimeZoneInformation(&tz);
    dst = cp = tzbuf;
    off = tzoff;
    for (src = tz.StandardName; *src; src++)
	if (is_upper(*src)) *dst++ = *src;
    if (cp == dst) {
	/* In Asian Windows, tz.StandardName may not contain
	   the timezone name. */
	strcpy(cp, wildabbr);
	cp += strlen(wildabbr);
    }
    else
	cp = dst;
    d = div(tz.Bias+tz.StandardBias, 60);
    sprintf(cp, "%d", d.quot);
    sprintf(off, "%d", d.quot);
    if (d.rem) {
	sprintf(cp=strchr(cp, 0), ":%d", abs(d.rem));
	sprintf(off=strchr(off, 0), ":%d", abs(d.rem));
    }
    if (tz.StandardDate.wMonth) {
	cp = strchr(cp, 0);
	dst = cp;
	for (src = tz.DaylightName; *src; src++)
	    if (is_upper(*src)) *dst++ = *src;
	if (cp == dst) {
	    /* In Asian Windows, tz.StandardName may not contain
	       the daylight name. */
	    strcpy(tzbuf, wildabbr);
	    cp += strlen(wildabbr);
	}
	else
	    cp = dst;
	d = div(tz.Bias+tz.DaylightBias, 60);
	sprintf(cp, "%d", d.quot);
	if (d.rem)
	    sprintf(cp=strchr(cp, 0), ":%d", abs(d.rem));
	cp = strchr(cp, 0);
	sprintf(cp=strchr(cp, 0), ",M%d.%d.%d/%d",
		tz.DaylightDate.wMonth,
		tz.DaylightDate.wDay,
		tz.DaylightDate.wDayOfWeek,
		tz.DaylightDate.wHour);
	if (tz.DaylightDate.wMinute || tz.DaylightDate.wSecond)
	    sprintf(cp=strchr(cp, 0), ":%d", tz.DaylightDate.wMinute);
	if (tz.DaylightDate.wSecond)
	    sprintf(cp=strchr(cp, 0), ":%d", tz.DaylightDate.wSecond);
	cp = strchr(cp, 0);
	sprintf(cp=strchr(cp, 0), ",M%d.%d.%d/%d",
		tz.StandardDate.wMonth,
		tz.StandardDate.wDay,
		tz.StandardDate.wDayOfWeek,
		tz.StandardDate.wHour);
	if (tz.StandardDate.wMinute || tz.StandardDate.wSecond)
	    sprintf(cp=strchr(cp, 0), ":%d", tz.StandardDate.wMinute);
	if (tz.StandardDate.wSecond)
	    sprintf(cp=strchr(cp, 0), ":%d", tz.StandardDate.wSecond);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "Win32 TZ=%s\n", tzbuf);
#endif

    snprintf(tzbuffer+3, PM_TZ_MAXLEN+4, "%s", tzbuf);
    putenv(tzbuffer);

    tzset();
    t = localtime(&now);
    tzn = tzname[(t->tm_isdst > 0)];

    snprintf(tzbuffer+3, PM_TZ_MAXLEN+4, "%s%s", tzn, tzoff);
    putenv(tzbuffer);

    return tzbuffer+3;
#endif
}

/*
 * __pmTimezone: work out local timezone
 */
char *
__pmTimezone(void)
{
    static char tzbuffer[PM_TZ_MAXLEN+4] = "TZ=";
    char * tz = getenv("TZ");

    if (tz == NULL || tz[0] == ':') {
	/* NO TZ in the environment - invent one. If TZ starts with a colon,
	 * it's an Olson-style TZ and it does not supported on all IRIXes, so
	 * squash it into a simple one (pv#788431). */
	tz = __pmSquashTZ(tzbuffer);
    } else if (strlen(tz) > PM_TZ_MAXLEN) {
	/* TZ is too long to fit into the internal PCP timezone structs
	 * let's try to sqash it a bit */
	char *tb;

	if ((tb = strdup(tz)) == NULL) {
	    /* sorry state of affairs, go squash w/out malloc */
	    tz = __pmSquashTZ(tzbuffer);
	}
	else {
	    char *ptz = tz;
	    char *zeros;
	    char *end = tb;

	    while ((zeros = strstr(ptz, ":00")) != NULL) {
		strncpy (end, ptz, zeros-ptz);
		end += zeros-ptz;
		*end = '\0';
		ptz = zeros+3;
	    }

	    if (strlen(tb) > PM_TZ_MAXLEN) { 
		/* Still too long - let's pretend it's Olson */
		tz=__pmSquashTZ(tzbuffer);
	    } else {
		strcpy (tzbuffer+3, tb);
		putenv (tzbuffer);
		tz = tzbuffer+3;
	    }

	    free (tb);
	}
    }

    return tz;
}

int
pmUseZone(const int tz_handle)
{
    if (tz_handle < 0 || tz_handle >= nzone)
	return -1;
    
    curzone = tz_handle;
    strcpy(&envtz[3], zone[curzone]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseZone(%d) tz=%s\n", curzone, zone[curzone]);
#endif

    return 0;
}

int
pmNewZone(const char *tz)
{
    int		len;
    int		hack = 0;

    len = (int)strlen(tz);
    if (len == 3) {
	/*
	 * things like TZ=GMT may be broken in libc, particularly
	 * in _ltzset() of time_comm.c, where changes to TZ are
	 * sometimes not properly reflected.
	 * TZ=GMT+0 avoids the problem.
	 */
	len += 2;
	hack = 1;
    }

    if (len+4 > envtzlen) {
	/* expand buffer for env */
	if (envtz != NULL)
	    free(envtz);
	envtzlen = len+4;
	envtz = (char *)malloc(envtzlen);
	strcpy(envtz, "TZ=");
    }
    strcpy(&envtz[3], tz);
    if (hack)
	/* see above */
	strcpy(&envtz[6], "+0");

    curzone = nzone++;
    zone = (char **)realloc(zone, nzone * sizeof(char *));
    if (zone == NULL) {
	__pmNoMem("pmNewZone", nzone * sizeof(char *), PM_FATAL_ERR);
    }
    zone[curzone] = strdup(&envtz[3]);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewZone(%s) -> %d\n", zone[curzone], curzone);
#endif

    return curzone;
}

int
pmNewContextZone(void)
{
    __pmContext	*ctxp;
    int		sts;
    static pmID	pmid = 0;
    pmResult	*rp;

    if ((ctxp = __pmHandleToPtr(pmWhichContext())) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_ARCHIVE)
	sts = pmNewZone(ctxp->c_archctl->ac_log->l_label.ill_tz);
    else if (ctxp->c_type == PM_CONTEXT_LOCAL)
	/* from env, not PMCD */
	sts = pmNewZone(__pmTimezone());
    else {
	/* assume PM_CONTEXT_HOST */
	if (pmid == 0) {
	    char	*name = "pmcd.timezone";
	    if ((sts = pmLookupName(1, &name, &pmid)) < 0)
		return sts;
	}
	if ((sts = pmFetch(1, &pmid, &rp)) >= 0) {
	    if (rp->vset[0]->numval == 1 && 
		(rp->vset[0]->valfmt == PM_VAL_DPTR || rp->vset[0]->valfmt == PM_VAL_SPTR))
		sts = pmNewZone((char *)rp->vset[0]->vlist[0].value.pval->vbuf);
	    else
		sts = PM_ERR_VALUE;
	    pmFreeResult(rp);
	}
    }
    
    return sts;
}

char *
pmCtime(const time_t *clock, char *buf)
{
#if !defined(IS_SOLARIS) && !defined(IS_MINGW)
    static struct tm	tbuf;
#endif
    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	strcpy(buf, asctime(localtime(clock)));
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	strcpy(buf, asctime(localtime(clock)));
#else
	asctime_r(localtime_r(clock, &tbuf), buf);
#endif
    }

    return buf;
}

struct tm *
pmLocaltime(const time_t *clock, struct tm *result)
{
#if defined(IS_SOLARIS) || defined(IS_MINGW)
    struct tm	*tmp;
#endif
    if (curzone >= 0) {
	_pushTZ();
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
	_popTZ();
    }
    else {
#if defined(IS_SOLARIS) || defined(IS_MINGW)
	tmp = localtime(clock);
        memcpy(result, tmp, sizeof(*result));
#else
	localtime_r(clock, result);
#endif
    }

    return result;
}

time_t
__pmMktime(struct tm *timeptr)
{
    time_t	ans;

    if (curzone >= 0) {
	_pushTZ();
	ans = mktime(timeptr);
	_popTZ();
    }
    else
	ans = mktime(timeptr);

    return ans;
}

int
pmWhichZone(char **tz)
{
    if (curzone >= 0)
	*tz = zone[curzone];

    return curzone;
}
