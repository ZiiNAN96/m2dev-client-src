# cgltf

Upstream: https://github.com/jkuhlmann/cgltf

Version: v1.15, commit `360db1a95480fe102ae9c69b27c5d101167ff5ba`.

License: MIT (see LICENSE). `cgltf.h` is unmodified upstream source.

Private dependency of AssetRuntime/GlTF only. The provider uses cgltf for GLB/JSON
structure parsing, then bounded, alignment-safe accessor conversion owned by the
adapter. There are no external file callbacks, image decoders, compression
decoders, or animation engines in this dependency.
