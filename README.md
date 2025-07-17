# Bloomfilter Unique (buniq)

by Ron Dilley <ron.dilley@uberadmin.com>

You can find the latest information on wirespy [here](http://www.uberadmin.com/Projects/buniq/ "Bloomfilter Unique")

## What is WireSpy (wsd)?

Wirespy is a simple network sniffer for information security that extracts
interesting metadata about network traffic and logs it.  That sounds like
a million other security and network tools, and it is in many ways though
there are some very important differences.

## Why use it?

Wirespy is not a replacement for tcpdump, wireshark or any of the other
network sniffers.  It has a specific purpose in providing long term
metadata about network traffic including TCP flow logging.  It is efficent
and can monitoring live network traffic or process PCAP files.

I use it on my network recorders to extract metadata from the PCAP files
that takes up less space, further extended the number of months of network
intelligence I can save before running out of disk space.

The TCP flow capability is tollerant of lost packets which are common
when passively monitoring network traffic.

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
  buniq input.txt                    # Remove duplicates from file
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
