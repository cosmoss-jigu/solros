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
 * $Id: kl_htnode.h 1122 2004-12-21 23:26:23Z tjm $
 *
 * This file is part of libutil.
 * A library which provides auxiliary functions.
 * libutil is part of lkcdutils -- utilities for Linux kernel crash dumps.
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

#ifndef __KL_HTNODE_H
#define __KL_HTNODE_H

/* Node structure for use in hierarchical trees (htrees).
 */
typedef struct htnode_s {
	struct htnode_s	*next;
	struct htnode_s	*prev;
	struct htnode_s	*parent;
	struct htnode_s	*children;
	int				 seq;
	int				 level;
	int				 key;
} htnode_t;

/* Flag values
 */
#define HT_BEFORE	0x1
#define HT_AFTER	0x2
#define HT_CHILD	0x4
#define HT_PEER		0x8

/* Function prototypes
 */
htnode_t *kl_next_htnode(
	htnode_t *		/* htnode pointer */);

htnode_t *kl_prev_htnode(
	htnode_t *		/* htnode pointer */);

void ht_insert_peer(
	htnode_t *		/* htnode pointer */,
	htnode_t *		/* new htnode pointer*/,
	int 			/* flags */);

void ht_insert_child(
	htnode_t *		/* htnode pointer */,
	htnode_t *		/* new htnode pointer*/,
	int 			/* flags */);

int ht_insert(
	htnode_t *		/* htnode pointer */,
	htnode_t *		/* new htnode pointer*/,
	int 			/* flags */);

void ht_insert_next_htnode(
	htnode_t *      /* htnode pointer */,
	htnode_t *      /* new htnode pointer*/);

#endif /* __KL_HTNODE_H */
