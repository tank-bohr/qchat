# Third-party components

QChat's own source is MIT licensed. Dependencies retain their own licenses.

- Apache Thrift 0.25.0: Apache-2.0. Compiler and runtime must have matching versions.
- Qt Base and Qt Declarative: the versions selected by the pinned vcpkg baseline.
  See each module's license and Qt's [open-source obligations](https://www.qt.io/licensing/open-source-lgpl-obligations).
- Boost and Qt's transitive dependencies: see `vcpkg_installed/<triplet>/share/*/copyright`.
  Desktop builds publish these files in a separate notices archive.

Static distributions include a companion `qchat-qt-sources.tar.gz`: the corresponding
Qt source archives, verified against vcpkg's SHA-512 metadata, and the pinned vcpkg
recipes containing all applied patches. `qchat-source.tar.gz` contains the application
source. These accompany the notices archives on releases and in the Linux desktop
CI artifact. Together with the README build instructions they allow rebuilding and
relinking the application with a modified Qt. Running modified builds and reverse
engineering for debugging those modifications are permitted; there is no signing
or installation lock.

Redistribute these companion materials with the executable, and retain the applicable
license notices. Changing dependencies or build recipes also requires updating the
corresponding source package. See Qt's linked license guidance for the full terms.
