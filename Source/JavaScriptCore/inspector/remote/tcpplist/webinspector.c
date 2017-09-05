// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "char_buffer.h"
#include "webinspector.h"

// some arbitrarly limit, to catch bad packets
#define MAX_BODY_LENGTH 1<<24

struct wi_private {
    bool is_sim;
    cb_t in;
    cb_t partial;
    bool has_length;
    size_t body_length;
};

wi_status wi_on_error(wi_t self, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return WI_ERROR;
}

wi_status wi_on_debug(wi_t self, const char *message,
                      const char *buf, size_t length) {
    if (self->is_debug && *self->is_debug) {
        char *text;
        cb_asprint(&text, buf, length, 80, 30);
        printf("%s[%zd]:\n%s\n", message, length, text);
        free(text);
    }
    return WI_SUCCESS;
}

//
// RECV
//

wi_status wi_parse_length(wi_t self, const char *buf, size_t *to_length) {
    if (!buf || !to_length) {
        return WI_ERROR;
    }
    *to_length = (
            ((((unsigned char) buf[0]) & 0xFF) << 24) |
            ((((unsigned char) buf[1]) & 0xFF) << 16) |
            ((((unsigned char) buf[2]) & 0xFF) << 8) |
            (((unsigned char) buf[3]) & 0xFF));
    if (MAX_BODY_LENGTH > 0 && *to_length > MAX_BODY_LENGTH) {
#define TO_CHAR(c) ((c) >= ' ' && (c) < '~' ? (c) : '.')
        return self->on_error(self, "Invalid packet header "
                                      "0x%x%x%x%x == %c%c%c%c == %zd",
                              buf[0], buf[1], buf[2], buf[3],
                              TO_CHAR(buf[0]), TO_CHAR(buf[1]),
                              TO_CHAR(buf[2]), TO_CHAR(buf[3]),
                              *to_length);
    }
    return WI_SUCCESS;
}

wi_status wi_parse_plist(wi_t self, const char *from_buf, size_t length,
                         plist_t *to_rpc_dict, bool *to_is_partial) {
    wi_private_t my = self->private_state;
    *to_rpc_dict = NULL;

        plist_from_bin(from_buf, length, to_rpc_dict);

    return (*to_rpc_dict ? WI_SUCCESS : WI_ERROR);
}

wi_status wi_recv_packet(wi_t self, const char *packet, ssize_t length) {
    wi_on_debug(self, "wi.recv_packet", packet, length);

    size_t body_length = 0;
    plist_t rpc_dict = NULL;
    bool is_partial = false;
    if (!packet || length < 4 || wi_parse_length(self, packet, &body_length) ||
        // (body_length != length - 4) || // TODO
        wi_parse_plist(self, packet + 4, body_length, &rpc_dict, &is_partial)) {

        // invalid packet
        char *text = NULL;
        if (body_length != length - 4) {
            if (asprintf(&text, "size %zd != %zd - 4", body_length, length) < 0) {
                return self->on_error(self, "asprintf failed");
            }
        } else {
            cb_asprint(&text, packet, length, 80, 50);
        }
        wi_status ret = self->on_error(self, "Invalid packet %s\n", text);
        free(text);
        return ret;
    }

    if (is_partial) {
        return WI_SUCCESS;
    }
    wi_status ret = self->recv_plist(self, rpc_dict);
    return ret;
}

wi_status wi_recv_loop(wi_t self) {
    wi_private_t my = self->private_state;
    wi_status ret;
    const char *in_head = my->in->in_head;
    const char *in_tail = my->in->in_tail;
    while (1) {
        size_t in_length = in_tail - in_head;
        if (!my->has_length && in_length >= 4) {
            // can read body_length now
            size_t len;
            ret = wi_parse_length(self, in_head, &len);
            if (ret) {
                in_head += 4;
                break;
            }
            my->body_length = len;
            my->has_length = true;
            // don't advance in_head yet
        } else if (my->has_length && in_length >= my->body_length + 4) {
            // can read body now
            ret = self->recv_packet(self, in_head, my->body_length + 4);
            in_head += my->body_length + 4;
            my->has_length = false;
            my->body_length = 0;
            if (ret) {
                break;
            }
        } else {
            // need more input
            ret = WI_SUCCESS;
            break;
        }
    }
    my->in->in_head = in_head;
    return ret;
}

wi_status wi_on_recv(wi_t self, const char *buf, ssize_t length) {
    wi_private_t my = self->private_state;
    if (length < 0) {
        return WI_ERROR;
    } else if (length == 0) {
        return WI_SUCCESS;
    }
    wi_on_debug(self, "wi.recv", buf, length);
    if (cb_begin_input(my->in, buf, length)) {
        return self->on_error(self, "begin_input buffer error");
    }
    wi_status ret = wi_recv_loop(self);
    if (cb_end_input(my->in)) {
        return self->on_error(self, "end_input buffer error");
    }
    return ret;
}

//
// STRUCTS
//

void wi_private_free(wi_private_t my) {
    if (my) {
        cb_free(my->in);
        cb_free(my->partial);
        memset(my, 0, sizeof(struct wi_private));
        free(my);
    }
}
wi_private_t wi_private_new() {
    wi_private_t my = (wi_private_t)malloc(sizeof(
                                                   struct wi_private));
    if (my) {
        memset(my, 0, sizeof(struct wi_private));
        my->in = cb_new();
        my->partial = cb_new();
        if (!my->in || !my->partial) {
            wi_private_free(my);
            return NULL;
        }
    }
    return my;
}


void wi_free(wi_t self) {
    if (self) {
        wi_private_free(self->private_state);
        memset(self, 0, sizeof(struct wi_struct));
        free(self);
    }
}
wi_t wi_new(bool is_sim) {
    wi_t self = (wi_t)malloc(sizeof(struct wi_struct));
    if (!self) {
        return NULL;
    }
    memset(self, 0, sizeof(struct wi_struct));
    self->on_recv = wi_on_recv;
    self->recv_packet = wi_recv_packet;
    self->on_error = wi_on_error;
    self->private_state = wi_private_new();
    if (!self->private_state) {
        wi_free(self);
        return NULL;
    }
    self->private_state->is_sim = is_sim;
    return self;
}