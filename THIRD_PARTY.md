The application dynamically links Qt 6.8.3 (Core, Gui, Qml, Quick, Quick Controls and their dependencies). Qt is available under commercial and open-source licenses, with terms varying by module. Review and fulfill the applicable terms before distributing binaries. License texts and component notices are available in the matching source packages at https://download.qt.io/archive/qt/6.8/6.8.3/ . The SDK also provides component metadata under `.tools/qt/6.8.3/mingw_64/sbom`.

The project-local compiler is Qt's MinGW-w64 GCC 13.1 toolchain. Its runtime libraries and notices must be accounted for when distributing a build. Development tooling (aqtinstall, CMake, Ninja and psutil) is installed only in `.tools/python`; it is not embedded in application source or committed to the repository.

This repository does not yet declare a license for the original application code. The owner should choose one before public distribution. This does not change the licenses of third-party components.

The FFT, PCM decoder and WAV parser in `src/dsp` are implemented in this repository and have no external runtime DSP dependency. Loudness standards validation and selection of additional decoders remain future milestones.
