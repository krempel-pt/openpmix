/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2009 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2014-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2015-2020 Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 */
#include "src/include/pmix_config.h"

#include "pmix_common.h"

#ifdef HAVE_STRING_H
#    include <string.h>
#endif

#include "src/class/pmix_list.h"
#include "src/mca/base/base.h"
#include "src/mca/prm/base/base.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "src/mca/prm/base/static-components.h"

/* Instantiate the global vars */
pmix_prm_globals_t pmix_prm_globals = {
    .lock = PMIX_LOCK_STATIC_INIT,
    .actives = PMIX_LIST_STATIC_INIT,
    .initialized = false,
    .selected = false
};

pmix_prm_API_module_t pmix_prm = {
    .notify = pmix_prm_base_notify
};

static pmix_status_t pmix_prm_close(void)
{
    pmix_prm_base_active_module_t *active, *prev;

    if (!pmix_prm_globals.initialized) {
        return PMIX_SUCCESS;
    }
    pmix_prm_globals.initialized = false;
    pmix_prm_globals.selected = false;

    PMIX_LIST_FOREACH_SAFE (active, prev, &pmix_prm_globals.actives,
                            pmix_prm_base_active_module_t) {
        pmix_list_remove_item(&pmix_prm_globals.actives, &active->super);
        if (NULL != active->module->finalize) {
            active->module->finalize();
        }
        PMIX_RELEASE(active);
    }
    PMIX_DESTRUCT(&pmix_prm_globals.actives);

    PMIX_DESTRUCT_LOCK(&pmix_prm_globals.lock);
    return pmix_mca_base_framework_components_close(&pmix_prm_base_framework, NULL);
}

static pmix_status_t pmix_prm_open(pmix_mca_base_open_flag_t flags)
{
    /* initialize globals */
    pmix_prm_globals.initialized = true;
    PMIX_CONSTRUCT_LOCK(&pmix_prm_globals.lock);
    pmix_prm_globals.lock.active = false;
    PMIX_CONSTRUCT(&pmix_prm_globals.actives, pmix_list_t);

    /* Open up all available components */
    return pmix_mca_base_framework_components_open(&pmix_prm_base_framework, flags);
}

PMIX_MCA_BASE_FRAMEWORK_DECLARE(pmix, prm, "PMIx Network Operations", NULL, pmix_prm_open,
                                pmix_prm_close, mca_prm_base_static_components,
                                PMIX_MCA_BASE_FRAMEWORK_FLAG_DEFAULT);

PMIX_CLASS_INSTANCE(pmix_prm_base_active_module_t, pmix_list_item_t, NULL, NULL);

static void rlcon(pmix_prm_rollup_t *p)
{
    PMIX_CONSTRUCT_LOCK(&p->lock);
    p->lock.active = false;
    p->status = PMIX_SUCCESS;
    p->requests = 0;
    p->replies = 0;
    p->cbfunc = NULL;
    p->cbdata = NULL;
}
static void rldes(pmix_prm_rollup_t *p)
{
    PMIX_DESTRUCT_LOCK(&p->lock);
}
PMIX_CLASS_INSTANCE(pmix_prm_rollup_t, pmix_object_t, rlcon, rldes);
