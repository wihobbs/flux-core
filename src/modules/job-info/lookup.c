/************************************************************\
 * Copyright 2018 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* lookup.c - lookup in job-info */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <flux/core.h>
#include <assert.h>

#include "src/common/libczmqcontainers/czmq_containers.h"
#include "src/common/libeventlog/eventlog.h"
#include "src/common/libjob/idf58.h"
#include "src/common/libutil/jpath.h"
#include "ccan/str/str.h"

#include "job-info.h"
#include "lookup.h"
#include "allow.h"

struct lookup_ctx {
    struct info_ctx *ctx;
    const flux_msg_t *msg;
    flux_jobid_t id;
    json_t *keys;
    bool lookup_eventlog;
    int flags;
    flux_future_t *f;
    bool allow;
};

static void info_lookup_continuation (flux_future_t *fall, void *arg);

static void lookup_ctx_destroy (void *data)
{
    if (data) {
        struct lookup_ctx *ctx = data;
        flux_msg_decref (ctx->msg);
        json_decref (ctx->keys);
        flux_future_destroy (ctx->f);
        free (ctx);
    }
}

static struct lookup_ctx *lookup_ctx_create (struct info_ctx *ctx,
                                             const flux_msg_t *msg,
                                             flux_jobid_t id,
                                             json_t *keys,
                                             int flags)
{
    struct lookup_ctx *l = calloc (1, sizeof (*l));
    int saved_errno;

    if (!l)
        return NULL;

    l->ctx = ctx;
    l->id = id;
    l->flags = flags;

    if (!(l->keys = json_copy (keys))) {
        errno = ENOMEM;
        goto error;
    }

    l->msg = flux_msg_incref (msg);

    return l;

error:
    saved_errno = errno;
    lookup_ctx_destroy (l);
    errno = saved_errno;
    return NULL;
}

static int lookup_key (struct lookup_ctx *l,
                       flux_future_t *fall,
                       const char *key)
{
    flux_future_t *f = NULL;
    char path[64];

    /* Check for duplicate key, return if already looked up */
    if (flux_future_get_child (fall, key) != NULL)
        return 0;

    if (flux_job_kvs_key (path, sizeof (path), l->id, key) < 0) {
        flux_log_error (l->ctx->h, "%s: flux_job_kvs_key", __FUNCTION__);
        goto error;
    }

    if (!(f = flux_kvs_lookup (l->ctx->h, NULL, 0, path))) {
        flux_log_error (l->ctx->h, "%s: flux_kvs_lookup", __FUNCTION__);
        goto error;
    }

    if (flux_future_push (fall, key, f) < 0) {
        flux_log_error (l->ctx->h, "%s: flux_future_push", __FUNCTION__);
        goto error;
    }

    return 0;

error:
    flux_future_destroy (f);
    return -1;
}

static int lookup_keys (struct lookup_ctx *l)
{
    flux_future_t *fall = NULL;
    size_t index;
    json_t *key;

    if (!(fall = flux_future_wait_all_create ())) {
        flux_log_error (l->ctx->h, "%s: flux_wait_all_create", __FUNCTION__);
        goto error;
    }
    flux_future_set_flux (fall, l->ctx->h);

    if (l->lookup_eventlog) {
        if (lookup_key (l, fall, "eventlog") < 0)
            goto error;
    }

    json_array_foreach (l->keys, index, key) {
        if (lookup_key (l, fall, json_string_value (key)) < 0)
            goto error;
    }

    if (flux_future_then (fall,
                          -1,
                          info_lookup_continuation,
                          l) < 0) {
        flux_log_error (l->ctx->h, "%s: flux_future_then", __FUNCTION__);
        goto error;
    }

    l->f = fall;
    return 0;

error:
    flux_future_destroy (fall);
    return -1;
}

static void apply_updates_R (struct lookup_ctx *l,
                             const char *key,
                             json_t *update_object,
                             json_t *context)
{
    const char *context_key;
    json_t *value;

    json_object_foreach (context, context_key, value) {
        /* RFC 21 resource-update event only allows update
         * to:
         * - expiration
         */
        if (streq (context_key, "expiration"))
            if (jpath_set (update_object,
                           "execution.expiration",
                           value) < 0)
                flux_log (l->ctx->h, LOG_INFO,
                          "%s: failed to update job %s %s",
                          __FUNCTION__, idf58 (l->id), key);
    }
}

static int lookup_current (struct lookup_ctx *l,
                           flux_future_t *fall,
                           const char *key,
                           const char *value,
                           char **current_value)
{
    flux_future_t *f_eventlog;
    const char *s_eventlog;
    json_t *value_object = NULL;
    json_t *eventlog = NULL;
    size_t index;
    json_t *entry;
    const char *update_event_name = NULL;
    char *value_object_str = NULL;
    int save_errno;

    if (streq (key, "R"))
        update_event_name = "resource-update";

    if (!(value_object = json_loads (value, 0, NULL))) {
        errno = EINVAL;
        goto error;
    }

    if (!(f_eventlog = flux_future_get_child (fall, "eventlog"))) {
        flux_log_error (l->ctx->h, "%s: flux_future_get_child",
                        __FUNCTION__);
        goto error;
    }

    if (flux_kvs_lookup_get (f_eventlog, &s_eventlog) < 0) {
        if (errno != ENOENT)
            flux_log_error (l->ctx->h, "%s: flux_kvs_lookup_get",
                            __FUNCTION__);
        goto error;
    }

    if (!(eventlog = eventlog_decode (s_eventlog))) {
        errno = EINVAL;
        goto error;
    }

    json_array_foreach (eventlog, index, entry) {
        const char *name;
        json_t *context = NULL;
        if (eventlog_entry_parse (entry, NULL, &name, &context) < 0)
            goto error;
        if (streq (name, update_event_name)) {
            if (streq (key, "R"))
                apply_updates_R (l, key, value_object, context);
        }
    }

    if (!(value_object_str = json_dumps (value_object, 0)))
        goto error;

    (*current_value) = value_object_str;
    json_decref (eventlog);
    json_decref (value_object);
    return 0;

error:
    save_errno = errno;
    json_decref (eventlog);
    json_decref (value_object);
    free (value_object_str);
    errno = save_errno;
    return -1;
}

static void info_lookup_continuation (flux_future_t *fall, void *arg)
{
    struct lookup_ctx *l = arg;
    struct info_ctx *ctx = l->ctx;
    const char *s;
    char *current_value = NULL;
    size_t index;
    json_t *key;
    json_t *o = NULL;
    json_t *tmp = NULL;
    char *data = NULL;

    if (!l->allow) {
        flux_future_t *f;

        if (!(f = flux_future_get_child (fall, "eventlog"))) {
            flux_log_error (ctx->h, "%s: flux_future_get_child", __FUNCTION__);
            goto error;
        }

        if (flux_kvs_lookup_get (f, &s) < 0) {
            if (errno != ENOENT)
                flux_log_error (l->ctx->h, "%s: flux_kvs_lookup_get", __FUNCTION__);
            goto error;
        }

        if (eventlog_allow (ctx, l->msg, l->id, s) < 0)
            goto error;
        l->allow = true;
    }

    if (!(o = json_object ()))
        goto enomem;

    tmp = json_integer (l->id);
    if (json_object_set_new (o, "id", tmp) < 0) {
        json_decref (tmp);
        goto enomem;
    }

    json_array_foreach (l->keys, index, key) {
        flux_future_t *f;
        const char *keystr = json_string_value (key); /* validated earlier */
        json_t *val = NULL;

        if (!(f = flux_future_get_child (fall, keystr))) {
            flux_log_error (ctx->h, "%s: flux_future_get_child", __FUNCTION__);
            goto error;
        }

        if (flux_kvs_lookup_get (f, &s) < 0) {
            if (errno != ENOENT)
                flux_log_error (l->ctx->h, "%s: flux_kvs_lookup_get", __FUNCTION__);
            goto error;
        }

        /* treat empty value as invalid */
        if (!s) {
            errno = EPROTO;
            goto error;
        }

        if ((l->flags & FLUX_JOB_LOOKUP_CURRENT)
            && streq (keystr, "R")) {
            if (lookup_current (l, fall, keystr, s, &current_value) < 0)
                goto error;
            s = current_value;
        }

        /* check for JSON_DECODE flag last, as changes above could affect
         * desired value */
        if ((l->flags & FLUX_JOB_LOOKUP_JSON_DECODE)
            && (streq (keystr, "jobspec")
                || streq (keystr, "R"))) {
            /* We assume if it was stored in the KVS it's valid JSON,
             * so failure is ENOMEM */
            if (!(val = json_loads (s, 0, NULL)))
                goto enomem;
        }
        else {
            if (!(val = json_string (s)))
                goto enomem;
        }

        if (json_object_set_new (o, keystr, val) < 0) {
            json_decref (val);
            goto enomem;
        }

        free (current_value);
        current_value = NULL;
    }

    /* must have been allowed earlier or above, otherwise should have
     * taken error path */
    assert (l->allow);

    if (!(data = json_dumps (o, JSON_COMPACT)))
        goto enomem;

    if (flux_respond (ctx->h, l->msg, data) < 0) {
        flux_log_error (ctx->h, "%s: flux_respond", __FUNCTION__);
        goto error;
    }

    goto done;

enomem:
    errno = ENOMEM;
error:
    if (flux_respond_error (ctx->h, l->msg, errno, NULL) < 0)
        flux_log_error (ctx->h, "%s: flux_respond_error", __FUNCTION__);

done:
    /* flux future destroyed in lookup_ctx_destroy, which is called
     * via zlist_remove() */
    json_decref (o);
    free (data);
    free (current_value);
    zlist_remove (ctx->lookups, l);
}

/* Check if lookup allowed, either b/c message is from instance owner
 * or previous lookup verified it's ok.
 */
static int check_allow (struct lookup_ctx *l)
{
    int ret;

    /* if rpc from owner, no need to do guest access check */
    if (flux_msg_authorize (l->msg, FLUX_USERID_UNKNOWN) == 0) {
        l->allow = true;
        return 0;
    }

    if ((ret = eventlog_allow_lru (l->ctx,
                                   l->msg,
                                   l->id)) < 0)
        return -1;

    if (ret) {
        l->allow = true;
        return 0;
    }

    return 0;
}

/* If we need the eventlog for an allow check or for update-lookup
 * we need to add it to the key lookup list.
 */
static int check_to_lookup_eventlog (struct lookup_ctx *l)
{
    if (!l->allow || (l->flags & FLUX_JOB_LOOKUP_CURRENT)) {
        size_t index;
        json_t *key;
        json_array_foreach (l->keys, index, key) {
            if (streq (json_string_value (key), "eventlog"))
                return 0;
        }
        l->lookup_eventlog = true;
    }

    return 0;
}

void lookup_cb (flux_t *h, flux_msg_handler_t *mh,
                const flux_msg_t *msg, void *arg)
{
    struct info_ctx *ctx = arg;
    struct lookup_ctx *l = NULL;
    size_t index;
    json_t *key;
    json_t *keys;
    flux_jobid_t id;
    int flags;
    int valid_flags = FLUX_JOB_LOOKUP_JSON_DECODE | FLUX_JOB_LOOKUP_CURRENT;
    const char *errmsg = NULL;

    if (flux_request_unpack (msg, NULL, "{s:I s:o s:i}",
                             "id", &id,
                             "keys", &keys,
                             "flags", &flags) < 0) {
        flux_log_error (h, "%s: flux_request_unpack", __FUNCTION__);
        goto error;
    }

    if (flags & ~valid_flags) {
        errno = EPROTO;
        errmsg = "lookup request rejected with invalid flag";
        goto error;
    }

    /* validate keys is an array and all fields are strings */
    if (!json_is_array (keys)) {
        errno = EPROTO;
        goto error;
    }

    json_array_foreach (keys, index, key) {
        if (!json_is_string (key)) {
            errno = EPROTO;
            goto error;
        }
    }

    if (!(l = lookup_ctx_create (ctx, msg, id, keys, flags)))
        goto error;

    if (check_allow (l) < 0)
        goto error;

    if (check_to_lookup_eventlog (l) < 0)
        goto error;

    if (lookup_keys (l) < 0)
        goto error;

    if (zlist_append (ctx->lookups, l) < 0) {
        flux_log_error (h, "%s: zlist_append", __FUNCTION__);
        goto error;
    }
    zlist_freefn (ctx->lookups, l, lookup_ctx_destroy, true);

    return;

error:
    if (flux_respond_error (h, msg, errno, errmsg) < 0)
        flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
    lookup_ctx_destroy (l);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
