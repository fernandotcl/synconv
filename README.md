## Introduction

synconv is a command line based audio format transcoder with an rsync-like behavior. It's specially useful for synchronizing a music collection with portable devices that either don't support some audio formats used in your collection or that could benefit from smaller file sizes obtained with the usage of a more lossy audio format.

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

##Usage

Use it as you would use rsync. By default, synconv will transcode to MP3 and will use multiple threads for transcoding. Existing MP3 files will not be re-encoded:

```sh
synconv /music/Artist /media/phone/
```

See `synconv(1)` for more examples.

## Credits

synconv was created by [Fernando Tarlá Cardoso Lemos](mailto:fernandotcl@gmail.com).
