#rirc

A minimalistic irc client written in C.

While still under development, it currently supports
many features which you would expect from a basic
irc client.

##Building:

Default build:
```
make clean rirc
```

Debug build
```
make clean debug
```

##Usage:
```
  rirc [-c server [OPTIONS]]

Help:
  -h, --help             Print this message

Options:
  -c, --connect=SERVER   Connect to SERVER
  -p, --port=PORT        Connect using PORT
  -j, --join=CHANNELS    Comma separated list of channels to join
  -n, --nicks=NICKS      Comma and/or space separated list of nicks to use
  -v, --version          Print rirc version and exit

Examples:
  rirc -c server.tld -j '#chan'
  rirc -c server.tld -p 1234 -j '#chan1,#chan2' -n 'nick, nick_, nick__'
```

Hotkeys:
```
  ^N : go to next channel
  ^P : go to previous channel
  ^L : clear channel
  ^X : close channel
  ^F : find channel
```

##More info:
[rcr.io/rirc.html](http://rcr.io/rirc.html)

##Screenshots:
![image](https://raw.github.com/robbinsr/rirc/master/rirc.png?raw=true)
