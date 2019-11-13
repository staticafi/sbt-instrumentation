#!/usr/bin/env python3

import argparse
import os
import selectors
import subprocess
from subprocess import PIPE, DEVNULL
import sys
import time


def log(msg):
    print('wrapper: ' + str(msg))

err = log

def nstime():
    """
    @return time in nanoseconds
    """
    return time.perf_counter()

def run_predator(timeout, clang, infile):
    """
    @timeout: int, number of seconds to wait until Predator produces result
    @infile: string, path to LLVM bitcode file to process by Predator
    @outfile: string, path to logfile to produce
    """

    start = nstime()
    print('|> slllvm {} {}'.format(' '.join(clang), infile))
    with subprocess.Popen(['slllvm'] + clang + [infile], stdout=PIPE, stderr=PIPE) as proc:
        compilation_result = str(proc.stderr.readline().strip())

        if 'Trying to compile' in compilation_result and 'OK' in compilation_result:
            log('Predator successfully compiled the input file')
        else:
            log('Predator failed to compile the input file')
            log('STDERR')
            print('\n'.join(map(lambda l: str(l).strip(), proc.stderr.readlines())))
            log('STDOUT')
            print('\n'.join(map(lambda l: str(l).strip(), proc.stdout.readlines())))
            return ('error', set())


        error_locations = set()
        sel = selectors.DefaultSelector()
        sel.register(proc.stderr, selectors.EVENT_READ)
        sel.register(proc.stdout, selectors.EVENT_READ)

        while True:
            for key, mask in sel.select():
                fd = key.fileobj
                line = fd.readline()
                line = str(line.strip())
                if 'error:' in line and 'Pass has detected some errors' not in line:
                    [row, col] = line.split(':')[1:3]
                    try:
                        error_locations.add((int(row), int(col)))
                    except ValueError:
                        pass

                # we are not interested in traces
                if 'TRACE' in line:
                    break

            if proc.poll() != None:
                break

            now = nstime()
            elapsed = now - start
            if elapsed > timeout:
                log('timeout, killing Predator')
                proc.kill()
                return ('timeout', set())

    log('Predator finished')
    return ('ok', error_locations)

def write_logfile(res, errs, outfile):
    with open(outfile, 'w') as f:
        f.write('{}\n'.format(res))
        for (line, col) in errs:
            f.write('{} {}\n'.format(line,col))

def assert_in_path(executable):
    ret = subprocess.call(['which', executable], stdout=DEVNULL, stderr=DEVNULL)
    if ret != 0:
        err('`which {}` failed with error code {}'.format(executable, ret))
        sys.exit(1)
    log('{} exists in PATH'.format(executable))

def create_parser():
    parser = argparse.ArgumentParser(description='Run Predator with timeout and process output')
    parser.add_argument('--debug', default=False, action='store_true', help='enable debugging of this script and Predator')
    parser.add_argument('--timeout', '-t', default=20, type=int, metavar='SEC', help='number of seconds to wait until Predator produces result (default:20)')
    parser.add_argument('infile', metavar='program.bc', help='path to LLVM bitcode file to process')
    parser.add_argument('--out', '-o', default='predator.log', metavar='FILE', help='where to save processed log output (default:"predator.log")')
    parser.add_argument('--32', dest='is32bit', action='store_true', default=False, help='use 32-bit mode (default: disabled)')
    return parser

if __name__ == '__main__':
    parser = create_parser()
    args = parser.parse_args()

    # if debugging is disabled, override the log function by a nop
    if not args.debug:
        log = lambda msg: msg

    log('args: {}'.format(str(vars(args))))

    assert_in_path('slllvm')

    clang = []
    if args.is32bit:
        clang.append('-m32')

    res, errs = run_predator(args.timeout, clang, args.infile)
    log('Writing logfile to {}'.format(args.out))
    write_logfile(res, errs, args.out)
