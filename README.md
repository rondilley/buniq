# Bloomfilter Unique (buniq)

by Ron Dilley <ron.dilley@uberadmin.com>

You can find the latest information on wirespy [here](http://www.uberadmin.com/Projects/buniq/ "Bloomfilter Unique")

## What is Bloomfilter Unique (buniq)?

Bloomfilter Unique is a simple tool for removing duplicate lines from a
text file.  By using bloom filters, the process of removing duplicates
benifits from their space-efficient probabilistic data structure.  In
short, it's a fast and efficient way to remove duplicates from very large
files.

## Why use it?

I built this tool to address a problem I was having removing
duplicate password from very large dictionaries that just keep
growing and growing based on new published datasets along with
more and more cracked hashes.

My base disctionaries are over 5gb now and removing duplicates
was getting closer and closer to the 1h mark to complete.  Using
sort and uniq with large source files uses lots of IO and memory.

| size | filename |
| ---- | -------- |
| 1.1G | guessed.lst |
| 3.3G | master.lst |
| 1.0G | mega.lst |

The sad reality is that sort | uniq takes almost 40 minutes to
process and remove the duplicates.

```sh
time cat guessed.lst master.lst mega.lst | sort | uniq > test.lst

real	37m50.684s
user	46m40.996s
sys	0m44.241s
```

Bloom filters are space-efficient and the error rate is configurable.
Misses are always accurate, hit rate accuracy is based on the selected
error rate value.  I don't need the unique process to be perfect for
passwords, and the difference in performance is striking.  Same files,
buniq completed the process in 3 1/2 minutes instead of 40.

```sh
time cat guessed.lst master.lst mega.lst | buniq - > test1.lst

real	3m26.142s
user	3m16.268s
sys	0m14.993s
```

I am sure there all kinds of other placeses where buniq can help as
a replacement for "sort | uniq".

## Implementation

buniq comes with a minimal set of options as follows:

```sh
% ./buniq -h
buniq v0.4 [Jul 17 2025 - 00:23:16]

syntax: buniq [options] [file]

Reads from stdin if no file is specified.
Uses dynamic bloom filters for large files (>100MB) or stdin.

Basic Options:
 -d|--debug (0-9)     enable debugging info
 -e|--error (rate)    error rate [default: 0.01]
 -h|--help            this info
 -v|--version         display version information

Advanced Options:
 -j|--threads (N)     use N threads for parallel processing
 -c|--count           count duplicate occurrences
 -s|--stats           show processing statistics
 -p|--progress        show progress bar
 -D|--duplicates      show duplicate lines instead of unique
 -f|--format (type)   output format: text, json, csv, tsv
 -b|--bloom-type (t)  bloom filter type: regular, scaling
 -S|--save-bloom (f)  save bloom filter to file
 -L|--load-bloom (f)  load bloom filter from file
 -a|--adaptive        use adaptive bloom filter sizing

Examples:
  buniq input.txt                   # Remove duplicates from file
  cat file | buniq                  # Remove duplicates from stdin
  buniq -e 0.001 large.txt          # Use lower error rate for better accuracy
  buniq -j 4 -s large.txt           # Use 4 threads and show statistics
  buniq -c -f json data.txt         # Count duplicates and output as JSON
  buniq -D -p huge.txt              # Show duplicates with progress bar
  buniq -b scaling -a input.txt     # Use adaptive scaling bloom filter
```

## Security Implications

Assume that there are errors in the wsd source that
would allow a specially crafted packet to allow an attacker
to exploit wsd to gain access to the computer that wsd is
running on!!!  wsd tries to get rid of priviledges it does
not need and can run in a chroot environment.  I recommend
that you use the chroot and uid/gid options.  They are there
to compensate for my poor programming skills.  Don't trust
this software and install and use is at your own risk.

## Bugs

I am not a programmer by any stretch of the imagination.  I
have attempted to remove the obvious bugs and other
programmer related errors but please keep in mind the first
sentence.  If you find an issue with code, please send me
an e-mail with details and I will be happy to look into
it.

Ron Dilley
ron.dilley@uberadmin.com
