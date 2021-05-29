#!/bin/sh
#
test_description='Test flux-shell task exit support'

. `dirname $0`/sharness.sh

test_under_flux 2 job

test_expect_success 'flux-shell: first task exit posts shell.task-exit event' '
	jobid=$(flux mini submit /bin/true) &&
	run_timeout 10 flux job wait-event -p guest.exec.eventlog \
		$jobid shell.task-exit
'

test_expect_success 'flux-shell: create 30s sleep script - rank 1 exits early' '
	cat >testscript.sh <<-EOT &&
	#!/bin/bash
	test \$FLUX_TASK_RANK -eq 1 && exit 200
	sleep 30
	EOT
	chmod +x testscript.sh
'

test_expect_success 'flux-shell: run script with 2 tasks and 1s timeout' '
	test_must_fail run_timeout 30 flux mini run \
		-n2 -o exit-timeout=1s ./testscript.sh 2>tmout.err &&
	grep "exception.*timeout" tmout.err
'

test_expect_success 'flux-shell: run script with 2 nodes and 1s timeout' '
	test_must_fail run_timeout 30 flux mini run \
		-n2 -N2 -o exit-timeout=1s ./testscript.sh 2>tmout.err &&
	grep "exception.*timeout" tmout.err
'

test_expect_success 'flux-shell: exit-timeout=aaa is rejected' '
	test_must_fail flux mini run -o exit-timeout=aaa /bin/true
'
test_expect_success 'flux-shell: exit-timeout=false is rejected' '
	test_must_fail flux mini run -o exit-timeout=false /bin/true
'
test_expect_success 'flux-shell: exit-timeout=none is accepted' '
	flux mini run -o exit-timeout=none /bin/true
'
test_expect_success 'flux-shell: exit-timeout=100 is accepted' '
	flux mini run -o exit-timeout=100 /bin/true
'
test_expect_success 'flux-shell: exit-timeout=42.34 is accepted' '
	flux mini run -o exit-timeout=42.34 /bin/true
'

test_done
