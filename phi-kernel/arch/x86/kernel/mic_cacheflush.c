/* * Copyright (c) Intel Corporation (2011).
*
* Disclaimer: The codes contained in these modules may be specific to the
* Intel Software Development Platform codenamed: Knights Ferry, and the 
* Intel product codenamed: Knights Corner, and are not backward compatible 
* with other Intel products. Additionally, Intel will NOT support the codes 
* or instruction set in future products.
*
* Intel offers no warranty of any kind regarding the code.  This code is
* licensed on an "AS IS" basis and Intel is not obligated to provide any support,
* assistance, installation, training, or other services of any kind.  Intel is 
* also not obligated to provide any updates, enhancements or extensions.  Intel 
* specifically disclaims any warranty of merchantability, non-infringement, 
* fitness for any particular purpose, and any other warranty.
*
* Further, Intel disclaims all liability of any kind, including but not
* limited to liability for infringement of any proprietary rights, relating
* to the use of the code, even if Intel is notified of the possibility of
* such liability.  Except as expressly stated in an Intel license agreement
* provided with this code and agreed upon with Intel, no license, express
* or implied, by estoppel or otherwise, to any intellectual property rights
* is granted herein.
*/

#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include <asm/pgtable.h>
#include <asm/vsyscall.h>
#include <asm/cacheflush.h>

#define CBOX_TR5_MSR		0x00000007
#define CBOX_TR12_MSR		0x0000000E
#define TR5_CMD_INVL		0x00000003 /* Invalidate */
#define DIS_ICACHE_SNOOP	0x00010000
#define ENB_ICACHE_SNOOP	~(DIS_ICACHE_SNOOP)

#define	ICACHE		(1<<0)		/* flush instruction cache        */
#define	DCACHE		(1<<1)		/* writeback and flush data cache */
#define	BCACHE		(ICACHE|DCACHE)	/* flush both caches              */
#define ICACHE_CMDLINE	"icache_snoop_off"

#define is_nx_support (__supported_pte_mask & _PAGE_NX)
#define pte_user(pte) (pte_flags(pte) & _PAGE_USER)
#define pte_no_exec(pte)  (pte_flags(pte) & _PAGE_NX)
#define IS_READONLY_EXEC(val) ((val) & (_PAGE_PRESENT | \
		_PAGE_USER | _PAGE_ACCESSED))

/* 0 - OFF, 1 - ON*/
static int icache_snoop = 1;
static int cmd_parsed = 0;
static void icache_flush_handler(void);
static void icache_flush(void *unused);

/*
 * Parse the kernel command line to check if icache snoop
 * needs to be disabled or not.
 */
static void parse_cmdline_icache(const char *cmd_line)
{
	char *parsed;
	int len = strlen(ICACHE_CMDLINE);

	if (!cmd_parsed) {
		parsed = strstr(cmd_line, ICACHE_CMDLINE);
		if (parsed) {
			if (!strncmp(parsed, ICACHE_CMDLINE, len)) {
				icache_snoop = 0;
			}
		}
		cmd_parsed = 1;
	}
}

/*
 * Disable or enable icache snoop
 */
void set_icache_snoop_handler(void)
{
	uint32_t tr12;

	rdmsrl(CBOX_TR12_MSR, tr12);

	if (!icache_snoop)
		tr12 |= DIS_ICACHE_SNOOP;
	else
		tr12 &= ENB_ICACHE_SNOOP;

	wrmsrl(CBOX_TR12_MSR, tr12);
	/* Without the flush below an AP can hang at boot time */
	icache_flush_handler();
}

void set_icache_snoop(void *unused)
{
	set_icache_snoop_handler();
}

/*
 * This is called by BSP and the APs during initialization
 * to disable icache. Only disable if the command line ICACHE_CMDLINE
 * is present
 */
void __cpuinit disable_icache_snoop(void)
{
	if (!cmd_parsed)
		parse_cmdline_icache(saved_command_line);

	set_icache_snoop_handler();
}

/*
 * IPI handler to flush icache. Executed on each CPU
 */
static void icache_flush_handler(void)
{
	uint32_t tr5;

	rdmsrl(CBOX_TR5_MSR, tr5);
	tr5 |= TR5_CMD_INVL;
	wrmsrl(CBOX_TR5_MSR, tr5);
}

static void icache_flush(void *unused)
{
	icache_flush_handler();
}

/*
 * This function initiates iCache flush. Call this function to
 * to flush icaches on MIC
 */
void mic_flush_icache(void)
{
	if (icache_snoop)
		return;

	preempt_disable();
	/* flush icache on the current CPU */
	icache_flush_handler();
	smp_call_function_many(cpu_online_mask, icache_flush, NULL, 1);
	preempt_enable();
}

EXPORT_SYMBOL(mic_flush_icache);

static __always_inline void mic_flush_icache_lazy(struct page *page)
{
	/*
	 * PG_arch_1 is cleared when flush dcache is called
	 */
	if (test_bit(PG_arch_1, &page->flags))
		return;

	mic_flush_icache();

	/* mark the page as clean */
	set_bit(PG_arch_1, &page->flags);
}

/*
 * This is lazy way to flush icache when the CPU has no NX feature enabled.
 * This is called from mk_pte.
 *
 * Note:mic_flush_icache_nx and mic_flush_icache_nonx are not completely
 * equivalent in their functionality because the previous pte value
 * could not be passed as an argument to this function, so condition
 * (page swapin or new page or page migration ||
 * copy_on_write with page copying) will not be checked, resulting
 * in extra icache flushes.
 */
void mic_flush_icache_nonx(struct page *page, pgprotval_t protval)
{
	if (icache_snoop ||  is_nx_support)
		return;

	/*
	 * Refer asm/pgtable_types.h
	 * Flush if the page is of PAGE_READONLY_EXEC
	 */
	if (IS_READONLY_EXEC(protval))
		mic_flush_icache_lazy(page);
 }

/*
 * This is lazy way to flush icache provided the CPU has the NX feature enabled.
 * This is called from set_pte.
 */
void mic_flush_icache_nx(pte_t *ptep, pte_t pte)
{
	/*
	 * Donot continue if the icache snoop is enabled
	 * or if the NX feature doesnt exist
	 */
	if(icache_snoop || !is_nx_support)
		return;

	/*
	 * Similar to the ia64 set_pte code
	 * We only flush and set PG_arch_1 bit if the page is
	 * present && page is user page && has backing page struct
	 * && page is executable &&
	 * (page swapin or new page or page migration ||
	 * copy_on_write with page copying)
	 */
	if (pte_present(pte) && pte_user(pte) && pfn_valid(pte_pfn(pte)) &&
		!pte_no_exec(pte) && (!pte_present(*ptep) ||
		pte_pfn(*ptep) != pte_pfn(pte)))
		mic_flush_icache_lazy(pte_page(pte));
}

/* mark the page dirty w.r.t the icache so that next set_pte flushes the cache*/
void mic_icache_dirty(struct page *page)
{
	if(icache_snoop || !is_nx_support)
		return;

	clear_bit(PG_arch_1, &(page)->flags);
}

EXPORT_SYMBOL(mic_icache_dirty);

/*
 * Get the current icache snoop status 0 - off, 1 - on
 */
static int icache_snoop_get (void *data, u64 *val)
{
	*val = icache_snoop;
	return 0;
}

/*
 * Set the icache snoop  0 - off, 1 - on
 */
static int icache_snoop_set (void *data, u64 val)
{
	if (icache_snoop != (!!val)) {
		preempt_disable();
		icache_snoop = !!val;
		set_icache_snoop_handler();
		smp_call_function_many(cpu_online_mask, set_icache_snoop, NULL, 1);
		preempt_enable();
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(icache_snoop_fops, icache_snoop_get,
			icache_snoop_set, "%llu\n");

/*
 * Initialize the debugfs to enable/disable icache snoop during runtime
 */
static int __init icache_init(void)
{
	struct dentry *entry;

	entry = debugfs_create_file("icache_snoop", 0600, NULL, NULL,
					 &icache_snoop_fops);

	if (!IS_ERR_OR_NULL(entry))
		 return 0;

	return entry ? PTR_ERR(entry) : -ENODEV;
}

arch_initcall(icache_init);

/*
 * Syscall implementation for icache flush
 */
asmlinkage int sys_cacheflush(void *__addr, __const int __nbytes, __const int __op)
{
	int ret = 0;

	switch (__op) {
		case ICACHE:
			flush_icache_range((unsigned long)__addr,
				 (unsigned long)__addr + __nbytes);
			break;
		default:
			ret = -EINVAL;
	}

	return ret;
}
