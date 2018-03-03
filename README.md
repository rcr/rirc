# rirc
[![Coverity Scan Build](https://scan.coverity.com/projects/4940/badge.svg)](https://scan.coverity.com/projects/4940)

A minimalistic irc client written in C.

While still under development, it currently supports
many features which you would expect from a basic
irc client.

## Building:

Default build:
```
make rirc
```

Debug build
```
make debug
```

## Usage:
```
  rirc [-vh] [-c server [OPTIONS]]

Help:
  -h, --help             Print this message

Options:
  -c, --connect=SERVER   Connect to SERVER
  -p, --port=PORT        Connect using PORT
  -j, --join=CHANNELS    Comma separated list of channels to join
  -n, --nicks=NICKS      Comma and/or space separated list of nicks to use
  -v, --version          Print rirc version and exit

Examples:
  rirc -c server -j '#chan'
  rirc -c server -j '#chan' -c server2 -j '#chan2'
  rirc -c server -p 1234 -j '#chan1,#chan2' -n 'nick, nick_, nick__'
```

Hotkeys:
```
  ^N : go to next channel
  ^P : go to previous channel
  ^L : clear channel
  ^X : close channel
  ^F : find channel
  ^C : cancel input/action
  ^U : scroll buffer up
  ^D : scroll buffer down
```

## More info:
[rcr.io/rirc/](http://rcr.io/rirc/)
