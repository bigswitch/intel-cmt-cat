/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2023 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

/**
 * @brief Platform QoS utility - capability module
 *
 */

#include "cap.h"

#include "common.h"
#include "main.h"
#include "pqos.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif

#define BUFFER_SIZE 512
#define NON_VERBOSE 0

/**
 * @brief Print line with indentation
 *
 * @param [in] indent indentation level
 * @param [in] format format to produce output according to,
 *                    variable number of arguments
 */
static void
printf_indent(const unsigned indent, const char *format, ...)
{
        printf("%*s", indent, "");

        va_list args;

        va_start(args, format);
        vprintf(format, args);
        va_end(args);
}

/**
 * @brief Print cache information
 *
 * @param [in] indent indentation level
 * @param [in] cache cache information structure
 */
static void
cap_print_cacheinfo(const unsigned indent, const struct pqos_cacheinfo *cache)
{
        ASSERT(cache != NULL);

        printf_indent(indent, "Num ways: %u\n", cache->num_ways);
        printf_indent(indent, "Way size: %u bytes\n", cache->way_size);
        printf_indent(indent, "Num sets: %u\n", cache->num_sets);
        printf_indent(indent, "Line size: %u bytes\n", cache->line_size);
        printf_indent(indent, "Total size: %u bytes\n", cache->total_size);
}

/**
 * @brief Get event name string
 *
 * @param [in] event mon event type
 *
 * @return Mon event name string
 */
static const char *
get_mon_event_name(int event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return "LLC Occupancy (LLC)";
        case PQOS_MON_EVENT_LMEM_BW:
                return "Local Memory Bandwidth (LMEM)";
        case PQOS_MON_EVENT_TMEM_BW:
                return "Total Memory Bandwidth (TMEM)";
        case PQOS_MON_EVENT_RMEM_BW:
                return "Remote Memory Bandwidth (RMEM) (calculated)";
        case PQOS_PERF_EVENT_LLC_MISS:
                return "LLC misses";
        case PQOS_PERF_EVENT_LLC_REF:
                return "LLC references";
        case PQOS_PERF_EVENT_IPC:
                return "Instructions/Clock (IPC)";
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                return "LLC misses - pcie read";
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                return "LLC misses - pcie write";
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                return "LLC references - pcie read";
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                return "LLC references - pcie write";
        default:
                return "unknown";
        }
}

/**
 * @brief Print Monitoring capabilities
 *
 * @param [in] indent indentation level
 * @param [in] mon monitoring capability structure
 * @param [in] verbose verbose mode
 */
static void
cap_print_features_mon(const unsigned indent,
                       const struct pqos_cap_mon *mon,
                       const int verbose)
{
        unsigned i;
        char buffer_cache[BUFFER_SIZE] = "\0";
        char buffer_memory[BUFFER_SIZE] = "\0";
        char buffer_other[BUFFER_SIZE] = "\0";

        ASSERT(mon != NULL);

        /**
         * Iterate through all supported monitoring events
         * and generate capability detail string for each of them
         */
        for (i = 0; i < mon->num_events; i++) {
                const struct pqos_monitor *monitor = &(mon->events[i]);
                char *buffer = NULL;

                switch (monitor->type) {
                case PQOS_MON_EVENT_L3_OCCUP:
                        buffer = buffer_cache;
                        break;

                case PQOS_MON_EVENT_LMEM_BW:
                case PQOS_MON_EVENT_TMEM_BW:
                case PQOS_MON_EVENT_RMEM_BW:
                        buffer = buffer_memory;
                        break;

                case PQOS_PERF_EVENT_LLC_MISS:
                case PQOS_PERF_EVENT_LLC_REF:
                case PQOS_PERF_EVENT_IPC:
                case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                        buffer = buffer_other;
                        break;

                default:
                        break;
                }

                if (buffer == NULL)
                        continue;

                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "%*s%s\n", indent + 8, "",
                         get_mon_event_name(monitor->type));

                if (verbose) {
                        if (monitor->scale_factor != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s scale factor: %u\n", indent + 12,
                                         "", monitor->scale_factor);
                        if (monitor->max_rmid != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s max rmid: %u\n", indent + 12, "",
                                         monitor->max_rmid);
                        if (monitor->counter_length != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s counter length: %ub\n",
                                         indent + 12, "",
                                         monitor->counter_length);
                }
        }

        printf_indent(indent, "Monitoring\n");

        if (strlen(buffer_cache) > 0) {
                printf_indent(indent + 4,
                              "Cache Monitoring Technology (CMT) events:\n");
                printf("%s", buffer_cache);
        }

        if (strlen(buffer_memory) > 0) {
                printf_indent(indent + 4,
                              "Memory Bandwidth Monitoring (MBM) events:\n");
                printf("%s", buffer_memory);
        }

        if (strlen(buffer_other) > 0) {
                printf_indent(indent + 4, "PMU events:\n");
                printf("%s", buffer_other);
        }
}

/**
 * @brief Print L3 CAT capabilities
 *
 * @param [in] indent indentation level
 * @param [in] l3ca L3 CAT capability structure
 * @param [in] verbose verbose mode
 */
static void
cap_print_features_l3ca(const unsigned indent,
                        const struct pqos_cap_l3ca *l3ca,
                        const int verbose)
{
        unsigned min_cbm_bits;

        ASSERT(l3ca != NULL);

        printf_indent(indent, "L3 CAT\n");
        printf_indent(indent + 4, "CDP: %s\n",
                      l3ca->cdp ? (l3ca->cdp_on ? "enabled" : "disabled")
                                : "unsupported");
        printf_indent(indent + 4, "Non-Contiguous CBM: %s\n",
                      l3ca->non_contiguous_cbm ? "supported" : "unsupported");
        printf_indent(indent + 4, "Num COS: %u\n", l3ca->num_classes);

        if (!verbose)
                return;

        printf_indent(indent + 4, "Way size: %u bytes\n", l3ca->way_size);
        printf_indent(indent + 4, "Ways contention bit-mask: 0x%lx\n",
                      l3ca->way_contention);
        if (pqos_l3ca_get_min_cbm_bits(&min_cbm_bits) != PQOS_RETVAL_OK)
                printf_indent(indent + 4, "Min CBM bits: unavailable\n");
        else
                printf_indent(indent + 4, "Min CBM bits: %u\n", min_cbm_bits);
        printf_indent(indent + 4, "Max CBM bits: %u\n", l3ca->num_ways);
}

/**
 * @brief Print L2 CAT capabilities
 *
 * @param [in] indent indentation level
 * @param [in] l2ca L2 CAT capability structure
 * @param [in] verbose verbose mode
 */
static void
cap_print_features_l2ca(const unsigned indent,
                        const struct pqos_cap_l2ca *l2ca,

                        const int verbose)
{
        unsigned min_cbm_bits;

        ASSERT(l2ca != NULL);

        printf_indent(indent, "L2 CAT\n");
        printf_indent(indent + 4, "CDP: %s\n",
                      l2ca->cdp ? (l2ca->cdp_on ? "enabled" : "disabled")
                                : "unsupported");
        printf_indent(indent + 4, "Non-Contiguous CBM: %s\n",
                      l2ca->non_contiguous_cbm ? "supported" : "unsupported");
        printf_indent(indent + 4, "Num COS: %u\n", l2ca->num_classes);

        if (!verbose)
                return;

        printf_indent(indent + 4, "Way size: %u bytes\n", l2ca->way_size);
        printf_indent(indent + 4, "Ways contention bit-mask: 0x%lx\n",
                      l2ca->way_contention);
        if (pqos_l2ca_get_min_cbm_bits(&min_cbm_bits) != PQOS_RETVAL_OK)
                printf_indent(indent + 4, "Min CBM bits: unavailable\n");
        else
                printf_indent(indent + 4, "Min CBM bits: %u\n", min_cbm_bits);
        printf_indent(indent + 4, "Max CBM bits: %u\n", l2ca->num_ways);
}

/**
 * @brief Print MBA capabilities
 *
 * @param [in] indent indentation level
 * @param [in] mba MBA capability structure
 * @param [in] verbose verbose mode
 */
static void
cap_print_features_mba(const unsigned indent,
                       const struct pqos_cap_mba *mba,
                       const int verbose)
{
        ASSERT(mba != NULL);

        printf_indent(indent, "Memory Bandwidth Allocation (MBA)\n");
        printf_indent(indent + 4, "Num COS: %u\n", mba->num_classes);

        if (mba->ctrl != -1) {
                const char *ctrl_status = NULL;

                if (!mba->ctrl)
                        ctrl_status = "unsupported";
                else if (!mba->ctrl_on)
                        ctrl_status = "disabled";
                else if (mba->ctrl_on == 1)
                        ctrl_status = "enabled";

                if (ctrl_status)
                        printf_indent(indent + 4, "CTRL: %s\n", ctrl_status);
        }

        if (!verbose)
                return;

        printf_indent(indent + 4, "Granularity: %u\n", mba->throttle_step);
        printf_indent(indent + 4, "Min B/W: %u\n", 100 - mba->throttle_max);
        printf_indent(indent + 4, "Type: %s\n",
                      mba->is_linear ? "linear" : "nonlinear");
}

/**
 * @brief Print capabilities
 *
 * @param [in] cap system capability structure
 * @param [in] cpu CPU topology structure
 * @param [in] verbose verbose mode
 */
void
cap_print_features(const struct pqos_cap *cap,
                   const struct pqos_cpuinfo *cpu,
                   const int verbose)
{
        unsigned i;
        const struct pqos_capability *cap_mon = NULL;
        const struct pqos_capability *cap_l3ca = NULL;
        const struct pqos_capability *cap_l2ca = NULL;
        const struct pqos_capability *cap_mba = NULL;
        enum pqos_interface interface;
        int ret;

        if (cap == NULL || cpu == NULL)
                return;

        for (i = 0; i < cap->num_cap; i++)
                switch (cap->capabilities[i].type) {
                case PQOS_CAP_TYPE_MON:
                        cap_mon = &(cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_L3CA:
                        cap_l3ca = &(cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_L2CA:
                        cap_l2ca = &(cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_MBA:
                        cap_mba = &(cap->capabilities[i]);
                        break;
                default:
                        break;
                }

        if (cap_mon == NULL && cap_l3ca == NULL && cap_l2ca == NULL &&
            cap_mba == NULL)
                return;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK)
                return;

        if (interface == PQOS_INTER_MSR)
                printf("Hardware capabilities\n");

#ifdef __linux__
        else {
                struct utsname name;

                printf("OS capabilities");
                if (uname(&name) >= 0)
                        printf(" (%s kernel %s)", name.sysname, name.release);
                printf("\n");
        }
#endif

        /**
         * Monitoring capabilities
         */
        if (cap_mon != NULL)
                cap_print_features_mon(4, cap_mon->u.mon, verbose);

        if (cap_l3ca != NULL || cap_l2ca != NULL || cap_mba != NULL)
                printf_indent(4, "Allocation\n");

        /**
         * Cache Allocation capabilities
         */
        if (cap_l3ca != NULL || cap_l2ca != NULL)
                printf_indent(8, "Cache Allocation Technology (CAT)\n");

        if (cap_l3ca != NULL)
                cap_print_features_l3ca(12, cap_l3ca->u.l3ca, verbose);

        if (cap_l2ca != NULL)
                cap_print_features_l2ca(12, cap_l2ca->u.l2ca, verbose);

        /**
         * Memory Bandwidth Allocation capabilities
         */
        if (cap_mba != NULL)
                cap_print_features_mba(8, cap_mba->u.mba, verbose);

        if (!verbose)
                return;

        printf("Cache information\n");

        if (cpu->l3.detected) {
                printf_indent(4, "L3 Cache\n");
                cap_print_cacheinfo(8, &(cpu->l3));
        }

        if (cpu->l2.detected) {
                printf_indent(4, "L2 Cache\n");
                cap_print_cacheinfo(8, &(cpu->l2));
        }
}
