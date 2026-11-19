# Nox native production build

Use the regular CE Release build. See the [Nox maintenance and player
guide](../../doc/ce-nox/README.md) for the build command, engine ownership,
original HDV requirements and runtime validation. The former native-only MSBuild
override was used by historical candidates. Current projects get the native
source list directly from `engines/nox/module.mk`.
