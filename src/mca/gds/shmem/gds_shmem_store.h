/*
 * Copyright (c) 2015-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2016-2018 IBM Corporation.  All rights reserved.
 * Copyright (c) 2018      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2018-2020 Mellanox Technologies, Inc.
 *                         All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting.  All rights reserved.
 * Copyright (c) 2022      Triad National Security, LLC. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PMIX_GDS_SHMEM_STORE_H
#define PMIX_GDS_SHMEM_STORE_H

#include "gds_shmem.h"

BEGIN_C_DECLS

PMIX_EXPORT pmix_status_t
pmix_gds_shmem_store_qualified(
    pmix_hash_table_t *ht,
    pmix_rank_t rank,
    pmix_value_t *value
);

PMIX_EXPORT pmix_status_t
pmix_gds_shmem_store_local_job_data_in_shmem(
    pmix_gds_shmem_job_t *job,
    pmix_list_t *job_data
);

PMIX_EXPORT pmix_status_t
pmix_gds_shmem_store_modex_in_shmem(
    pmix_gds_base_ctx_t ctx,
    pmix_proc_t *proc,
    pmix_gds_modex_key_fmt_t key_fmt,
    char **kmap,
    pmix_buffer_t *pbkt
);

END_C_DECLS

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
