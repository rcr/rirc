<p align="center">
  <img src="https://raw.githubusercontent.com/rcr/rirc/master/docs/birb.jpg" alt="birb"/>
</p>

---

<p align="center">
  <a href="https://sonarcloud.io/dashboard?id=rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rirc&metric=ncloc"/>
  </a>
  <a href="https://scan.coverity.com/projects/4940">
    <img alt="coverity" src="https://scan.coverity.com/projects/4940/badge.svg"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rirc&metric=sqale_rating"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rirc&metric=reliability_rating"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rirc&metric=security_rating"/>
  </a>
</p>

---

# rirc

A minimalistic irc client written in C.

Connections are TLS enabled over port 6697 by default.

### Configuring:

Configure rirc by editing `config.h`. Defaults are in `config.def.h`

### Building:

rirc requires the latest version of GNU gperf to compile.

See: https://www.gnu.org/software/gperf/

Build rirc:

```
git submodule init
git submodule update --recursive
make
```

### Installing:

Default install path:

```
BIN_DIR = /usr/local/bin
MAN_DIR = /usr/local/share/man/man1
```

Edit `Makefile` to alter install path if needed, then:

```
make install
```

### Usage:

```
rirc [-hv] [-s server [...]]

Info:
  -h, --help      Print help message and exit
  -v, --version   Print rirc version and exit

Server options:
  -s, --server=SERVER       Connect to SERVER
  -p, --port=PORT           Connect to SERVER using PORT
  -w, --pass=PASS           Connect to SERVER using PASS
  -u, --username=USERNAME   Connect to SERVER using USERNAME
  -r, --realname=REALNAME   Connect to SERVER using REALNAME
  -n, --nicks=NICKS         Comma separated list of nicks to use for SERVER
  -c, --chans=CHANNELS      Comma separated list of channels to join for SERVER

Server connection options:
   --ipv4                   Connect to server using only ipv4 addresses
   --ipv6                   Connect to server using only ipv6 addresses
   --tls-disable            Set server TLS disabled
   --tls-verify=<mode>      Set server TLS peer certificate verification mode
```

Commands:

```
  :clear
  :close
  :connect
  :disconnect
  :quit
```

Keys:

```
  ^N : go to next channel
  ^P : go to previous channel
  ^L : clear channel
  ^X : close channel
  ^C : cancel input/action
  ^U : scroll buffer up
  ^D : scroll buffer down
   ← : input cursor back
   → : input cursor forward
   ↑ : input history back
   ↓ : input history forward
```

### More info:

[rcr.io/rirc/](http://rcr.io/rirc/)
