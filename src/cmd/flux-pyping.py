##############################################################
# Copyright 2023 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import argparse
import flux
import flux.job
import json
from flux.constants import FLUX_NODEID_ANY

class PingData:
    def __init__(self, topic_string, total_to_send, node=FLUX_NODEID_ANY):
        self.topic_string = str(topic_string) + ".ping"
        self.total_to_send = int(total_to_send)
        self.node = node
        self.been_sent = int(0)
    
    def timer_cb(self, fh, _t, msg, args):
        if self.total_to_send >= self.been_sent:
            return 0
        fut = fh.rpc(self.topic_string, payload=msg, nodeid=self.node)
        fut.then(self.recv_response_ping(fh, fut)) ## receive the response here
        self.been_sent += 1
    
    def recv_response_ping(self, fh, future):
        pass
        

def main():
    parser = argparse.ArgumentParser(
        prog="flux-pyping",
        description="Ping another broker or service through Python APIs"
    )
    parser.add_argument(
        "-i", "--interval",
        dest="interval",
        help="amount of time between requests, default is 1s"
    )
    parser.add_argument(
        "-c", "--count",
        dest="count",
        help="number of requests to send"
    )
    parser.add_argument(
        "topic",
        help="the flux service you would like to ping, i.e. kvs"
    )

    args = parser.parse_args()

    fh = flux.Flux()

    ping = PingData(args.topic, args.count) ## replace with CL args

    watcher = fh.timer_watcher_create(0, ping.timer_cb, args.interval)

    watcher.start()

    fh.reactor_run()

if __name__ == "__main__":
    main()
