# assets

`assets` will read files into plain data in memory: images, 3D models, sounds,
and fonts. It knows nothing about the GPU or the sound device, so any program can
use it to read files, and `window`, `graphics`, and `sound` use it when you give
them a file name.

> [!NOTE] Not available yet
> `assets` is step 2 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- Read PNG, baseline JPEG, BMP, TGA, and QOI images, and later progressive JPEG,
  HDR, DDS, and KTX2.
- Read OBJ models with their materials, and later glTF 2.0 and binary FBX.
- Read WAV and QOA sounds, and later Ogg Vorbis and MP3.
- Read TrueType fonts.
- Load in the background, and read from folders, from pack files, from an
  Android package, or from files preloaded by a web page.

Every decoder is written from scratch, including the inflate compression that
PNG and FBX share.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#assets).
