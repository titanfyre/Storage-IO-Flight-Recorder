# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | storage-io-flight-recorder |
| CPP/conan | published_custom_source_index | [storage-io-flight-recorder](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | account_email_confirmed_requires_matching_trusted_publishing_policy | Storage-IO-Flight-Recorder.Native |
| NET/nuget | built_verified_requires_trusted_publishing_policy | StorageIOFlightRecorder.SDK [Source](Storage-IO-Flight-Recorder-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | storage-io-flight-recorder [Source](Storage-IO-Flight-Recorder-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [storage-io-flight-recorder](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [storage-io-flight-recorder](https://crates.io/crates/storage-io-flight-recorder) [Source](Storage-IO-Flight-Recorder-Rust-cargo/) |
| JavaScript/npm | registry_publication_requires_action | [storage-io-flight-recorder](https://www.npmjs.com/package/storage-io-flight-recorder) [Source](Storage-IO-Flight-Recorder-JavaScript-npm/) |
| Java/maven | published_install_verified | [storage-io-flight-recorder](https://central.sonatype.com/artifact/io.github.titancsa/storage-io-flight-recorder/1.0.0) [Source](Storage-IO-Flight-Recorder-Java-maven/) |
| Java/jitpack | published_install_verified | [storage-io-flight-recorder](https://jitpack.io/#titanfyre/Storage-IO-Flight-Recorder/java-sdk-v1.0.0) [Source](Storage-IO-Flight-Recorder-Java-maven/) |
| Go/modules | published_install_verified | [storage-io-flight-recorder](https://pkg.go.dev/github.com/titanfyre/Storage-IO-Flight-Recorder/Storage-IO-Flight-Recorder-Go-modules@v1.0.0) [Source](Storage-IO-Flight-Recorder-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Storage-IO-Flight-Recorder/Storage-IO-Flight-Recorder-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [StorageIOFlightRecorder](https://www.powershellgallery.com/packages/StorageIOFlightRecorder/1.0.0) [Source](Storage-IO-Flight-Recorder-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | storage_io_flight_recorder [Source](Storage-IO-Flight-Recorder-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [storage-io-flight-recorder](https://rubygems.org/gems/storage-io-flight-recorder) [Source](Storage-IO-Flight-Recorder-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/storage-io-flight-recorder](https://packagist.org/packages/titancsa/storage-io-flight-recorder) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
