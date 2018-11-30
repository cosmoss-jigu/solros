# MIC_PM driver 
obj-$(CONFIG_MIC_PM)			+= micpm.o
micpm-y					+= mic_pm.o mic_tmu.o micpm_proc.o
micpm-$(CONFIG_MIC_CPUIDLE)		+= mic_cpuidle.o corec6.o pc6_trampoline.o corec6_trampoline.o mic_menu.o 
micpm-$(CONFIG_MIC_CPUFREQ)		+= mic_cpufreq.o

#micpm-objs :=  mic_pm.o mic_cpuidle.o corec6.o pc6_trampoline.o corec6_trampoline.o mic_menu.o mic_tmu.o

