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
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rcr_rirc&metric=reliability_rating"/>
  </a>
  <a href="https://sonarcloud.io/dashboard?id=rcr_rirc">
    <img alt="sonarcloud" src="https://sonarcloud.io/api/project_badges/measure?project=rcr_rirc&metric=security_rating"/>
  </a>
</p>

---

# rirc
A minimalistic irc client written in C.

rirc supports only TLS connections, the default port is 6697

## Configuring:

Configure rirc by editing `config.h`

## Building:
rirc requires the latest version of GNU gperf to compile.

See: https://www.gnu.org/software/gperf/

Initialize, configure and build mbedtls:

    git submodule init
    git submodule update --recursive
    cd mbedtls
    ./scripts/config.pl set MBEDTLS_THREADING_C
    ./scripts/config.pl set MBEDTLS_THREADING_PTHREAD
    cmake .
    cmake --build .
    cd ..

Build rirc:

    make

## Installing:
Default install path:

    EXE_DIR = /usr/local/bin
    MAN_DIR = /usr/local/share/man/man1

Edit `Makefile` to alter install path if needed, then:

    make install

## Usage:

    rirc [-hv] [-s server [-p port] [-w pass] [-u user] [-r real] [-n nicks] [-c chans]], ...]

    Help:
      -h, --help      Print this message and exit
      -v, --version   Print rirc version and exit

    Options:
      -s, --server=SERVER       Connect to SERVER
      -p, --port=PORT           Connect to SERVER using PORT
      -w, --pass=PASS           Connect to SERVER using PASS
      -u, --username=USERNAME   Connect to SERVER using USERNAME
      -r, --realname=REALNAME   Connect to SERVER using REALNAME
      -n, --nicks=NICKS         Comma separated list of nicks to use for SERVER
      -c, --chans=CHANNELS      Comma separated list of channels to join for SERVER

Commands:

      :clear
      :close
      :connect [host [port] [pass] [user] [real]]
      :disconnect
      :quit

Keys:

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

## More info:
[rcr.io/rirc/](http://rcr.io/rirc/)
