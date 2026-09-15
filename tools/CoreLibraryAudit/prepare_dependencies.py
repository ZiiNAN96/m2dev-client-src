"""Fetch only the pinned audit dependencies into ignored build/clibx/deps."""
import hashlib
import pathlib
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEPS = ROOT / "build/clibx/deps"
SOURCES = (
    ("ozz", "guillaumeblanc/ozz-animation", "refs/tags/0.17.0", "22b4401c77a2be68acb0a8aa2e857f94dbaccfed3ab25e304a1ddea4c546e86b"),
    ("acl", "nfrechette/acl", "refs/tags/v2.1.0", "0ac8473cd30eb768bae1ef58558e3088242d6fef81f727ce7b5ff5af9be74fce"),
    ("rtm", "nfrechette/rtm", "d7982f2b2524feeb322f424b29cf43df30b5d5b7", "c284ac260732e992c6fba7a755d6f62d3f5fc2e33ee2efa97b14091e6d39a3b0"),
    ("basisu", "BinomialLLC/basis_universal", "refs/tags/v2_50", "216e49e1f4213d4bfa4afaa07527e16bac28533dddd444197d3aa19230ac130c"),
    ("meshoptimizer", "zeux/meshoptimizer", "refs/tags/v0.25", "68b2fef4e4eaad98e00c657c1e7f8982a7176e61dd7efdeaec67a025b8519be9"),
)

if __name__ == "__main__":
    DEPS.mkdir(parents=True, exist_ok=True)
    for name, repo, revision, digest in SOURCES:
        archive = DEPS / (name + ".tar.gz")
        if not archive.exists():
            urllib.request.urlretrieve(f"https://codeload.github.com/{repo}/tar.gz/{revision}", archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"SHA256 mismatch: {archive}")
        with tarfile.open(archive) as source:
            source.extractall(DEPS, filter="data")
        print(name, digest)
