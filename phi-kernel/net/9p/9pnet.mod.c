#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x3ef6acc2, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x8487a2b6, __VMLINUX_SYMBOL_STR(flush_work) },
	{ 0x2d3385d3, __VMLINUX_SYMBOL_STR(system_wq) },
	{ 0x3356b90b, __VMLINUX_SYMBOL_STR(cpu_tss) },
	{ 0x8130d5db, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x4791ca02, __VMLINUX_SYMBOL_STR(perf_tp_event) },
	{ 0xda3e43d1, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x754d539c, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x1b6314fd, __VMLINUX_SYMBOL_STR(in_aton) },
	{ 0xb5dcab5b, __VMLINUX_SYMBOL_STR(remove_wait_queue) },
	{ 0xacf4d843, __VMLINUX_SYMBOL_STR(match_strdup) },
	{ 0x2008b31e, __VMLINUX_SYMBOL_STR(sock_release) },
	{ 0xaa3588f3, __VMLINUX_SYMBOL_STR(copy_from_iter) },
	{ 0x88bfa7e, __VMLINUX_SYMBOL_STR(cancel_work_sync) },
	{ 0x44e9a829, __VMLINUX_SYMBOL_STR(match_token) },
	{ 0x735ab831, __VMLINUX_SYMBOL_STR(init_user_ns) },
	{ 0x85df9b6c, __VMLINUX_SYMBOL_STR(strsep) },
	{ 0xc499ae1e, __VMLINUX_SYMBOL_STR(kstrdup) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x53654c6a, __VMLINUX_SYMBOL_STR(make_kgid) },
	{ 0xf432dd3d, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x7f66b0d3, __VMLINUX_SYMBOL_STR(kernel_read) },
	{ 0xbed20a97, __VMLINUX_SYMBOL_STR(trace_define_field) },
	{ 0x43368495, __VMLINUX_SYMBOL_STR(ftrace_event_buffer_commit) },
	{ 0x7b8e198f, __VMLINUX_SYMBOL_STR(from_kuid) },
	{ 0x614dabe, __VMLINUX_SYMBOL_STR(idr_destroy) },
	{ 0x8f64aa4, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x6798a936, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x449ad0a7, __VMLINUX_SYMBOL_STR(memcmp) },
	{ 0x9963d2d6, __VMLINUX_SYMBOL_STR(iov_iter_kvec) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x73d6ce06, __VMLINUX_SYMBOL_STR(from_kgid) },
	{ 0x84ffea8b, __VMLINUX_SYMBOL_STR(idr_preload) },
	{ 0xb74787a0, __VMLINUX_SYMBOL_STR(idr_alloc) },
	{ 0x4e3567f7, __VMLINUX_SYMBOL_STR(match_int) },
	{ 0x9917cdcf, __VMLINUX_SYMBOL_STR(fput) },
	{ 0xfb73fb98, __VMLINUX_SYMBOL_STR(idr_remove) },
	{ 0x77fba6d9, __VMLINUX_SYMBOL_STR(ftrace_event_reg) },
	{ 0xf2d06515, __VMLINUX_SYMBOL_STR(module_put) },
	{ 0x2f2f7bc0, __VMLINUX_SYMBOL_STR(idr_find_slowpath) },
	{ 0x504c8571, __VMLINUX_SYMBOL_STR(make_kuid) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xfb6af58d, __VMLINUX_SYMBOL_STR(recalc_sigpending) },
	{ 0x6b2dc060, __VMLINUX_SYMBOL_STR(dump_stack) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x5a80c8ca, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x9327f5ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0x3faa7577, __VMLINUX_SYMBOL_STR(sock_alloc_file) },
	{ 0xe8e9fd26, __VMLINUX_SYMBOL_STR(ftrace_event_buffer_reserve) },
	{ 0xcf21d241, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x868fab52, __VMLINUX_SYMBOL_STR(event_triggers_call) },
	{ 0x34f22f94, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x5860aad4, __VMLINUX_SYMBOL_STR(add_wait_queue) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0xc9d8f656, __VMLINUX_SYMBOL_STR(trace_event_raw_init) },
	{ 0x42bb8d80, __VMLINUX_SYMBOL_STR(perf_trace_buf_prepare) },
	{ 0x86763a99, __VMLINUX_SYMBOL_STR(kernel_bind) },
	{ 0x67c409d5, __VMLINUX_SYMBOL_STR(fget) },
	{ 0x53569707, __VMLINUX_SYMBOL_STR(this_cpu_off) },
	{ 0x9dd9dc82, __VMLINUX_SYMBOL_STR(put_page) },
	{ 0x1e786525, __VMLINUX_SYMBOL_STR(iov_iter_advance) },
	{ 0xfa66f77c, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0xfcceecab, __VMLINUX_SYMBOL_STR(copy_to_iter) },
	{ 0x420be990, __VMLINUX_SYMBOL_STR(ftrace_print_symbols_seq) },
	{ 0x2e0d2f7f, __VMLINUX_SYMBOL_STR(queue_work_on) },
	{ 0x11caddc7, __VMLINUX_SYMBOL_STR(trace_seq_printf) },
	{ 0xb0e602eb, __VMLINUX_SYMBOL_STR(memmove) },
	{ 0x1cbea8fd, __VMLINUX_SYMBOL_STR(idr_init) },
	{ 0xf088b9bb, __VMLINUX_SYMBOL_STR(ftrace_raw_output_prep) },
	{ 0x32748413, __VMLINUX_SYMBOL_STR(try_module_get) },
	{ 0x38e7ac17, __VMLINUX_SYMBOL_STR(__sock_create) },
	{ 0x95c396c, __VMLINUX_SYMBOL_STR(vfs_write) },
	{ 0xe914e41e, __VMLINUX_SYMBOL_STR(strcpy) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "BDC2934A30F60A8EDE382E5");
