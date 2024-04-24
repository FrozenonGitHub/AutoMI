# AutoMI

This repository contains implementation of AutoMI, a framework for automatically converting vertex-centric graph algorithms into their multi-instance versions. Currently AutoMI suports GAS programs that run on popular distributed GAS systems, e.g., PowerGraph and PowerLyra. To evaluate the efficiency of AutoMI generated multi-instance GAS programs and the effectiveness of TrackFree optimization, we tested Boolean BFS, Integer BFS, SSSP, SpMV, PPR, GC and CF (SGD) over real-life and synthetic graphs. Further details can be found in our paper.

## Datasets
* We use the following data graphs in the experiment.
  * LiveJournal [link](http://snap.stanford.edu/data/soc-LiveJournal1.html)
  * Twitter [link](http://konect.cc/networks/twitter/)
  * Friendster [link]( http://konect.cc/networks/friendster/)
  * UKDomain [link](http://konect.cc/networks/dimacs10-uk-2007-05/)
  * MovieLens [link](http://grouplens.org/datasets/movielens/)
  * NetFlix [link](http://konect.cc/networks/netflix/)

## AutoMI auto-conversion
* Implementation of AutoMI auto-converter can be found under `utils/` directory.

* In order to automatically convert single-instance GAS program to single-instance multi-instance version, use the following command (use "--track-free" to enable TrackFree optimization, optional):
```
$ python converter.py --track-free <single-instance-filename> <multi-instance-filename>

# For example:
$ python converter.py example_single_SSSP.cpp track_sssp.cpp
# generates multi-instance SSSP (without TrackFree),

$ python converter.py example_single_SSSP.cpp trackfree_sssp.cpp
# generates multi-instance SSSP with TrackFree enabled.
```

## Experiments

### 1. AWS EC2 Environment Setup
* You should have Amazon EC2 cluster with password-less SSH setup between machines

* On your local machine, provide private IP of EC2 instances in `machines` file one by one in each line. Similarly for public IP in `PublicIP` file.

* On your local machine, run `script/aws/AUTOMI_MOUNT` script to create `/AutoMI` directory and mount EBS volume at `/AutoMI` on each EC2 instance.

### 2. Building
* Synchronize current directory on your local machine with the `/AutoMI` directory on EC2 master machine.

* On EC2 master machine, build AutoMI as follows:
```
$ cd /AutoMI
$ ./configure
$ cd /AutoMI/release/toolkits/graph_automi
$ make
```

### 3. Testing
* On EC2 master machine, run `/AutoMI/scripts/aws/AUTOMI_INIT` script to prepare EC2 cluster

* See `/AutoMI/script/aws/bench_automi.sh` for an example to run tests for SSSP over LiveJournal graph
