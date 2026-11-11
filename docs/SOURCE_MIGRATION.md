# Initial payload migration

Only Project Gracemeria's distribution files were copied from its existing local
checkout: the runtime manifest, licence, artwork, and exported xenon-analysis
preparation data. The original working tree was left unchanged. Source projects,
tests, build folders, tools, legacy analysis inputs and runtime debug traces
were excluded. The stable module ID is unchanged.

There is currently no prebuilt DLL declared by this package. Its exported hook
routines remain necessary for the existing local preparation flow. Once a
tested prebuilt DLL is supplied, publish that DLL and its nativeExtension
manifest declaration rather than the hook source needed to build it.
