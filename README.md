<p align="center">
  <img src="https://raw.githubusercontent.com/rcr/rirc/master/docs/birb.jpg" alt="birb"/>
</p>
---
<p align="center">
  <a href="https://sonarcloud.io/dashboard?id=rcr_rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rcr_rirc&metric=ncloc"/>
  </a>
  <a href="https://scan.coverity.com/projects/4940">
    <img alt="coverity" src="https://scan.coverity.com/projects/4940/badge.svg"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rcr_rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rcr_rirc&metric=sqale_rating"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rcr_rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rcr_rirc&metric=security_rating"/>
  </a>
</p>
---

# rirc
A minimalistic irc client written in C.

While still under development, it currently supports
many features which you would expect from a basic
irc client.

## Building:

```
make
```
Or
```
make debug
```

```
make install
```

## Usage:
```
  rirc [-hv] [-s server [-p port] [-w pass] [-u user] [-r real] [-n nicks] [-c chans]], ...]

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
   ← : input cursor back
   → : input cursor forward
   ↑ : input history back
   ↓ : input history forward
```

## More info:
[rcr.io/rirc/](http://rcr.io/rirc/)
