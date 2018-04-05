box11
=====

## Synopsis

`box11` is the next generation in box-drawing technology. It provides a simple
way to display text anywhere on the screen.

Run `box11 --help` for usage information.

## Usage

`box11` displays standard input. For example:

```sh
while true; do 
    date
    sleep 1
done | box11 -u hv
```

This will write the date in a grey box in the upper-left corner of the screen,
updating it once per second. The `-u hv` argument will cause the box to
automatically resize to fit the horizontal and vertical bounds of the text.

## Build

### Dependencies

`xcb`, `xcb-xrm`, `cairo`, and `pango` are required to build `box11`. Consult
your distribution's repositories for package names.

### Steps

To build, run:

* `mkdir build`
* `make`

This will place the `box11` executable in the `build` directory. 

To install it system-wide, simply `make install`. Running that command with the
`INSTALL_PATH` environment variable set will override the directory to which
the `box11` executable is installed.

## Future goals

This project could be expanded in a number of ways:

* Manpage
* Image rendering support
* User input (probably better suited to another project like `dmenu`)
