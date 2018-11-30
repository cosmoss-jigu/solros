#ifndef _PCNLINK_API_H
#define _PCNLINK_API_H
/**
 * default configurations
 */
#define PCNLINK_PCNSRV_ID                0
#define PCNLINK_PCNSRV_PORT          20000
#define PCNLINK_ID                       1
#define PCNLINK_PORT                 25000
#define PCNLINK_PER_CHN_TX_Q_SIZE    (32 * 1024 * 1024)
#define PCNLINK_IN_Q_REAP_THRESHOLD  (1024 * 1024)

/**
 * pcnlink APIs
 */
int  pcnlink_up(int pcnsrv_id,  int pcnsrv_port,
		int pcnlink_id, int pcnlink_port,
		int qsize);
void pcnlink_down(void);

#include <pcnlink-kapi.h>

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
# include <pcnlink-uapi.h>
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
#endif /* _PCNLINK_API_H */
