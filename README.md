# FLOOP

## What is it?

Floop is a pseudorandom number generator that outputs a stream of bytes to STDOUT.
Its main purpose is to be used as a program to do (hopefully) secure wipes of storage devices such as hard drives and flash media.

## Usage

Floop outputs *only to STDOUT*; if you want to write to a file, either pipe it to `dd`, or redirect STDOUT with `>`.

### Examples

Use 8 threads, with each thread generating 0x200000 elements of 8-byte words, and control it with `dd`.
```
./floop --threads 8 --thread-buf 0x200000 --count 0 | dd of=/some/file/or/device count=10 bs=128M
```

The `--thread-buf` flag controls how many 8-byte words a thread will generate per iteration.
So, if `--thread-buf` is set to 2, each thread defined by `--threads` will generate 2 8-byte words, or 16 bytes, per iteration.
The iteration is controlled with `--count`, which when set to 0 will make floop run forever; this is why we pipe it to `dd` above.

Using `dd` is also useful because it gives you information on how fast it was able to write the bytes to the destination.

#### Actual Use Case

The following was on a Western Digital Passport Ultra 1TB Portable External USB 3.0 Hard Drive (WDBZFP0010BBL-NESN).
```
 $ floop -t 8 -b 0x200000 -c 0 | pv -bartpes 1000170586112 | dd bs=128M of=/dev/sde
dd: error writing ‘/dev/sde’: No space left on device==========================================================> ] 99% ETA 0:00:00
 931GiB 3:28:57 [76.1MiB/s] [76.1MiB/s] [======================================================================>] 100%
0+9287529 records in
0+9287528 records out
1000170586112 bytes (1.0 TB) copied, 12592.6 s, 79.4 MB/s
```

The speed is not impressive, because of the slow speed of the drive itself.
In comparison, using openssl (using AES encryption to encrypt a stream of 0s and using that data as output) with `openssl enc -aes-256-ctr -pass pass:"$(dd if=/dev/random bs=128 count=1 2>/dev/null | base64)" -nosalt </dev/zero` was more than 2x slower than floop on this drive.

## Technical Notes

Floop works by creating worker threads, and one big master buffer.
Each worker writes to a desginated subsection (which is `--thread-buf` elements big) within the master buffer.
When all workers have finished writing to their subsection, the master thread executes a single `fwrite(3)` call of the entire master buffer to STDOUT.
Before the start of the next iteration, the main thread randomly reassigns to each worker a new subsection of the master buffer.

### The PRNG Algorithm

Floop uses a version of George Marsaglia's `xorshift` PRNG[^xorshift], written by Sebastiano Vigna, which was released in his paper "An experimental exploration of Marsaglia's xorshift generators, scrambled" (2014).
This algorithm has excellent randomness properties (as described in the paper), and also has a large period of 2^1024 -1.
It uses an internal state of 128 bytes (1024 bits), and Floop uses `/dev/random` to initialize the state.
All worker threads have its own PRNG state (for the generation of random bytes to the master buffer), and the main thread also has its own PRNG state (for randomizing the master thread segments amongst the worker threads).

### Performance

The `--threads` and `--thread-buf` flags are important because these parameters control how fast Floop can write bytes to STDOUT.
The user must experiment with trial and error to determine the optimum settings for their platform/hardware, but generally it should be faster to have larger buffers, as this will reduce the number of `fwrite(3)` calls.
However, do note that larger buffer sizes mean less security, as this will mean larger stretches of bits written by a single worker thread.

## Note on Security

Floop was designed to generate random bytes *quickly*.
If you are paranoid about security, you should really only use `/dev/random` as a source of random bytes.
However, note that Floop's purpose is to only prepare a drive to be used as an encrypted medium --- its sole purpose is to hide which blocks have actual data.
The more you use the encrypted medium with a strong cipher like AES, the stronger Floop's impact will become.
This is because more and more of Floop's bytes on the disk will get replaced by cryptographically secure bytes of your encryption mechanism.

[^xorshift]: http://xorshift.di.unimi.it/
