/* * Copyright (c) 2010 - 2012 Intel Corporation.
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
/*
 * $Id: kl_module.h 1122 2004-12-21 23:26:23Z tjm $
 *
 * This file is part of libklib.
 * A library which provides access to Linux system kernel dumps.
 *
 * Created by Silicon Graphics, Inc.
 * Contributions by IBM, NEC, and others
 *
 * Copyright (C) 1999 - 2002 Silicon Graphics, Inc. All rights reserved.
 * Copyright (C) 2001, 2002 IBM Deutschland Entwicklung GmbH, IBM Corporation
 * Copyright 2000 Junichi Nomura, NEC Solutions <j-nomura@ce.jp.nec.com>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version. See the file COPYING for more
 * information.
 */

#ifndef __KL_MODULE_H
#define __KL_MODULE_H

/*
 * insmod generates ksymoops
 *
 */

typedef struct kl_modinfo_s {
	char *modname;         /* name of module as loaded in dump */
	/* store ksym info for all modules in a linked list */
	struct kl_modinfo_s *next;
	char *object_file;     /* name of file that module was loaded from*/
	                       /* ? possibly store modtime and version here ? */
	uint64_t header;       /* address of module header */
	uint64_t mtime;        /* time of last modification of object_file */
	uint32_t version;      /* kernel version that module was compiled for */
	uint64_t text_sec;     /* address of text section */
	uint64_t text_len;     /* length of text section */
	uint64_t data_sec;     /* address of data section */
	uint64_t data_len;     /* length of data section */
	uint64_t rodata_sec;   /* address of rodata section */
	uint64_t rodata_len;   /* length of rodata section */
	uint64_t bss_sec;      /* address of rodata section */
	uint64_t bss_len;      /* length of rodata section */
	char *ksym_object;     /* ksym for object */
	char *ksym_text_sec;   /* ksym for its text section */
	char *ksym_data_sec;   /* ksym for its data section */
	char *ksym_rodata_sec; /* ksym for its rodata section */
	char *ksym_bss_sec;    /* ksym for its bss sectio */
} kl_modinfo_t;

int  kl_get_module(char*, kaddr_t*, void**);
int  kl_get_module_2_6(char*, kaddr_t*, void**);
int  kl_get_modname(char**, void*);
int  kl_new_get_modname(char**, void*);
void kl_free_modinfo(kl_modinfo_t**);
int  kl_new_modinfo(kl_modinfo_t**, void*);
int  kl_set_modinfo(kaddr_t, char*, kl_modinfo_t*);
int  kl_complete_modinfo(kl_modinfo_t*);
int  kl_load_ksyms(int);
int  kl_load_ksyms_2_6(int);
int  kl_unload_ksyms(void);
int  kl_load_module_sym(char*, char*, char*);
int  kl_unload_module_sym(char*);
int  kl_autoload_module_info(char*);
kl_modinfo_t * kl_lkup_modinfo(char*);

#endif /* __KL_MODULE_H */
