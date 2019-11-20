#!/usr/bin/env python3

from subprocess import PIPE, DEVNULL
import argparse
import os
import re
import selectors
import subprocess
import sys
import time

class ErrorReport:
    """
    An error type and its location.
    Location is given by integers @row and @col.
    Type of the error is given by string @ty.
        Current possible values for ty: "leak", "invalid"
    """
    def __init__(self, row, col, ty):
        self.row = row
        self.col = col
        self.ty = ty

    def __key(self):
        return (self.row, self.col, self.ty)

    def __hash__(self):
        return hash(self.__key())

    def __eq__(self, other):
        if isinstance(other, ErrorReport):
            return self.__key() == other.__key()
        return NotImplemented

def log(msg):
    print('wrapper: ' + str(msg))

err = log

def nstime():
    """
    @return time in nanoseconds
    """
    return time.perf_counter()

def parse_errors(line):
    """
    Parse all errors reported by Predator on this line of output
    Return list, never return None
    """
    # we are not interested in traces
    if 'TRACE' in line:
        return []

    error_classes = [
        (': error: dereference of already deleted heap object', ['invalid']),
        (': error: dereferencing object of size [0-9]*B out of bounds', ['invalid']),
        (': error: dereference of NULL value', ['invalid']),
        (': error: invalid dereference', ['invalid']),
        (': error: dereference of non-existing non-heap object', ['invalid']),
        (': error: not enough space to store value of a pointer', ['invalid']),
        (': error: invalid L-value', ['invalid']),

        (': error: (free|realloc)\(\) called with offset', ['free']),
        (': error: invalid free\(\)', ['free']),
        (': error: double free', ['free']),
        (': error: attempt to free a non-existing non-heap object', ['free']),
        (': error: attempt to free a non-heap object', ['free']),
        (': error: (free|realloc)\(\) called on non-pointer value', ['free']),

        (': (error|warning): memory leak detected', ['leak']),
    ]

    result = []
    for (regex, classes) in error_classes:
        try:
            match = re.search(regex, line)
        except:
            print(regex)
            print(line)
        if match != None:
            spl = line.split(':')
            if len(spl) == 4:
                row = spl[1]
                result += [ErrorReport(int(row), 'none', cl) for cl in classes]
            elif len(spl) == 5:
                [row, col] = spl[1:3]
                try:
                    result += [ErrorReport(int(row), int(col), cl) for cl in classes]
                except ValueError:
                    pass

    return set(result)

def line_reports_unsupported_feature(line):
    unsupported_feature_regexes = map(re.compile, [
        ': warning: ignoring call of undefined function: ',
        ': warning: conditional jump depends on uninitialized value',
        ': warning: possible .*flow of .* integer',
    ])

    return any(map(lambda x: x.search(line) != None, unsupported_feature_regexes))

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
            for key, mask in sel.select(timeout):
                fd = key.fileobj
                line = fd.readline().strip().decode('utf-8')
                line_errors = parse_errors(line)
                error_locations.update(line_errors)

                if line_reports_unsupported_feature(line):
                    log('Predator encountered an unsupported feature, so the result is unknown')
                    log('The report is based on this line:')
                    log(line)
                    proc.kill()
                    return ('unknown', set())
                elif len(line_errors) == 0 and ': error:' in line:
                    log('Predator encountered an unknown error, so the result is error')
                    log('That is based on this line:')
                    log(line)
                    proc.kill()
                    return ('error', set())

                if re.search('clEasyRun\(\) took', line):
                    log('Predator finished')
                    return ('ok', error_locations)

            if proc.poll() != None:
                break

            now = nstime()
            elapsed = now - start
            if elapsed > timeout:
                log('timeout, killing Predator')
                proc.kill()
                return ('timeout', set())

        log('Predator has not finished gracefully')
        return ('error', set())

def write_logfile(res, errs, outfile):
    with open(outfile, 'w') as f:
        f.write('{}\n'.format(res))
        for err in errs:
            f.write('{} {} {}\n'.format(err.row,err.col,err.ty))

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
