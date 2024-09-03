#!/bin/sh

test_description='flux-resource status general tests'

. $(dirname $0)/sharness.sh

test_under_flux 4

export FLUX_PYCLI_LOGLEVEL=10

test_expect_success 'flux-resource status: works' '
	flux resource status
'
test_expect_success 'flux-resource status: --states=help reports valid states' '
	flux resource status --states=help >status-help.out 2>&1 &&
	test_debug "cat status-help.out" &&
	grep "valid states:" status-help.out
'
test_expect_success 'flux-resource status: bad --states reports valid states' '
	test_expect_code 1 flux resource status --states=foo >status-bad.out 2>&1 &&
	test_debug "cat status-bad.out" &&
	grep "valid states:" status-bad.out
'
test_expect_success 'flux-resource status -s all shows all expected states' '
	flux resource status -s all >status-all.out &&
	test_debug "cat status-all.out" &&
	grep avail status-all.out &&
	grep exclude status-all.out &&
	grep torpid status-all.out &&
	grep allocated status-all.out &&
	grep draining status-all.out &&
	grep drained status-all.out
'
test_expect_success 'flux-resource status: allocated set is not displayed by default' '
	flux submit -N1 --wait-event=alloc sleep inf &&
	flux resource status >status-noalloc.out &&
	test_must_fail grep allocated status-noalloc.out
'
test_expect_success 'flux-resource status: allocated nodes can be displayed' '
	flux resource status -s allocated | grep allocated
'
test_done
