/*
 * lisp_ms.c
 *
 * This file is part of LISP Mobile Node Implementation.
 *
 * Copyright (C) 2012 Cisco Systems, Inc, 2012. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Please send any bug reports or fixes you make to the email address(es):
 *    LISP-MN developers <devel@lispmob.org>
 *
 * Written or modified by:
 *    Florin Coras <fcoras@ac.upc.edu>
 */

#include "lisp_ms.h"

int ms_process_map_request_msg(map_request_msg *mreq, lisp_addr_t *local_rloc, uint16_t dst_port);

void ms_ctrl_start(lisp_ctrl_device *dev) {
    lispd_log_msg(LISP_LOG_DEBUG_1, "Starting Map-Server ...");
}


int ms_process_map_request_msg(map_request_msg *mreq, lisp_addr_t *local_rloc, uint16_t dst_port)
{
    lisp_addr_t                 *src_eid                = NULL;
    lisp_addr_t                 *dst_eid                = NULL;
    lisp_addr_t                 *remote_rloc            = NULL;
    address_field               **itrs                  = NULL;
    eid_prefix_record           **eids                  = NULL;
    lispd_mapping_elt           *mapping                = NULL;
    map_reply_opts              opts;
    int i;

    lispd_log_msg(LISP_LOG_DEBUG_3, "Map-Server: Processing LISP Map-Request message");

    if (mreq_msg_get_hdr(mreq)->rloc_probe) {
        lispd_log_msg(LISP_LOG_DEBUG_3, "Map-Server: Received LISP Map-Request message with Probe bit set. Discarding!");
        return(BAD);
    }

    if (mreq_msg_get_hdr(mreq)->solicit_map_request) {
        lispd_log_msg(LISP_LOG_DEBUG_3, "Map-Server: Received LISP Map-Request message with SMR bit set. Discarding!");
        return(BAD);
    }

    if (!(src_eid = lisp_addr_init_from_field(mreq_msg_get_src_eid(mreq))))
        return(BAD);

    /* Process additional ITR RLOCs. Obtain remote RLOC to use for Map-Replies*/
    itrs = mreq_msg_get_itr_rlocs(mreq);
    for (i = 0; i < mreq_msg_get_hdr(mreq)->additional_itr_rloc_count + 1; i++) {
        /* XXX: support only for IP RLOCs */
        if (ip_iana_afi_to_sock_afi(address_field_get_afi(itrs[i])) == lisp_addr_ip_get_afi(local_rloc)) {
            remote_rloc = lisp_addr_init_from_field(itrs[i]);
            break;
        }
    }

    if (!remote_rloc){
        lispd_log_msg(LISP_LOG_DEBUG_3,"Map-Server: No supported AFI in the list of ITR-RLOCS");
        goto err;
    }

    /* Set flags for Map-Reply */
    opts.send_rec   = 0;
    opts.echo_nonce = 0;
    opts.rloc_probe = 0;

    /* Process record and send Map Reply for each one */
    eids = mreq_msg_get_eids(mreq);
    for (i = 0; i < mreq_msg_get_hdr(mreq)->record_count; i++) {
        if (!(dst_eid = lisp_addr_init_from_field(eid_prefix_record_get_eid(eids[i]))))
            goto err;

        /* Save prefix length only if the entry is an IP */
        if (lisp_addr_get_afi(dst_eid) == LM_AFI_IP)
            ip_prefix_set_plen(lisp_addr_get_ippref(dst_eid), eid_prefix_record_get_hdr(eids[i])->eid_prefix_length);

        lispd_log_msg(LISP_LOG_DEBUG_1, "Map-Server: received Map-Request from EID %s for EID %s",
                lisp_addr_to_char(src_eid), lisp_addr_to_char(dst_eid));

        /* Check the existence of the requested EID */
        if (!(mapping = local_map_db_lookup_eid(dst_eid))){
            lispd_log_msg(LISP_LOG_DEBUG_1,"Map-Server: the requested EID %s is not registered",
                    lisp_addr_to_char(dst_eid));
            lisp_addr_del(dst_eid);
            continue;
        }

//        if (is_mrsignaling(eid_prefix_record_get_eid(eid)))
//            return(mrsignaling_recv_mrequest(mreq, dst_eid, local_rloc, remote_rloc, dst_port));
        err = build_and_send_map_reply_msg(mapping, local_rloc, remote_rloc, dst_port, mreq_msg_get_hdr(mreq)->nonce, opts);

        lisp_addr_del(dst_eid);
    }

    lisp_addr_del(src_eid);
    lisp_addr_del(remote_rloc);
    return(GOOD);
err:
    lisp_addr_del(src_eid);
    if (remote_rloc)
        lisp_addr_del(remote_rloc);
    if (dst_eid)
        lisp_addr_del(dst_eid);
    return(BAD);
}

lispd_mapping_elt *process_mapping_record(mapping_record *record) {
    lispd_mapping_elt   *mapping    = NULL;
    lisp_addr_t         *eid        = NULL;
    locator_field       **locs      = NULL;
    lispd_locator_elt   *loc        = NULL;
    int i;


    eid = lisp_addr_init_from_field(mapping_record_get_eid(record));
    if (!eid)
        goto err;

    mapping = mapping_init(eid);
    if (!mapping)
        goto err;

    locs = mapping_record_get_locators(record);
    for (i = 0; i < mapping_record_get_hdr(record)->locator_count; i++) {
        if (!(loc = locator_init_from_field(locs[i])))
            goto err;
        lispd_log_msg(LISP_LOG_DEBUG_1, "    RLOC: %s", locator_to_char(loc));
        if (add_locator_to_mapping(mapping, loc) != GOOD)
            goto err;
    }
    return(mapping);

err:
    if (eid)
        lisp_addr_del(eid);
    if (mapping)
        free_mapping_elt(mapping, 0);
    return(NULL);
}

int ms_process_map_register_msg(map_register_msg *mreg) {
    int i;
    mapping_record      **records   = NULL;
    lispd_mapping_elt   *mapping    = NULL;

    records = mreg_msg_get_records(mreg);
    for (i = 0; i < mreg_msg_get_hdr(mreg)->record_count; i++) {
        mapping = process_mapping_record(records[i]);
        if (!mapping) {
            lispd_log_msg(LISP_LOG_DEBUG_1, "MS: Couldn't process mapping record. Skipping");
            continue;
        }

        /* check authentication */

        local_map_db_add_mapping(mapping);
        /* start timers */
        if (mreg_msg_get_hdr(mreg)->map_notify){
            /* send map-notify */
        }


    }
    return(GOOD);

err:
    return(BAD);
}

int ms_process_lisp_ctrl_msg(lisp_ctrl_device *dev, lisp_msg *msg, lisp_addr_t *local_rloc, uint16_t remote_port) {
    int ret = BAD;

     switch(msg->type) {
     case LISP_MAP_REQUEST:
         ret = ms_process_map_request_msg(msg->msg, local_rloc, remote_port);
         break;
     case LISP_MAP_REGISTER:
         ret = ms_process_map_register_msg(msg->msg);
         break;
     case LISP_MAP_REPLY:
     case LISP_MAP_NOTIFY:
     case LISP_INFO_NAT:
         lispd_log_msg(LISP_LOG_DEBUG_1, "Map-Server: Received control message with type %d. Discarding!",
                 msg->type);
         break;
     default:
         lispd_log_msg(LISP_LOG_DEBUG_1, "Map-Server: Received unidentified type (%d) control message", msg->type);
         ret = BAD;
         break;
     }

     if (ret != GOOD) {
         lispd_log_msg(LISP_LOG_DEBUG_2, "Map-Server: Failed to process LISP control message");
         return(BAD);
     } else {
         lispd_log_msg(LISP_LOG_DEBUG_2, "Map-Server: Completed processing of LISP control message");
         return(ret);
     }
}

ctrl_device_vtable ms_vtable = {
        ms_process_lisp_ctrl_msg,
        ms_ctrl_start
};

lisp_ctrl_device *ms_ctrl_init() {
    lisp_ms *ms;
    ms = calloc(1, sizeof(lisp_ctrl_device));
    ms->super.mode = 2;
    ms->super.vtable = &ms_vtable;
    lispd_log_msg(LISP_LOG_DEBUG_1, "Finished Initializing Map-Server");
    local_map_db_init();

    return((lisp_ctrl_device *)ms);
}

