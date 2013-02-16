## Introduction

synconv is a command line based audio format transcoder with an rsync-like behavior. It's specially useful for synchronizing a music collection with portable devices that either don't support some audio formats used in your collection or that could benefit from smaller file sizes obtained with the usage of a more lossy audio format.

![synconv screenshot](https://raw.github.com/fernandotcl/synconv/master/screenshot.png)

synconv doesn't do the transcoding itself. Instead, it uses command line decoders and encoders (such as [lame][]) to transcode the files. It creates transcoding pipelines that are run in parallel, optimized for multicore machines. Those pipelines are created in a way that should be familiar to users of synchronization software such as [rsync][]. By default, if the transcoded (or copied) file is already up-to-date, it isn't overwritten, making it easier to keep files synchronized across devices.

[lame]: http://lame.sourceforge.net/
[rsync]: http://www.samba.org/ftp/rsync/rsync.html

A brief list of features:

* Support for FLAC, Ogg Vorbis and MP3 codecs
* Parallel transcoding, optimized for multicore CPUs
* Audio tags are copied from the original file to the transcoded file
* Support for re-encoding files and skipping files that can't be transcoded
* Timestamps are checked to ensure that only modified files are replaced
* Renaming filters make working with filesystems such as FAT32 easier

## Usage

Use it as you would use rsync. By default, synconv will transcode to MP3 and will use multiple threads for transcoding. Existing MP3 files will not be re-encoded:

```sh
synconv /music/Artist /media/phone/
```

See `synconv(1)` for more examples.

## Installing

The easiest way to install synconv is through [Homebrew][]. There is a formula for synconv in [my Homebrew tap][tap].

[homebrew]: http://mxcl.github.com/homebrew/
[tap]: https://github.com/fernandotcl/homebrew-fernandotcl

If you're compiling from source, you will need:

* [TagLib][]
* [pkg-config][]
* [CMake][]

[taglib]: http://taglib.github.com/
[pkg-config]: http://www.freedesktop.org/wiki/Software/pkg-config
[cmake]: http://www.cmake.org/

You probably want the encoders and decoders too. We currently support:

* [flac] (for FLAC encoding and decoding)
* [lame] (for MP3 encoding and decoding)
* oggenc (for Ogg Vorbis encoding, part of [vorbis-tools])
* oggdec (for Ogg Vorbis decoding, part of [vorbis-tools])

[flac]: http://flac.sourceforge.net/
[lame]: http://lame.sourceforge.net/
[vorbis-tools]: http://www.vorbis.com/

You don't need all of them, just the ones you will use.

To compile and install:

```sh
cd /path/to/source
cmake .
make install
```

## Credits

synconv was created by [Fernando Tarlá Cardoso Lemos](mailto:fernandotcl@gmail.com).

## License

synconv is available under the BSD 2-clause license. See the LICENSE file for more information.
