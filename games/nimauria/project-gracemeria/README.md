# Project Gracemeria

Ace Combat 6 module payload for Xenon. The module ID remains
org.nimauria.project-gracemeria. Users supply their own compatible game content.

This folder contains distribution metadata, artwork, licence and the runtime
preparation data currently required by Xenon. Gracemeria presently prepares its
native DLL locally using xenon-analysis/ and its exported hook routines. No
prebuilt game DLL is included or claimed validated by this migration.

A prebuilt module distribution can replace the preparation data with its DLL
and a matching nativeExtension manifest declaration after revision/ABI testing.
Hooks compiled into that DLL do not need separate source files at runtime.
Developer source, tests, build projects and tools are not part of this folder.
