#!/usr/bin/env python
"""
bio.py - replacement for block IO testing in shell
    major difference is, we just extract results, we don't tally
    and leave data processing to post analysis


By default, it is expected this script will be launched from a
control host and pdsh into slave hosts (with local devices) and
perform block-level tests.

A copy of "fair-lio" binary should be accessible through absolute path.
You can change the path with --fbin option though.

Example 1: To survey block devices /dev/sdc /dev/sdd on host 'warp1a'
Only 1 iteration per test, 10 seconds per test.

    ./bio.py --mode survey-lun --hosts warp1a --devices /dev/sdc /dev/sdd \
                    --iters 1 --runtime 10 -v

Example 2: You can also use wildcards to expand the device list such as:

    ./bio.py --mode survey-lun --host warp1a --devices /dev/sd{c..z} \
                    --iters 1 --runtime 10 -v

The bash brace expansion using ranges /dev/sd{c..z} from
/dev/sdc /dev/sdd /dev/sde all the way to /dev/sdz.

Runing locally, you can use --disable-pdsh to disable pdsh-based invokation.
This generally saves some time and more reliable.


TODO:

    - add uniform check, which survey individual LUNs only.



"""
__author__ = "fwang2@gmail.com"
import argparse
import random
import time
import shlex
import subprocess
import re
import testutils
import signal
import sys

from color import hprint, eprint

args = None
runs = []
output = None

def parse_args():
    parser = argparse.ArgumentParser(description="Block IO Test Program")
    parser.add_argument('--mode', choices=["survey-lun", "survey-host"], required=True,
            help="Pick a running mode")
    parser.add_argument('--devices', nargs="+", required=True, help="device list")
    parser.add_argument('--hosts', nargs="+", required=True, help="host name")
    parser.add_argument('--blocksizes', nargs="+",
        default=[4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 4194304],
        help="a list block transfer size to step through, default: from 4KB to 4MB")
    parser.add_argument('--qdepth', nargs="+",
            default=[2, 4, 8, 16], help="a list of queue depth. Default: 2,4,8,16")
    parser.add_argument('--runtime', type=int, default=30, help="runtime per test, default: 30")
    parser.add_argument('--delay', type=int, default=5, help="delay in between test, default: 5")
    parser.add_argument('--emailto', default=None, help="email results to someone")
    parser.add_argument('--iomode', nargs="+",
            default=['seq_wr', 'seq_rd', 'rand_wr', 'rand_rd'],
            help="IO patterns for test, default: seq_wr, seq_rd, rand_wr, rand_rd")
    parser.add_argument('--iters', type=int, default=3,
            help="iteration for each run. Default: 3")
    parser.add_argument('--steps', type=int, default=0, help="enumerate devices, default is 0, means no")
    parser.add_argument('-v', '--verbose', default=False, action="store_true", help="debug output")
    parser.add_argument('--saveruns', default=False, action="store_true", help="Save run permu")
    parser.add_argument('--dryrun', default=False, action="store_true", help="Dry run only, default: false")
    parser.add_argument('--fbin', default="/root/ioutils/bio-fairlio/fair-lio",
            help="Specify a path to fair-lio. Default:/root/ioutils/bio-fair/fair-lio")
    parser.add_argument('--disable-pdsh', default=False, action="store_true", help="Don't use pdsh")

    myargs = parser.parse_args()
    return myargs


def gen_system_settings():
    strbuf = []

    strbuf.append("Run parameters:")
    strbuf.append("\t hosts = %s" % args.hosts)
    strbuf.append("\t devices = %s" % args.devices)
    strbuf.append("\t blocksizes = %s" % args.blocksizes)
    strbuf.append("\t queue depth = %s" % args.qdepth)
    strbuf.append("\t runtime = %s" % args.runtime)
    strbuf.append("\t delay = %s" % args.delay)
    strbuf.append("\t iomode = %s" % args.iomode)
    strbuf.append("\t iters = %s" % args.iters)
    strbuf.append("\t steps = %s" % args.steps)

    strbuf.append("\nSystem settings:")

    settings =  "\n".join(strbuf)

    if args.verbose:
        print settings

    return settings

def permu_parameters(devices):
    """
    given a list of devices, generate corresponding permutation
    with parameters space
    """
    global runs

    for iomode in args.iomode:
        for qs in args.qdepth:
            for bs in args.blocksizes:
                for (iter, val) in enumerate(range(args.iters)):
                    # emulate bash $RANDOM which return random integer
                    # 0 - 32767, signed 16-bit integer
                    offset = random.randint(1, 32767)
                    runs.append([iomode, qs, bs, iter+1, offset, len(devices), devices])


def gen_run_permutation():
    """
    Generate permutations for all the runs
    """

    num_devs = len(args.devices)
    devices = args.devices

    if args.steps != 0:
        steps = range(1, num_devs, args.steps)
        # we want the last step to include all devices, right?
        if steps[-1] != num_devs:
            steps.append(num_devs)

        for n in steps:
            devlist = random.sample(devices, n)
            permu_parameters(devlist)
    else:
        permu_parameters(devices)

def extract_result(out, err):
    """
    extract something like this:
    feral01: ...
    feral01: total: 2658516992 126.77 MB/s skew 0.52 MB/s
    """
    if err:
        eprint("Error on extracting result\n")
        eprint(err)
        sys.exit(1)

    res = re.search(r"total:\s+(.+)\s+MB/s skew", out)

    # res = (2658516992 126.77)

    return res.group(1).split()[1]


def cmd_str(hosts, devices, iomode, qd, bs, offset=0):
    """
    expect hosts to be list involved in the test
    expect device to be list
    """

    cmd_args = []

    if not isinstance(hosts, list):
        hosts = [hosts]
    if not isinstance(devices, list):
        devices = [devices]

    hosts = ",".join(hosts)
    devices = " ".join(devices)

    ioflags = None

    if iomode == "seq_wr":
        ioflags = "-w"
    elif iomode == "seq_rd":
        ioflags = ""
    elif iomode == "rand_rd":
        ioflags = "--random=%s" % offset
    elif iomode == "rand_wr":
        ioflags = "-w --random=%s" % offset
    else:
        eprint("Unknow iomode %s" % iomode)
        sys.exit(1)


    if not args.disable_pdsh:
        cmd_args.append("pdsh -w")
        cmd_args.append(hosts)

    # fair-lio
    cmd_args.append(args.fbin)
    cmd_args.append("-q %s -s %s -t %s %s %s" % (qd, bs, args.runtime, ioflags, devices))

    return " ".join(cmd_args)


def iter_devices(host, devices):
    """
    for this host, and do permutation for specified device list
    """
    num_devs = 1 # only single device is allowed
    for qd in args.qdepth:
        for bs in args.blocksizes:
            for iomode in args.iomode:
                for iter in range(args.iters):
                    cmd = cmd_str(host, devices, iomode, qd, bs)
                    if args.verbose or args.dryrun:
                        print "\t", cmd

                    if not args.dryrun:
                        p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        stdout, stderr = p.communicate()
                        bw = extract_result(stdout, stderr)
                        record("%s, %s, %s, %s, %s, %s, %s, %s" % (host, devices, iomode, bs, qd, num_devs, bw, iter+1))

                        time.sleep(args.delay)


def do_survey_lun():

    for host in args.hosts:
        for dev in args.devices:
            iter_devices(host, dev)

def do_survey_host():
    global runs

    # before we run, randomize the list
    random.shuffle(runs)

    print "We have a total of %i runs" % len(runs)

    if len(args.hosts) == 1:
        host = args.hosts[0]
    else:
        eprint("Receive more than one host in survey host mode")
        sys.exit(1)

    for index,item in enumerate(runs):

        # extract info
        # expecting devices to be a list
        # since cmd_str() expecting a list
        iomode, qd, bs, iter, offset, num_devs, devices = item

        hprint("run %s/%s" % (index+1, len(runs)))
        cmd = cmd_str(args.hosts, devices, iomode, qd, bs, offset)
        if args.verbose or args.dryrun:
            print "\t", cmd

        if not args.dryrun:
            p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = p.communicate()
            bw = extract_result(stdout, stderr)

            # record the result
            if len(devices) > 1:
                devices = " ".join(devices)

            record("%s, %s, %s, %s, %s, %s, %s, %s" % (host, devices, iomode, bs, qd, num_devs, bw, iter))

            time.sleep(args.delay)

def save_runs(file="runs.txt"):
    with open(file, "w") as f:
        for run in runs:
            f.write(str(run))
            f.write("\n")

def record(str):
    with open(output, "a") as f:
        f.write(str)
        f.write("\n")

def main():

    global args, output

    signal.signal(signal.SIGINT, testutils.sig_handler)

    args = parse_args()
    ts = testutils.timestamp()
    output = "bio-run-results-%s.csv" % ts
    permuf = "bio-permu-%s.txt" % ts
    if not args.dryrun:
        record("host, devices, io-mode, block-size, queue-size, num-devices, bw, iter")

    if args.verbose:
        print args

    settings = gen_system_settings()

    if args.mode == "survey-host":
        gen_run_permutation()
        do_survey_host()

        # check if we need to save permutated test runs
        if args.saveruns:
            save_runs(file=permuf)

    elif args.mode == "survey-lun":
        do_survey_lun()
    else:
        print "Unknown mode"
        sys.exit(1)

    # email the testing results

    if args.emailto is not None:
        testutils.send_mail("me",
                [args.emailto],
                "BIO test run %s" % output,  # subject
                settings,                    # message body
                files = [output])

if __name__ == "__main__": main()

