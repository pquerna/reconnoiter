/*
 * Copyright (c) 2007, OmniTI Computer Consulting, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name OmniTI Computer Consulting, Inc. nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "noit_defines.h"

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "eventer/eventer.h"
#include "utils/noit_log.h"
#include "noit_stats.h"
#include "noit_conf.h"

#ifdef NOIT_STATS_ENABLED

static noit_log_stream_t nlerr = NULL;
static noit_log_stream_t nldeb = NULL;
static int statsd_fd = -1;
static struct sockaddr_in statsd_addr;
static const char *statsd_appname;

void
noit_stats_init(const char *appname)
{
  char *hostname = NULL;
  int port;

  if(!nlerr) nlerr = noit_log_stream_find("error/stats");
  if(!nldeb) nldeb = noit_log_stream_find("debug/stats");

  /* TODO: sample rate */
  /* TODO: sample rate per counter or globs? */
  /* TODO: backends other than statsd */
  /* TODO: configuration of host/port */
  noit_conf_section_t *sconf = noit_conf_get_section(NULL, "/noit/stats");
  if (!sconf) {
    statsd_fd = -1;
    return;
  }

  noit_conf_get_string(sconf, "/hostname", &hostname);
  if(!hostname) hostname = strdup("127.0.0.1");
  if(!noit_conf_get_int(sconf, "/port", &port)) {
    port = 8125;
  }

  statsd_appname = appname;

  memset(&statsd_addr, 0, sizeof(statsd_addr));
  statsd_addr.sin_family = AF_INET;
  statsd_addr.sin_port = htons(port);
  inet_pton(AF_INET, hostname, &statsd_addr.sin_addr);
  free(hostname);

  statsd_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (statsd_fd == -1) {
    noitL(nlerr, "noit_statsd: socket() failed: %d\n", errno);
    return;
  }
}

void
noit_stats_destroy()
{
  if (statsd_fd != -1) {
    close(statsd_fd);
    statsd_fd = -1;
  }
}

static void
noit_stats_send_stat(const char *stat, int value, const char *type)
{
  ssize_t rv;
  socklen_t sl = sizeof(statsd_addr);
  char buf[128];

  /* TODO: sample rate */
  snprintf(buf, sizeof(buf), "%s.%s:%d|%s\n", statsd_appname, stat, value, type);
  rv = sendto(statsd_fd, buf, strlen(buf), 0, (struct sockaddr *)&statsd_addr, sl);
  if (rv == -1) {
    noitL(nlerr, "noit_statsd: sendto() failed: %d\n", errno);
  }
}

void
noit_stats_incr(const char *stat)
{
  if (statsd_fd == -1) return;
  noit_stats_send_stat(stat, 1, "c");
}

void
noit_stats_decr(const char *stat)
{
  if (statsd_fd == -1) return;
  noit_stats_send_stat(stat, -1, "c");
}

void noit_stats_timing(const char *stat, int timems)
{
  if (statsd_fd == -1) return;
  noit_stats_send_stat(stat, 1, "ms");
}

#endif