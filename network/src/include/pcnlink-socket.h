#ifndef _PCNLINK_SOCKET_H
#define _PCNLINK_SOCKET_H
#include <sys/socket.h>

#ifdef TARGET_ARCH_K1OM
/* redefine pcnlink and max for Xeon Phi */
# undef  AF_MAX
# undef  PF_PCNLINK
# undef  PF_MAX
# define AF_MAX          42
# define PF_PCNLINK      41
# define PF_MAX          AF_MAX
#else
# define PF_PCNLINK      PF_INET
#endif /* TARGET_ARCH_K1OM */

#endif /* _PCNLINK_SOCKET_H */

