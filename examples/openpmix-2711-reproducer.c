#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pmix_server.h>
#include <pmix.h>


/** Generic type for callback data */
typedef struct {
    pmix_status_t status;
    volatile bool filled;
    pmix_info_t *info;
    size_t ninfo;
} mycbdata_t;

/** Generic type for callback function */
typedef struct {
    pmix_op_cbfunc_t cbfunc;
    void *cbdata;
} mycbfunc_t;

/** Setting up data for callback routines */
#define INIT_CBDATA(d, n) do {     \
    memset(&(d), 0, sizeof(d));    \
    (d).ninfo = n;                 \
    if (n) PMIX_INFO_CREATE((d).info, n); \
} while(0)

/** Waiting for data to be filled by callback function */
#define WAIT_FOR_CBDATA(d) while(!(d).filled) usleep(10)

/** Set data to be available (used in callback function) */
#define SET_CBDATA_AVAIL(d) (d)->filled = true

/** Setting up data for callback routines */
#define DESTROY_CBDATA(d) if ((d).ninfo) PMIX_INFO_FREE((d).info, (d).ninfo)


/* struct holding the server callback functions */
static pmix_server_module_t module = {
    /* v1x interfaces */
    .client_connected = NULL, /* deprecated */
    .client_finalized = NULL,
    .abort = NULL,
    .fence_nb = NULL,
    .direct_modex = NULL,
    .publish = NULL,
    .lookup = NULL,
    .unpublish = NULL,
    .spawn = NULL,
    .connect = NULL,
    .disconnect = NULL,
    .register_events = NULL,
    .deregister_events = NULL,
    .listener = NULL,
    /* v2x interfaces */
    .notify_event = NULL,
    .query = NULL,
    .tool_connected = NULL,
    .log = NULL,
    .allocate = NULL,
    .job_control = NULL,
    .monitor = NULL,
    /* v3x interfaces */
    .get_credential = NULL,
    .validate_credential = NULL,
    .iof_pull = NULL,
    .push_stdin = NULL,
    /* v4x interfaces */
    .group = NULL,
    .fabric = NULL,
    .client_connected2 = NULL,
};

static void errhandler(size_t evhdlr_registration_id, pmix_status_t status,
		       const pmix_proc_t *source,
		       pmix_info_t info[], size_t ninfo,
		       pmix_info_t results[], size_t nresults,
		       pmix_event_notification_cbfunc_fn_t cbfunc, void *cbdata)
{
    printf("%s(status %d proc %s:%u ninfo %lu nresults %lu\n",
	 __func__, status, source->nspace, source->rank, ninfo, nresults);
}

/**
 * To be called by PMIx_server_register_resources() to provide status
 */
static void registerResources_cb(pmix_status_t status, void *cbdata)
{
    printf("%s()\n", __func__);

    mycbdata_t *data = cbdata;

    data->status = status;

    SET_CBDATA_AVAIL(data);
}

/**
 * To be called by error handler registration function to provide success state
 */
static void registerErrorHandler_cb (pmix_status_t status,
				     size_t errhandler_ref, void *cbdata)
{
    printf("%s()\n", __func__);

    mycbdata_t *data = cbdata;

    data->status = status;

    SET_CBDATA_AVAIL(data);
}

#define INFO_LIST_ADD(i, key, val, t) \
    do { \
	pmix_status_t status = PMIx_Info_list_add(i, key, val, t); \
	if (status != PMIX_SUCCESS) printf("%s: Failed to add : %s\n", __func__, PMIx_Error_string(status)); \
    } while(0)

bool pspmix_server_init(char *nspace, pmix_rank_t rank, const char *clusterid,
			const char *srvtmpdir, const char *systmpdir)
{
    void *list = PMIx_Info_list_start();

    /* Name of the namespace to use for this PMIx server */
    INFO_LIST_ADD(list, PMIX_SERVER_NSPACE, nspace, PMIX_STRING);

    /* Rank of this PMIx server */
    INFO_LIST_ADD(list, PMIX_SERVER_RANK, &rank, PMIX_PROC_RANK);

    /* The host RM wants to declare itself as willing to accept tool connection
     * requests. */
    bool tmpbool = false;
    INFO_LIST_ADD(list, PMIX_SERVER_TOOL_SUPPORT, &tmpbool, PMIX_BOOL);

    /* The host RM wants to declare itself as being the local system server for
     * PMIx connection 
     *
     * PMIx servers that are designated as system servers by including the
     * PMIX_SERVER_SYSTEM_SUPPORT attribute when calling PMIx_server_init will
     * create a rendezvous file in PMIX_SYSTEM_TMPDIR top-level directory.
     * The filename will be of the form pmix.sys.hostname, where hostname is the
     * string returned by the gethostname system call. Note that only one PMIx
     * server on a node can be designated as the system server.
     *
     * Non-system PMIx servers will create a set of three rendezvous files in
     * the directory defined by either the PMIX_SERVER_TMPDIR attribute or the
     * TMPDIR environmental variable:
     * • pmix.host.tool.nspace where host is the string returned by the
     *			       gethostname system call and nspace is the
     *			       namespace of the server.
     * • pmix.host.tool.pid    where host is the string returned by the
     *			       gethostname system call and pid is the PID of the
     *			       server.
     * • pmix.host.tool        where host is the string returned by the
     *			       gethostname system call. Note that servers which
     *			       are not given a namespace-specific
     *			       PMIX_SERVER_TMPDIR attribute may not
     *			       generate this file due to conflicts should
     *			       multiple servers be present on the node.
     */
    tmpbool = false;
    INFO_LIST_ADD(list, PMIX_SERVER_SYSTEM_SUPPORT, &tmpbool, PMIX_BOOL);

    /* The host RM wants to declare itself as being the local session server for
     * PMIx connection requests. */
    tmpbool = false;
    INFO_LIST_ADD(list, PMIX_SERVER_SESSION_SUPPORT, &tmpbool, PMIX_BOOL);

    /* Server is acting as a gateway for PMIx requests that cannot be serviced
     * on backend nodes (e.g., logging to email). */
    tmpbool = false;
    INFO_LIST_ADD(list, PMIX_SERVER_GATEWAY, &tmpbool, PMIX_BOOL);

    /* Server is supporting system scheduler and desires access to appropriate
     * WLM-supporting features. Indicates that the library is to be initialized
     * for scheduler support. */
    tmpbool = false;
    INFO_LIST_ADD(list, PMIX_SERVER_SCHEDULER, &tmpbool, PMIX_BOOL);

    pmix_status_t status;

    pmix_data_array_t array;
    status = PMIx_Info_list_convert(list, &array);
    if (status != PMIX_SUCCESS) {
	printf("%s: Converting info list to array failed: %s\n", __func__,
	     PMIx_Error_string(status));
	return false;
    }

    pmix_info_t *info = array.array;
    size_t ninfo = array.size;
    PMIx_Info_list_release(list);

    /* initialize server library */
    status = PMIx_server_init(&module, info, ninfo);
    if (status != PMIX_SUCCESS) {
	printf("%s: PMIx_server_init() failed: %s\n", __func__,
	     PMIx_Error_string(status));
	return false;
    }
    printf("%s: PMIx_server_init() successful\n", __func__);

    printf("%s: PMIx_server_init info:\n", __func__);
    for (int i = 0; i < ninfo; i++) {
	char * istr = PMIx_Info_string(&info[i]);
	printf("%s\n", istr);
	free(istr);
    }
    PMIX_DATA_ARRAY_DESTRUCT(&array);

    mycbdata_t cbdata;

    /* tell the server common information */
    INIT_CBDATA(cbdata, 4);

    size_t i = 0;
    /* A string name for the cluster this allocation is on */
    PMIX_INFO_LOAD(&cbdata.info[i], PMIX_CLUSTER_ID, clusterid, PMIX_STRING);
    i++;

    /* String name of the RM */
    char *rmname = "ParaStation";
    PMIX_INFO_LOAD(&cbdata.info[i], PMIX_RM_NAME, rmname, PMIX_STRING);
    i++;

    /* RM version string */
    const char *rmversion = "5.1";
    PMIX_INFO_LOAD(&cbdata.info[i], PMIX_RM_VERSION, rmversion, PMIX_STRING);
    i++;

    /* Host where target PMIx server is located */
    const char *hostname = "node01";
    PMIX_INFO_LOAD(&cbdata.info[i], PMIX_SERVER_HOSTNAME, hostname,
		   PMIX_STRING);
    i++;

    printf("%s: PMIx_server_register_resources info:\n", __func__);
    for (i = 0; i < cbdata.ninfo; i++) {
	char * istr = PMIx_Info_string(&cbdata.info[i]);
	printf("%s\n", istr);
	free(istr);
    }

    status = PMIx_server_register_resources(cbdata.info, cbdata.ninfo,
					    registerResources_cb, &cbdata);
    if (status != PMIX_SUCCESS) {
	printf("%s: PMIx_server_register_resources() failed: %s\n", __func__,
	     PMIx_Error_string(status));
	DESTROY_CBDATA(cbdata);
	return false;
    }
    WAIT_FOR_CBDATA(cbdata);

    if (cbdata.status != PMIX_SUCCESS) {
	printf("%s: Callback from register resources failed: %s\n", __func__,
	     PMIx_Error_string(cbdata.status));
	DESTROY_CBDATA(cbdata);
	return false;
    }
    DESTROY_CBDATA(cbdata);

    printf("%s: PMIx_server_register_resources()"
	 " successful\n", __func__);

    /* register the error handler */
    INIT_CBDATA(cbdata, 0);
    PMIx_Register_event_handler(NULL, 0, NULL, 0,
	    errhandler, registerErrorHandler_cb, &cbdata);
    WAIT_FOR_CBDATA(cbdata);

    if (cbdata.status != PMIX_SUCCESS) {
	printf("%s: Callback from register error handler failed: %s\n", __func__,
	     PMIx_Error_string(cbdata.status));
	return false;
    }
    DESTROY_CBDATA(cbdata);
    return true;
}

static void fillSessionInfoArray(pmix_data_array_t *sessionInfo,
				 uint32_t session_id, uint32_t universe_size)
{
#define SESSION_INFO_ARRAY_LEN 2
    pmix_info_t *infos;
    PMIX_INFO_CREATE(infos, SESSION_INFO_ARRAY_LEN);

    size_t i = 0;
    /* first entry needs to be session id */
    PMIX_INFO_LOAD(&infos[i], PMIX_SESSION_ID, &session_id, PMIX_UINT32);
    i++;

    /* number of slots in this session */
    PMIX_INFO_LOAD(&infos[i], PMIX_MAX_PROCS, &universe_size, PMIX_UINT32);
    i++;

    printf("%s:\n", __func__);
    for (i = 0; i < SESSION_INFO_ARRAY_LEN; i++) {
	char * istr = PMIx_Info_string(&infos[i]);
	printf("%s\n", istr);
	free(istr);
    }

    sessionInfo->type = PMIX_INFO;
    sessionInfo->size = SESSION_INFO_ARRAY_LEN;
    sessionInfo->array = infos;
}

static void fillJobInfoArray(pmix_data_array_t *jobInfo)
{
#define JOB_INFO_ARRAY_LEN 6
    pmix_info_t *infos;
    PMIX_INFO_CREATE(infos, JOB_INFO_ARRAY_LEN);

    size_t i = 0;
    /* job identifier (this is the name of the namespace */
    PMIX_INFO_LOAD(&infos[i], PMIX_JOBID, "MyJobId", PMIX_STRING);
    i++;

    /* total num of processes in this job across all contained applications */
    uint32_t jobSize = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_JOB_SIZE, &jobSize, PMIX_UINT32);
    i++;

    /* Maximum number of processes in this job */
    uint32_t maxProcs = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_MAX_PROCS, &maxProcs, PMIX_UINT32);
    i++;

    /* regex of nodes containing procs for this job */
    char *nodelist_s = "node01";
    char *nodelist_r;
    PMIx_generate_regex(nodelist_s, &nodelist_r);
    PMIX_INFO_LOAD(&infos[i], PMIX_NODE_MAP, nodelist_r, PMIX_STRING);
    i++;

    /* regex describing procs on each node within this job */
    char *pmap_s = "0";
    char *pmap_r;
    PMIx_generate_ppn(pmap_s, &pmap_r);
    PMIX_INFO_LOAD(&infos[i], PMIX_PROC_MAP, pmap_r, PMIX_STRING);
    i++;

    /* number of applications in this job (required if > 1) */
    uint32_t numApps = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_JOB_NUM_APPS, &numApps, PMIX_UINT32);
    i++;

    printf("%s:\n", __func__);
    for (i = 0; i < JOB_INFO_ARRAY_LEN; i++) {
	char * istr = PMIx_Info_string(&infos[i]);
	printf("%s\n", istr);
	free(istr);
    }

    jobInfo->type = PMIX_INFO;
    jobInfo->size = JOB_INFO_ARRAY_LEN;
    jobInfo->array = infos;
}

static void fillAppInfoArray(pmix_data_array_t *appInfo)
{
#define APP_INFO_ARRAY_LEN 5
    pmix_info_t *infos;
    PMIX_INFO_CREATE(infos, APP_INFO_ARRAY_LEN);

    size_t i = 0;
    /* application number */
    uint32_t appNum = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_APPNUM, &appNum, PMIX_UINT32);
    i++;

    /* number of processes in this application */
    uint32_t appSize = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_APP_SIZE, &appSize, PMIX_UINT32);
    i++;

    /* lowest rank in this application within the job */
    PMIX_INFO_LOAD(&infos[i], PMIX_APPLDR, 0, PMIX_PROC_RANK);
    i++;

    /* working directory for spawned processes */
    PMIX_INFO_LOAD(&infos[i], PMIX_WDIR, get_current_dir_name(), PMIX_STRING);
    i++;

    /* concatenated argv for spawned processes */
    PMIX_INFO_LOAD(&infos[i], PMIX_APP_ARGV, "myapp", PMIX_STRING);
    i++;

    printf("%s:\n", __func__);
    for (i = 0; i < APP_INFO_ARRAY_LEN; i++) {
	char * istr = PMIx_Info_string(&infos[i]);
	printf("%s\n", istr);
	free(istr);
    }

    appInfo->type = PMIX_INFO;
    appInfo->size = APP_INFO_ARRAY_LEN;
    appInfo->array = infos;
}

static void fillNodeInfoArray(pmix_data_array_t *nodeInfo, const char *tmpdir,
			      const char *nsdir)
{
#define NODE_INFO_ARRAY_LEN 7
    pmix_info_t *infos;
    PMIX_INFO_CREATE(infos, NODE_INFO_ARRAY_LEN);

    size_t i = 0;
    /* node id (in the session) */
    uint32_t id = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_NODEID, &id, PMIX_UINT32);
    i++;

    /* hostname */
    PMIX_INFO_LOAD(&infos[i], PMIX_HOSTNAME, "node01", PMIX_STRING);
    i++;

    /* number of processes on the node (in this namespace) */
    uint32_t numProcs = 1;
    PMIX_INFO_LOAD(&infos[i], PMIX_LOCAL_SIZE, &numProcs, PMIX_UINT32);
    i++;

    /* lowest rank on this node within this job/namespace */
    pmix_rank_t rank = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_LOCALLDR, &rank, PMIX_PROC_RANK);
    i++;

    /* Comma-delimited list of ranks that are executing on the node
     * within this namespace */
    PMIX_INFO_LOAD(&infos[i], PMIX_LOCAL_PEERS, "0", PMIX_STRING);
    i++;

    /* Full path to the top-level temporary directory assigned to the
     * session */
    PMIX_INFO_LOAD(&infos[i], PMIX_TMPDIR, tmpdir, PMIX_STRING);
    i++;

    /* Full path to the temporary directory assigned to the specified job,
     * under PMIX_TMPDIR. */
    PMIX_INFO_LOAD(&infos[i], PMIX_NSDIR, nsdir, PMIX_STRING);
    i++;

    printf("%s:\n", __func__);
    for (i = 0; i < NODE_INFO_ARRAY_LEN; i++) {
	char * istr = PMIx_Info_string(&infos[i]);
	printf("%s\n", istr);
	free(istr);
    }

    nodeInfo->type = PMIX_INFO;
    nodeInfo->size = NODE_INFO_ARRAY_LEN;
    nodeInfo->array = infos;
}

static void fillProcDataArray(pmix_data_array_t *procData, const char *nsdir)
{
#define PROC_INFO_ARRAY_LEN 11
    pmix_info_t *infos;
    PMIX_INFO_CREATE(infos, PROC_INFO_ARRAY_LEN);

    size_t i = 0;
    /* process rank within the job, starting from zero */
    pmix_rank_t rank = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_RANK, &rank, PMIX_PROC_RANK);
    i++;

    /* application number within the job in which the process is a member. */
    uint32_t appNum = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_APPNUM, &appNum, PMIX_UINT32);
    i++;

    /* rank within the process' application */
    PMIX_INFO_LOAD(&infos[i], PMIX_APP_RANK, &rank, PMIX_PROC_RANK);
    i++;

    /* rank of the process spanning across all jobs in this session
     * starting with zero.
     * Note that no ordering of the jobs is implied when computing this value.
     * As jobs can start and end at random times, this is defined as a
     * continually growing number - i.e., it is not dynamically adjusted as
     * individual jobs and processes are started or terminated. */
    PMIX_INFO_LOAD(&infos[i], PMIX_GLOBAL_RANK, &rank, PMIX_PROC_RANK);
    i++;

    /* rank of the process on its node in its job
     * refers to the numerical location (starting from zero) of the process on
     * its node when idxing only those processes from the same job that share
     * the node, ordered by their overall rank within that job. */
    PMIX_INFO_LOAD(&infos[i], PMIX_LOCAL_RANK, &rank, PMIX_UINT16);
    i++;

    /* rank of the process on its node spanning all jobs
     * refers to the numerical location (starting from zero) of the process on
     * its node when idxing all processes (regardless of job) that share the
     * node, ordered by their overall rank within the job. The value represents
     * a snapshot in time when the specified process was started on its node and
     * is not dynamically adjusted as processes from other jobs are started or
     * terminated on the node. */
    PMIX_INFO_LOAD(&infos[i], PMIX_NODE_RANK, &rank, PMIX_UINT16);
    i++;

    /* node identifier where the process is located */
    uint32_t val_u32 = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_NODEID, &val_u32, PMIX_UINT32);
    i++;

    /* true if this proc resulted from a call to PMIx_Spawn */
    bool spawned = false;
    PMIX_INFO_LOAD(&infos[i], PMIX_SPAWNED, &spawned, PMIX_BOOL);
    i++;

    /* number of times this process has been re-instantiated
     * i.e, a value of zero indicates that the process has never been restarted.
     */
    uint32_t reinc = 0;
    PMIX_INFO_LOAD(&infos[i], PMIX_REINCARNATION, &reinc, PMIX_UINT32);
    i++;

    /* Full path to the subdirectory under PMIX_NSDIR assigned to the
     * specified process. */
    int pdsize = strlen(nsdir)+10;
    char *procdir = malloc(pdsize);
    if (snprintf(procdir, pdsize, "%s/%u", nsdir, 0) >= pdsize) {
	printf("%s: Warning, procdir truncated", __func__);
    }
    PMIX_INFO_LOAD(&infos[i], PMIX_PROCDIR, procdir, PMIX_STRING);
    i++;
    free(procdir);

    /* rank of the process on the package (socket) where this process
     * resides refers to the numerical location (starting from zero) of the
     * process on its package when counting only those processes from the
     * same job that share the package, ordered by their overall rank within
     * that job. Note that processes that are not bound to PUs within a
     * single specific package cannot have a package rank. */
    uint16_t pkgrank = 0; 
    PMIX_INFO_LOAD(&infos[i], PMIX_PACKAGE_RANK, &pkgrank, PMIX_UINT16);
    i++;

    printf("%s:\n", __func__);
    for (i = 0; i < PROC_INFO_ARRAY_LEN; i++) {
	char * istr = PMIx_Info_string(&infos[i]);
	printf("%s\n", istr);
	free(istr);
    }

    procData->type = PMIX_INFO;
    procData->size = PROC_INFO_ARRAY_LEN;
    procData->array = infos;
}

/**
 * To be called by PMIx_register_namespace() to provide status
 */
static void registerNamespace_cb(pmix_status_t status, void *cbdata)
{
    mycbdata_t *data = cbdata;

    printf("%s()\n", __func__);

    data->status = status;

    SET_CBDATA_AVAIL(data);
}

static bool pspmix_server_registerNamespace(const char *nspace, uint32_t jobSize,
				     const char *tmpdir, const char *nsdir)
{
    pmix_status_t status;

    /* fill infos */
    mycbdata_t data;
    INIT_CBDATA(data, 4 + 1 + 1 + jobSize);

    /* ===== session info ===== */

    size_t i = 0;
    /* number of allocated slots in a session (here for historical reasons) */
    uint32_t univSize = 1;
    PMIX_INFO_LOAD(&data.info[i], PMIX_UNIV_SIZE, &univSize, PMIX_UINT32);
    i++;

    /* session info array */
    pmix_data_array_t sessionInfo;
    fillSessionInfoArray(&sessionInfo, 12345, 1);
    PMIX_INFO_LOAD(&data.info[i], PMIX_SESSION_INFO_ARRAY, &sessionInfo,
		   PMIX_DATA_ARRAY);
    i++;

    /* ===== job info array ===== */
    /* total num of processes in this job (here for historical reasons) */
    PMIX_INFO_LOAD(&data.info[i], PMIX_JOB_SIZE, &jobSize, PMIX_UINT32);
    i++;

    pmix_data_array_t jobInfo;
    fillJobInfoArray(&jobInfo);
    PMIX_INFO_LOAD(&data.info[i], PMIX_JOB_INFO_ARRAY, &jobInfo,
		   PMIX_DATA_ARRAY);
    i++;

    /* ===== application info arrays ===== */
    pmix_data_array_t appInfo;
    fillAppInfoArray(&appInfo);

    PMIX_INFO_LOAD(&data.info[i], PMIX_APP_INFO_ARRAY, &appInfo,
		   PMIX_DATA_ARRAY);
    i++;

    /* ===== node info arrays ===== */
    pmix_data_array_t nodeInfo;
    fillNodeInfoArray(&nodeInfo, tmpdir, nsdir);

    PMIX_INFO_LOAD(&data.info[i], PMIX_NODE_INFO_ARRAY, &nodeInfo,
		       PMIX_DATA_ARRAY);
    i++;

    /* ===== process data ===== */

    /* information about all global ranks */
    pmix_data_array_t procData;
    fillProcDataArray(&procData, nsdir);

    PMIX_INFO_LOAD(&data.info[i], PMIX_PROC_DATA, &procData,
		   PMIX_DATA_ARRAY);
    i++;

    if (i != data.ninfo) {
	printf("%s: WARNING: Number of info fields does not match (%lu != %lu)\n",
	     __func__, i, data.ninfo);
    }

    /* register namespace */
    status = PMIx_server_register_nspace(nspace, 1, data.info,
	    data.ninfo, registerNamespace_cb, &data);
    if (status == PMIX_OPERATION_SUCCEEDED) {
	goto reg_nspace_success;
    }

    if (status != PMIX_SUCCESS) {
	printf("%s: PMIx_server_register_nspace() failed.\n", __func__);
	goto reg_nspace_error;
    }
    WAIT_FOR_CBDATA(data);

    if (data.status != PMIX_SUCCESS) {
	printf("%s: Callback from register namespace failed: %s\n", __func__,
		PMIx_Error_string(data.status));
	goto reg_nspace_error;
    }

reg_nspace_success:
    DESTROY_CBDATA(data);
    return true;

reg_nspace_error:
    DESTROY_CBDATA(data);
    return false;
}

int main(void) {

    char *clusterid = "ParaStation Cluster";

    /* generate server namespace name */
    static char nspace[PMIX_MAX_NSLEN];
    snprintf(nspace, PMIX_MAX_NSLEN, "pspmix");

    /* initialize the pmix server */
    if (!pspmix_server_init(nspace, 0, clusterid, NULL, NULL)) {
	printf("failed to initialize pspmix server\n");
	return false;
    }
    printf("pspmix server initialized\n");

    /* register namespace */
    if (!pspmix_server_registerNamespace("MyNamespace", 1, "/tmp/pmix-session-12345",
					 "/tmp/pmix-session-12345/NyNamespace")) {
	printf("failed to register namespace at the pspmix server\n");
	return false;
    }

    printf("namespace registered\n");


    return true;
}

/* vim: set ts=8 sw=4 tw=0 sts=4 noet :*/
