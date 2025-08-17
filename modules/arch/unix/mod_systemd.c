/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

#include <stdint.h>
#include <ap_config.h>
#include "ap_mpm.h"
#include "ap_listen.h"
#include <http_core.h>
#include <httpd.h>
#include <http_log.h>
#include <apr_version.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "unixd.h"
#include "scoreboard.h"
#include "mpm_common.h"

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#include "systemd/sd-daemon.h"

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

static int systemd_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
                              apr_pool_t *ptemp)
{
    sd_notify(0,
              "RELOADING=1\n"
              "STATUS=Reading configuration...\n");
    ap_extended_status = 1;
    return OK;
}

#ifdef HAVE_SELINUX
static void log_selinux_context(void)
{
    char *con;

    if (is_selinux_enabled() && getcon(&con) == 0) {
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, NULL,
                     APLOGNO(10497) "SELinux is enabled; "
                     "httpd running as context %s", con);
        freecon(con);
    }
}
#endif

/* Report the service is ready in post_config, which could be during
 * startup or after a reload.  The server could still hit a fatal
 * startup error after this point during ap_run_mpm(), so this is
 * perhaps too early, but by post_config listen() has been called on
 * the TCP ports so new connections will not be rejected.  There will
 * always be a possible async failure event simultaneous to the
 * service reporting "ready", so this should be good enough. */
static int systemd_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                               apr_pool_t *ptemp, server_rec *main_server)
{
    if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG)
        return OK;

#ifdef HAVE_SELINUX
    log_selinux_context();
#endif

    sd_notify(0, "READY=1\n"
              "STATUS=Configuration loaded.\n");
    return OK;
}

static int systemd_pre_mpm(apr_pool_t *p, ap_scoreboard_e sb_type)
{
    sd_notifyf(0, "READY=1\n"
               "STATUS=Processing requests...\n"
               "MAINPID=%" APR_PID_T_FMT, getpid());

    return OK;
}

static int systemd_monitor(apr_pool_t *p, server_rec *s)
{
    ap_sload_t sload;
    apr_interval_time_t up_time;
    char bps[5];

    if (!ap_extended_status) {
        /* Nothing useful to report with ExtendedStatus disabled. */
        return DECLINED;
    }
    
    ap_get_sload(&sload);
    /* up_time in seconds */
    up_time = (apr_uint32_t) apr_time_sec(apr_time_now() -
                               ap_scoreboard_image->global->restart_time);

    apr_strfsize((unsigned long)((float) (sload.bytes_served)
                                 / (float) up_time), bps);

    sd_notifyf(0, "READY=1\n"
               "STATUS=Total requests: %lu; Idle/Busy workers %d/%d;"
               "Requests/sec: %.3g; Bytes served/sec: %sB/sec\n",
               sload.access_count, sload.idle, sload.busy,
               ((float) sload.access_count) / (float) up_time, bps);

    return DECLINED;
}

static int ap_find_systemd_socket(process_rec * process, apr_port_t port) {
    int fdcount, fd;
    int sdc = sd_listen_fds(0);

    if (sdc < 0) {
        ap_log_perror(APLOG_MARK, APLOG_CRIT, sdc, process->pool, APLOGNO(02486)
                      "find_systemd_socket: Error parsing enviroment, sd_listen_fds returned %d",
                      sdc);
        return -1;
    }

    if (sdc == 0) {
        ap_log_perror(APLOG_MARK, APLOG_CRIT, sdc, process->pool, APLOGNO(02487)
                      "find_systemd_socket: At least one socket must be set.");
        return -1;
    }

    fdcount = atoi(getenv("LISTEN_FDS"));
    for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + fdcount; fd++) {
        if (sd_is_socket_inet(fd, 0, 0, -1, port) > 0) {
            return fd;
        }
    }

    return -1;
}

static int ap_systemd_listen_fds(int unset_environment){
    return sd_listen_fds(unset_environment);
}

static void systemd_register_hooks(apr_pool_t *p)
{
    APR_REGISTER_OPTIONAL_FN(ap_systemd_listen_fds);
    APR_REGISTER_OPTIONAL_FN(ap_find_systemd_socket);

    /* Enable ap_extended_status. */
    ap_hook_pre_config(systemd_pre_config, NULL, NULL, APR_HOOK_LAST);
    /* Signal service is ready. */
    ap_hook_post_config(systemd_post_config, NULL, NULL, APR_HOOK_REALLY_LAST);
    /* We know the PID in this hook ... */
    ap_hook_pre_mpm(systemd_pre_mpm, NULL, NULL, APR_HOOK_LAST);
    /* Used to update httpd's status line using sd_notifyf */
    ap_hook_monitor(systemd_monitor, NULL, NULL, APR_HOOK_MIDDLE);
}

AP_DECLARE_MODULE(systemd) = {
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    systemd_register_hooks,
};
