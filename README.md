# ACE-codebase

We propose a new Asymmetry & Concurrencyaware bufferpool management (ACE) that batches writes based on device concurrency and performs them in parallel to amortize the asymmetric write cost. In addition, ACE performs parallel prefetching to exploit the device’s read concurrency. ACE does not modify the existing bufferpool replacement policy, rather, it is a wrapper that can be integrated with any replacement policy. More details can be found in our [paper](https://ieeexplore.ieee.org/document/10184709)

The Simulation folder contains a simple development of ACE in C++.

###How to run

Just clone the repo and run `make` and then `script.sh`. Note that, this is just one script. We used many such scripts for different experiments. Also note, there must be a large file named testfile in the directory. To create a large file, create a file named 'testfile' with some contents. Then run:
```
for i in {1..25}; do cat testfile testfile > file2 && mv file2 testfile; done
```
You can tune the loop counter to control the size of the file.

Let's consider an example run:
```
./buffermanager -b 5 -f testfile3 -x 20 -e 50 -a 3 -s 100 -d 100 -r 1 -w 8 -k 3 -v 2
```
### Understanding the input

buffer size in pages: b

file that mimics DB: f

number of operations: x

percentage of read: e

ACE variant: a (a=1 is the baseline, a=5 is the final ACE counterpart)

skewness parameter: s (s% of the accesses will focus on d% of the data)

skewness parameter: d (s% of the accesses will focus on d% of the data)

read cost: r

write cost: w (w=a*r)

concurrency: k (only makes sense for a>1, not the baseline)

verbosity: v

The Postgres folder contains a Postgres deployment of ACE under its default page replacement algorithm clock sweep. Run `make` and then run `script.sh` for an example run. More details coming soon.

