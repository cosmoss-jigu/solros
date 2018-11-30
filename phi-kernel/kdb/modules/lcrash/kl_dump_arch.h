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
 * $Id: kl_dump_arch.h 1122 2004-12-21 23:26:23Z tjm $
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

#ifndef __KL_DUMP_ARCH_H
#define __KL_DUMP_ARCH_H

/* check for valid configuration
 */
#if !(defined(HOST_ARCH_ALPHA) || defined(HOST_ARCH_I386) || \
      defined(HOST_ARCH_IA64)  || defined(HOST_ARCH_S390) || \
      defined(HOST_ARCH_S390X) || defined(HOST_ARCH_ARM)  || \
      defined(HOST_ARCH_PPC64) || defined(HOST_ARCH_X86_64))
# error "No valid host architecture defined."
#endif
#if ((defined(HOST_ARCH_ALPHA) && \
      (defined(HOST_ARCH_I386) || defined(HOST_ARCH_IA64) || \
       defined(HOST_ARCH_S390) || defined(HOST_ARCH_S390X) || \
       defined(HOST_ARCH_ARM) || defined(HOST_ARCH_PPC64) || \
       defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_I386) && \
      (defined(HOST_ARCH_IA64) || defined(HOST_ARCH_S390) || \
       defined(HOST_ARCH_S390X)|| defined(HOST_ARCH_ARM) || \
       defined(HOST_ARCH_PPC64)|| defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_IA64) && \
      (defined(HOST_ARCH_S390)|| defined(HOST_ARCH_S390X) || \
       defined(HOST_ARCH_ARM) || defined(HOST_ARCH_PPC64) || \
       defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_S390) && \
      (defined(HOST_ARCH_S390X) || defined(HOST_ARCH_ARM) || \
       defined(HOST_ARCH_PPC64) || defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_S390X) && \
       (defined(HOST_ARCH_ARM) || defined(HOST_ARCH_PPC64) || \
       defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_ARM) && \
      (defined(HOST_ARCH_PPC64) || defined(HOST_ARCH_X86_64))) || \
     (defined(HOST_ARCH_PPC64) && defined(HOST_ARCH_X86_64)))
# error "More than one valid host architectures defined."
#endif
#if !(defined(DUMP_ARCH_ALPHA) || defined(DUMP_ARCH_I386) || \
      defined(DUMP_ARCH_IA64)  || defined(DUMP_ARCH_S390) || \
      defined(DUMP_ARCH_S390X) || defined(DUMP_ARCH_ARM) || \
      defined(DUMP_ARCH_PPC64) || defined(DUMP_ARCH_X86_64))
# error "No valid dump architecture defined."
#endif

/* optional: check that host arch equals one supported dump arch
 */
#ifdef SUPPORT_HOST_ARCH
# if (defined(HOST_ARCH_ALPHA) && !defined(DUMP_ARCH_ALPHA)) || \
     (defined(HOST_ARCH_I386) && !defined(DUMP_ARCH_I386)) || \
     (defined(HOST_ARCH_IA64) && !defined(DUMP_ARCH_IA64)) || \
     (defined(HOST_ARCH_S390) && !defined(DUMP_ARCH_S390)) || \
     (defined(HOST_ARCH_S390X) && !defined(DUMP_ARCH_S390X)) || \
     (defined(HOST_ARCH_ARM) && !defined(DUMP_ARCH_ARM)) || \
     (defined(HOST_ARCH_PPC64) && !defined(DUMP_ARCH_PPC64)) || \
     (defined(HOST_ARCH_X86_64) && !defined(DUMP_ARCH_X86_64))
#  error "Host architecture not supported as dump architecture."
# endif
#endif

/* include dump architecture specific stuff
 */
#ifdef DUMP_ARCH_ALPHA
# include <kl_mem_alpha.h>
# include <kl_dump_alpha.h>
#endif
/* cpw: use the " " form: */
#ifdef DUMP_ARCH_IA64
# include "kl_mem_ia64.h"
# include "kl_dump_ia64.h"
#endif
#ifdef DUMP_ARCH_I386
# include <kl_mem_i386.h>
# include <kl_dump_i386.h>
#endif
#ifdef DUMP_ARCH_S390
# include <kl_mem_s390.h>
# include <kl_dump_s390.h>
#endif
#ifdef DUMP_ARCH_S390X
# include <kl_mem_s390x.h>
# include <kl_dump_s390.h>
#endif
#ifdef DUMP_ARCH_ARM
# include <kl_mem_arm.h>
# include <kl_dump_arm.h>
#endif
#ifdef DUMP_ARCH_PPC64
#include <kl_mem_ppc64.h>
#include <kl_dump_ppc64.h>
#endif
#ifdef DUMP_ARCH_X86_64
#include <kl_mem_x86_64.h>
#include <kl_dump_x86_64.h>
#endif

/** Function prototypes
 **/
int kl_init_kern_info(void);

int kl_get_struct(
	kaddr_t 	/* address */,
	int 		/* size of struct */,
	void *		/* ptr to buffer */,
	char *		/* name of struct */);

#endif /* __KL_DUMP_ARCH_H */
