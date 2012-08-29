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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "noit_module.h"
#include "noit_check.h"
#include "noit_check_tools.h"
#include "utils/noit_log.h"
#include "utils/noit_hash.h"

static noit_log_stream_t nlerr = NULL;
static noit_log_stream_t nldeb = NULL;
static int statsd_fd;
static struct sockaddr_in statsd_addr;

typedef struct _mod_config {
  noit_hash_table *options;
} statsd_emitter_mod_config_t;


static int noit_statsd_emitter_config(noit_module_t *self, noit_hash_table *options) {
  statsd_emitter_mod_config_t *conf;
  conf = noit_module_get_userdata(self);
  if(conf) {
    if(conf->options) {
      noit_hash_destroy(conf->options, free, free);
      free(conf->options);
    }
  }
  else
    conf = calloc(1, sizeof(*conf));
  conf->options = options;
  noit_module_set_userdata(self, conf);
  return 1;
}

static int noit_statsd_emitter_onload(noit_image_t *self) {
  if(!nlerr) nlerr = noit_log_stream_find("error/statsd_emitter");
  if(!nldeb) nldeb = noit_log_stream_find("debug/statsd_emitter");
};

static int noit_statsd_emitter_init(noit_module_t *self) {
  const char *hostname = "127.0.0.1";
  unsigned short port = 8125;
  const char *config_val;
  statsd_emitter_mod_config_t *conf;
  conf = noit_module_get_userdata(self);

  /* TODO: sample rate */
  /* TODO: sample rate per counter or globs? */
  /* TODO: backends other than statsd */
  /* TODO: configuration of host/port */
  noit_conf_section_t *sconf = noit_conf_get_section(NULL, "/noit/statsd_emitter");
  if (!sconf) {
    statsd_fd = -1;
    return 0;
  }

  if(noit_hash_retr_str(conf->options, "port", strlen("port"),
                        (const char **)&config_val)) {
    port = atoi(config_val);
  }

  if(noit_hash_retr_str(conf->options, "hostname", strlen("hostname"),
                        (const char **)&config_val)) {
    hostname = config_val;
  }

  memset(&statsd_addr, 0, sizeof(statsd_addr));
  statsd_addr.sin_family = AF_INET;
  statsd_addr.sin_port = htons(port);
  inet_pton(AF_INET, hostname, &statsd_addr.sin_addr);

  statsd_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (statsd_fd == -1) {
    noitL(nlerr, "statsd_emitter: socket() failed: %d\n", errno);
    return -1;
  }
  return 0;
}

static void
noit_stats_send_stat(const char *stat, int value, const char *type)
{
  ssize_t rv;
  socklen_t sl = sizeof(statsd_addr);
  char buf[128];

  /* TODO: sample rate */
  snprintf(buf, sizeof(buf), "noit.%s:%d|%s\n", stat, value, type);
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

#include "statsd_emitter.xmlh"
noit_module_t statsd_emitter = {
  {
    NOIT_MODULE_MAGIC,
    NOIT_MODULE_ABI_VERSION,
    "statsd_emitter",
    "statsd emitter",
    statsd_emitter_xml_description,
    noit_statsd_emitter_onload
  },
  noit_statsd_emitter_config,
  noit_statsd_emitter_init,
  NULL,
  NULL
};
