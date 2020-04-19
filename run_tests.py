#!/usr/bin/env python
import subprocess
import shlex
import yaml
import matplotlib.pyplot as plt
import os
import numpy

def run(frame_count, algorithm, file_name):
    command = "./vmsim_prog -n %d -a %s %s"%(
        frame_count,
        algorithm,
        file_name
    )
    print("Attemting: %s"%command)
    out = subprocess.check_output(shlex.split(command))
    print(out)
    return yaml.load(out)

def e(filename):
    algorithms = ["opt", "lru", "second"]
    pages = [8, 16, 32, 64]

    results = {}
    for algorithm in algorithms:
        aresult = []
        for page in pages:
            presult = run(page, algorithm, filename)
            aresult.append(presult["Total page faults"])
        results[algorithm] = (pages, aresult)
    
    return results

def p1():
    files = ["test/gcc.trace", "test/gzip.trace", "test/swim.trace"]

    for f in files:
        r = e(f)
        plt.plot(r["opt"][0], r["opt"][1])
        plt.plot(r["lru"][0], r["lru"][1])
        plt.plot(r["second"][0], r["second"][1])
        plt.legend(["OPT", "LRU", "Second"], loc="upper left")
        plt.title(os.path.split(f)[1])
        plt.xlabel("Number of Frames")
        plt.ylabel("Page Faults")
        plt.savefig("%s.png"%f)
        plt.clf()

def p2():
    files = ["test/gcc.trace", "test/gzip.trace", "test/swim.trace"]

    results = {}
    for f in files:
        aresult = []
        lresul = []
        for page in range(2, 101):
            presult = run(page, "second", f)
            aresult.append(presult["Total page faults"])
            lresul.append(page)
        results[f] = (lresul, aresult)
        plt.plot(lresul[1:], numpy.diff(aresult))
        plt.title(os.path.split(f)[1])
        plt.xlabel("Number of Frames")
        plt.ylabel("Page Faults")
        plt.savefig("%s_opt.png"%f)
        plt.clf()

p1()
