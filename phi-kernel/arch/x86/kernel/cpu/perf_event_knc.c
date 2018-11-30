/* Driver for Intel Xeon Phi "Knights Corner" PMU */

#include <linux/perf_event.h>
#include <linux/types.h>

static const u64 knc_perfmon_event_map[] =
{
  [PERF_COUNT_HW_CPU_CYCLES]		= 0x002a,
  [PERF_COUNT_HW_INSTRUCTIONS]		= 0x0016,
  [PERF_COUNT_HW_CACHE_REFERENCES]	= 0x0028,
  [PERF_COUNT_HW_CACHE_MISSES]		= 0x0029,
  [PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= 0x0012,
  [PERF_COUNT_HW_BRANCH_MISSES]		= 0x002b,
};

static const u64 knc_hw_cache_event_ids
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] =
{
 [ C(L1D) ] = {
	[ C(OP_READ) ] = {
		/* On Xeon Phi event "0" is a valid DATA_READ          */
		/*   (L1 Data Cache Reads) Instruction.                */
		/* We code this as ARCH_PERFMON_EVENTSEL_INT as this   */
		/* bit will always be set in x86_pmu_hw_config().      */
		[ C(RESULT_ACCESS) ] = ARCH_PERFMON_EVENTSEL_INT,
						/* DATA_READ           */
		[ C(RESULT_MISS)   ] = 0x0003,	/* DATA_READ_MISS      */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x0001,	/* DATA_WRITE          */
		[ C(RESULT_MISS)   ] = 0x0004,	/* DATA_WRITE_MISS     */
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x0011,	/* L1_DATA_PF1         */
		[ C(RESULT_MISS)   ] = 0x001c,	/* L1_DATA_PF1_MISS    */
	},
 },
 [ C(L1I ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x000c,	/* CODE_READ          */
		[ C(RESULT_MISS)   ] = 0x000e,	/* CODE_CACHE_MISS    */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x0,
		[ C(RESULT_MISS)   ] = 0x0,
	},
 },
 [ C(LL  ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0x10cb,	/* L2_READ_MISS */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x10cc,	/* L2_WRITE_HIT */
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x10fc,	/* L2_DATA_PF2      */
		[ C(RESULT_MISS)   ] = 0x10fe,	/* L2_DATA_PF2_MISS */
	},
 },
 [ C(DTLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = ARCH_PERFMON_EVENTSEL_INT,
						/* DATA_READ */
						/* see note on L1 OP_READ */
		[ C(RESULT_MISS)   ] = 0x0002,	/* DATA_PAGE_WALK */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x0001,	/* DATA_WRITE */
		[ C(RESULT_MISS)   ] = 0x0002,	/* DATA_PAGE_WALK */
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x0,
		[ C(RESULT_MISS)   ] = 0x0,
	},
 },
 [ C(ITLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x000c,	/* CODE_READ */
		[ C(RESULT_MISS)   ] = 0x000d,	/* CODE_PAGE_WALK */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
 [ C(BPU ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0012,	/* BRANCHES */
		[ C(RESULT_MISS)   ] = 0x002b,	/* BRANCHES_MISPREDICTED */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
};


static u64 knc_pmu_event_map(int hw_event)
{
	return knc_perfmon_event_map[hw_event];
}

static struct event_constraint knc_event_constraints[] =
{
	INTEL_EVENT_CONSTRAINT(0xc3, 0x1),	/* HWP_L2HIT */
	INTEL_EVENT_CONSTRAINT(0xc4, 0x1),	/* HWP_L2MISS */
	INTEL_EVENT_CONSTRAINT(0xc8, 0x1),	/* L2_READ_HIT_E */
	INTEL_EVENT_CONSTRAINT(0xc9, 0x1),	/* L2_READ_HIT_M */
	INTEL_EVENT_CONSTRAINT(0xca, 0x1),	/* L2_READ_HIT_S */
	INTEL_EVENT_CONSTRAINT(0xcb, 0x1),	/* L2_READ_MISS */
	INTEL_EVENT_CONSTRAINT(0xcc, 0x1),	/* L2_WRITE_HIT */
	INTEL_EVENT_CONSTRAINT(0xce, 0x1),	/* L2_STRONGLY_ORDERED_STREAMING_VSTORES_MISS */
	INTEL_EVENT_CONSTRAINT(0xcf, 0x1),	/* L2_WEAKLY_ORDERED_STREAMING_VSTORE_MISS */
	INTEL_EVENT_CONSTRAINT(0xd7, 0x1),	/* L2_VICTIM_REQ_WITH_DATA */
	INTEL_EVENT_CONSTRAINT(0xe3, 0x1),	/* SNP_HITM_BUNIT */
	INTEL_EVENT_CONSTRAINT(0xe6, 0x1),	/* SNP_HIT_L2 */
	INTEL_EVENT_CONSTRAINT(0xe7, 0x1),	/* SNP_HITM_L2 */
	INTEL_EVENT_CONSTRAINT(0xf1, 0x1),	/* L2_DATA_READ_MISS_CACHE_FILL */
	INTEL_EVENT_CONSTRAINT(0xf2, 0x1),	/* L2_DATA_WRITE_MISS_CACHE_FILL */
	INTEL_EVENT_CONSTRAINT(0xf6, 0x1),	/* L2_DATA_READ_MISS_MEM_FILL */
	INTEL_EVENT_CONSTRAINT(0xf7, 0x1),	/* L2_DATA_WRITE_MISS_MEM_FILL */
	INTEL_EVENT_CONSTRAINT(0xfc, 0x1),	/* L2_DATA_PF2 */
	INTEL_EVENT_CONSTRAINT(0xfd, 0x1),	/* L2_DATA_PF2_DROP */
	INTEL_EVENT_CONSTRAINT(0xfe, 0x1),	/* L2_DATA_PF2_MISS */
	INTEL_EVENT_CONSTRAINT(0xff, 0x1),	/* L2_DATA_HIT_INFLIGHT_PF2 */
	EVENT_CONSTRAINT_END
};

#define KNC_ENABLE_COUNTER0			0x00000001
#define KNC_ENABLE_COUNTER1			0x00000002

#define MSR_KNC_IA32_PERF_GLOBAL_STATUS		0x0000002d
#define MSR_KNC_IA32_PERF_GLOBAL_OVF_CONTROL	0x0000002e
#define MSR_KNC_IA32_PERF_GLOBAL_CTRL		0x0000002f

static void knc_pmu_disable_all(void)
{
	u64 val;

	rdmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, val);
	val &= ~(KNC_ENABLE_COUNTER0|KNC_ENABLE_COUNTER1);
	wrmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, val);

	intel_pmu_pebs_disable_all();
	intel_pmu_lbr_disable_all();
}

static void knc_pmu_enable_all(int added)
{
	u64 val;

	intel_pmu_pebs_enable_all();
	intel_pmu_lbr_enable_all();

	rdmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, val);
	val |= (KNC_ENABLE_COUNTER0|KNC_ENABLE_COUNTER1);
	wrmsrl(MSR_KNC_IA32_PERF_GLOBAL_CTRL, val);
}

static inline void
knc_pmu_disable_event(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 val;

	val = hwc->config;
	val &= ~ARCH_PERFMON_EVENTSEL_ENABLE;
	(void)checking_wrmsrl(hwc->config_base + hwc->idx, val);

	if (unlikely(event->attr.precise_ip))
		intel_pmu_pebs_disable(event);
}

static void knc_pmu_enable_event(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 val;

	val = hwc->config;
	val |= ARCH_PERFMON_EVENTSEL_ENABLE;

	(void)checking_wrmsrl(hwc->config_base + hwc->idx, val);

	if (unlikely(event->attr.precise_ip))
		intel_pmu_pebs_enable(event);
}

static inline u64 knc_pmu_get_status(void)
{
	u64 status;

        rdmsrl(MSR_KNC_IA32_PERF_GLOBAL_STATUS, status);

	return status;
}

static inline void knc_pmu_ack_status(u64 ack)
{
        wrmsrl(MSR_KNC_IA32_PERF_GLOBAL_OVF_CONTROL, ack);
}

/*
 * Save and restart an expired event. Called by NMI contexts,
 * so it has to be careful about preempting normal event ops:
 */
static int knc_pmu_save_and_restart(struct perf_event *event)
{
  x86_perf_event_update(event);
  return x86_perf_event_set_period(event);
}



static int knc_pmu_handle_irq(struct pt_regs *regs)
{
	struct perf_sample_data data;
	struct cpu_hw_events *cpuc;
	int bit, loops, added;
	u64 ack,status;
	int handled;

	perf_sample_data_init(&data, 0);

	cpuc = &__get_cpu_var(cpu_hw_events);
	added = cpuc->n_added;

	knc_pmu_disable_all();

	status = knc_pmu_get_status();
	if (!status) {
		knc_pmu_enable_all(added);
		return 0;
	}

	loops = 0;
again:
	if (++loops > 100) {
		WARN_ONCE(1, "perfevents: irq loop stuck!\n");
		perf_event_print_debug();
		goto done;
	}

	inc_irq_stat(apic_perf_irqs);
	ack = status;

	intel_pmu_lbr_read();

	/*
	 * PEBS overflow sets bit 62 in the global status register
	 */
	if (__test_and_clear_bit(62, (unsigned long *)&status)) {
		handled++;
		x86_pmu.drain_pebs(regs);
	}

	for_each_set_bit(bit, (unsigned long *)&status, X86_PMC_IDX_MAX) {
		struct perf_event *event = cpuc->events[bit];

		handled++;

		if (!test_bit(bit, cpuc->active_mask))
			continue;

		if (!knc_pmu_save_and_restart(event))
			continue;

		data.period = event->hw.last_period;

		if (perf_event_overflow(event, 1, &data, regs))
		  x86_pmu_stop(event, 0);
	}

        knc_pmu_ack_status(ack);

	/*                                      
	 * Repeat if there is more work to be done:
	 */
	status = knc_pmu_get_status();
	if (status)
		goto again;

done:
	knc_pmu_enable_all(added);

	return 1;
}


static __initconst const struct x86_pmu knc_pmu = {
	.name			= "knc",
	.handle_irq		= knc_pmu_handle_irq,
	.disable_all		= knc_pmu_disable_all,
	.enable_all		= knc_pmu_enable_all,
	.enable			= knc_pmu_enable_event,
	.disable		= knc_pmu_disable_event,
	.hw_config		= x86_pmu_hw_config,
	.schedule_events	= x86_schedule_events,
	.eventsel		= MSR_KNC_EVNTSEL0,
	.perfctr		= MSR_KNC_PERFCTR0,
	.event_map		= knc_pmu_event_map,
	.max_events             = ARRAY_SIZE(knc_perfmon_event_map),
	.apic			= 1,
	.max_period		= (1ULL << 39) - 1,
	.version		= 0,
	.num_counters		= 2,
	.cntval_bits		= 40,
	.cntval_mask		= (1ULL << 40) - 1,
	.get_event_constraints	= x86_get_event_constraints,
	.event_constraints	= knc_event_constraints,
};

__init int knc_pmu_init(void)
{
	x86_pmu = knc_pmu;

	memcpy(hw_cache_event_ids, knc_hw_cache_event_ids, 
		sizeof(hw_cache_event_ids));

	return 0;
}
 
