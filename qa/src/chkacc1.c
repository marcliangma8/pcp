/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "localconfig.h"

int
main()
{
    int			s, sts, op, host;
    unsigned int	i;
    char		name[4*8 + 7 + 1]; /* handles full IPv6 address, if supported */
#if PCP_VER >= 3611
    __pmSockAddr	*inaddr;
#else
    __pmInAddr		inaddr;
    __pmIPAddr		ipaddr;
#endif

    sts = 0;
    for (op = 0; op < WORD_BIT; op++) {
	if ((s = __pmAccAddOp(1 << op)) < 0) {
	    printf("Bad op %d: %s\n", op, strerror(errno));
	    sts = s;
	}
	if ((s = __pmAccAddOp(1 << op)) >= 0) {
	    printf("duplicate op test failed for op %d\n", op);
	    sts = -EINVAL;
	}
    }
    if (sts < 0)
	exit(1);

    for (host = 0; host < WORD_BIT; host++) {
	sprintf(name, "155.%d.%d.%d", host * 3, 17+host, host);
	if ((s = __pmAccAddHost(name, 1 << host, 1 << host, 0)) < 0) {
	    printf("cannot add inet host for op%d: %s\n", host, strerror(s));
	    sts = s;
	}
#if PCP_VER >= 3611
	sprintf(name, "fec0::%x:%x:%x:%x:%x:%x",
		host * 3, 17+host, host,
		host * 3, 17+host, host);
	if ((s = __pmAccAddHost(name, 1 << host, 1 << host, 0)) < 0) {
	    printf("cannot add IPv6 host for op%d: %s\n", host, strerror(s));
	    sts = s;
	}
#endif
    }
    if (sts < 0)
	exit(1);

    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    for (host = 0; host < WORD_BIT; host++) {
	char	buf[20];

	sprintf(buf, "%d.%d.%d.%d", 155, host * 3, 17+host, host);
#if PCP_VER >= 3611
	if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
	    printf("insufficient memory\n");
	    continue;
	}
	sts = __pmAccAddClient(inaddr, &i);
	__pmSockAddrFree(inaddr);
#else
	inet_aton(buf, &inaddr);
	ipaddr = __pmInAddrToIPAddr(&inaddr);
	sts = __pmAccAddClient(ipaddr, &i);
#endif

	if (sts < 0) {
	    printf("add inet client from host %d: %s\n", host, pmErrStr(sts));
	    continue;
	}
	else if (i != (1 << host))
	    printf("inet host %d: __pmAccAddClient returns denyOpsResult 0x%x (expected 0x%x)\n",
		   host, i, 1 << host);
    }

#if PCP_VER >= 3611
    for (host = 0; host < WORD_BIT; host++) {
	char	buf[4*8 + 7 + 1]; /* handles full IPv6 address */

	sprintf(buf, "fec0::%x:%x:%x:%x:%x:%x",
		host * 3, 17+host, host,
		host * 3, 17+host, host);
	if ((inaddr =__pmStringToSockAddr(buf)) == NULL) {
	    printf("insufficient memory\n");
	    continue;
	}
	sts = __pmAccAddClient(inaddr, &i);
	__pmSockAddrFree(inaddr);

	if (sts < 0) {
	    printf("add IPv6 client from host %d: %s\n", host, pmErrStr(sts));
	    continue;
	}
	else if (i != (1 << host))
	    printf("IPv6 host %d: __pmAccAddClient returns denyOpsResult 0x%x (expected 0x%x)\n",
		   host, i, 1 << host);
    }
#endif
    
    putc('\n', stderr);
    __pmAccDumpHosts(stderr);

    exit(0);
}