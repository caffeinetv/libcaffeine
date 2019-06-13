Contributing to libcaffeine
===========================

Here are some ways you can help out.

## Participate in the community

Join the [Caffeine.tv Discord](https://discordapp.com/invite/caffeine) server and ping @Cogwheel#4242 for access to the `#development` channel

## Bug reports

Please open issues on Github for any problems you come across

## Documentation

* [README.md](README.md)
    * Clearer build instructions
    * Troubleshooting tips

## Source code

We have a lot of TODOs to make libcaffeine production ready. See the Github issues for some of the things we're looking for.

Follow the README to get libcaffeine building. For testing, check out our fork of [OBS Studio](https://github.com/caffeinetv/obs-studio).

Contributions to libcaffeine are subject to a Contributor License Agreement. You will be asked to sign this when you submit a pull request.

### Code style

`.clang-format` is the source of truth. Ignore anything here that conflicts.

Rule of thumb: mimic the style of whatever is around what you're changing. Here are some decided-on details:

#### Text formatting

* 120-column text width
* 4-space indentation
* Always use braces for compound statements (`if`, `for`, `switch`, etc)
* Spaces on both sides of `&` and `*` in declarations. `char * foo;`
* East const (`char const * const` instead of `const char * const`)

#### Naming

* Types, enum values, and file names: `PascalCase`
* Macros (to be avoided): `SCREAMING_SNAKE_CASE`
* Other names: `camelCase`

Names should be unadorned (no hungarian notation, no distincion between global, local, and member variables, etc.).

Acronyms, abbreviations, initialisms, etc should be treated like words, e.g. `IceUrl iceUrl;`

All names should be declared in the `caff` namespace, except when extending 3rd-party namespaces.

In C, all names should be prefixed with `caff_`, and otherwise follow the same conventions.

In C++, prefer `enum class`es. In C, include the enum type in the name of the enum value, e.g. `caff_ErrorNotSignedIn`

C++ filenames use `.hpp` and `.cpp`. C filenames use `.h` and `.c`.
