# rirc
[![Coverity Scan Build](https://scan.coverity.com/projects/4940/badge.svg)](https://scan.coverity.com/projects/4940)

![rirc](docs/birb.jpg?raw=true "rirc")

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
  rirc [-hv] [-s server [-p port] [-w pass] [-n nicks] [-c chans] [-u user] [-r real]], ...]

Help:
  -h, --help            Print this message and exit
  -v, --version         Print rirc version and exit

Options:
  -s, --server=SERVER      Connect to SERVER
  -p, --port=PORT          Connect to SERVER using PORT
  -w, --pass=PASS          Connect to SERVER using PASS
  -u, --username=USERNAME  Connect to SERVER using USERNAME
  -r, --realname=REALNAME  Connect to SERVER using REALNAME
  -n, --nicks=NICKS        Comma separated list of nicks to use for SERVER
  -c, --chans=CHANNELS     Comma separated list of channels to join for SERVER
```

Commands:
```
  :quit
  :clear
  :close
  :connect [host [port] [pass] [user] [real]]
```

Keys:
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
