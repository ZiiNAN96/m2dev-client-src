# cgltf

Upstream: https://github.com/jkuhlmann/cgltf

Version: v1.15, commit `360db1a95480fe102ae9c69b27c5d101167ff5ba`.

License: MIT (see LICENSE). `cgltf.h` and `cgltf_write.h` are unmodified upstream sources.

Private dependency of AssetRuntime/GlTF only. The provider uses cgltf for GLB/JSON
structure parsing, then bounded, alignment-safe accessor conversion owned by the
adapter. There are no external file callbacks, image decoders, compression
decoders, or animation engines in this dependency.

E2-X adds the matching `cgltf_write.h` from the same pinned commit as a private
dependency of the separate offline AssetToolCore. It serializes the neutral tool
model as glTF JSON; a bounded 28-byte GLB envelope permits native Unicode file I/O.
No writer implementation is linked into the runtime client.
Writer SHA256: `fb91760451d5c63bea1c0f606ddecb5cf8c17bfe02bf0f23ee4f635af5c47996`.
