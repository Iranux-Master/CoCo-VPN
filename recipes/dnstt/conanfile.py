from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.env import Environment
from conan.tools.files import copy, download, unzip
from conan.tools.layout import basic_layout

import os


class Dnstt(ConanFile):
    """dnstt-client (David Fifield, CC0) for the CoCo DNS tunnel (docs/plans/D1-dns-tunnel.md).

    The same release as the webpack.ir catalog tool "dnstt" and the server container
    (client/server_scripts/dnstt/Dockerfile). The module zip comes from the Go module proxy and is pinned by its
    SHA-256; its own go.sum pins the dependencies, which the Go checksum database verifies while building.
    """

    name = "dnstt"
    version = "1.20260501.0"
    package_type = "application"
    settings = "os", "arch"

    _module = "www.bamsoftware.com/git/dnstt.git"
    _zip_sha256 = "e19ef0719ec5de95b5fc38f6ea36646a46e815e22c78b56f25a8dbb399866ee5"
    _binary_name = "dnstt-client"

    _arch_map = {
        "x86": "386",
        "x86_64": "amd64",
        "armv8": "arm64",
    }

    @property
    def _goos(self):
        return {
            "Linux": "linux",
            "Macos": "darwin",
            "Windows": "windows",
        }.get(str(self.settings.os))

    @property
    def _binary_name_ext(self):
        return f"{self._binary_name}.exe" if str(self.settings.os) == "Windows" else self._binary_name

    @property
    def _module_dir(self):
        return os.path.join(self.source_folder, *self._module.split("/")[:-1], f"dnstt.git@v{self.version}")

    def layout(self):
        basic_layout(self)

    def validate(self):
        if not self._goos or str(self.settings.arch) not in self._arch_map:
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not support {self.settings.os} {self.settings.arch}"
            )

    def build_requirements(self):
        self.tool_requires("go/1.26.0")

    def source(self):
        archive = os.path.join(self.source_folder, "dnstt.zip")
        download(self, f"https://proxy.golang.org/{self._module}/@v/v{self.version}.zip", archive,
                 sha256=self._zip_sha256)
        unzip(self, archive, destination=self.source_folder)
        os.remove(archive)

    def build(self):
        env = Environment()
        env.define("GOPATH", os.path.join(self.build_folder, "gopath"))
        env.define("GOMODCACHE", os.path.join(self.build_folder, "gopath", "pkg", "mod"))
        env.define("GOCACHE", os.path.join(self.build_folder, "gocache"))
        env.define("GOTELEMETRY", "off")
        env.define("GOFLAGS", "-mod=readonly")
        env.define("CGO_ENABLED", "0")
        env.define("GOOS", self._goos)
        env.define("GOARCH", self._arch_map[str(self.settings.arch)])
        output = os.path.join(self.build_folder, self._binary_name_ext)
        with env.vars(self).apply():
            self.run(f'go build -trimpath -ldflags="-s -w" -o "{output}" ./dnstt-client', cwd=self._module_dir)

    def package(self):
        copy(self, self._binary_name_ext, src=self.build_folder, dst=self.package_folder)

    def package_info(self):
        self.cpp_info.exe = True
        self.cpp_info.location = os.path.join(self.package_folder, self._binary_name_ext)
        self.cpp_info.set_property("cmake_target_name", "bamsoftware::dnstt-client")
