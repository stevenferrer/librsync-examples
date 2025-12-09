# librsync-examples

A port of librsync [client/server example](https://github.com/librsync/librsync/tree/master/examples/stream) in C++ using [Boost.Asio](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html).

## Requirements

Development:

- clang >= 17
- meson >= 1.3
- ninja >= 1.13

Libraries:

- librsync >=v2.3.4
- popt >= v1.19
- boost >= 1.89

## How to setup

Setup build directory.

```sh
meson setup build
```

Compile the server.

```sh
meson compile -C build server
```

Compile the client.

```sh
meson compile -C build client
```

## How to run

Run the server.

```sh
./build/server foo.txt
```

Run the client.

```sh
./build/client
```

A copy of `foo.txt` should now be created.
