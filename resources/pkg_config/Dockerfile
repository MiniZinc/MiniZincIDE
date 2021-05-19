ARG BASE=ubuntu:latest
FROM ${BASE} AS composer

# Install MiniZinc toolchain
COPY minizinc/bin/* /usr/local/bin/
COPY minizinc/share/minizinc /usr/local/share/minizinc

# Install vendor solvers
COPY vendor/gecode /usr/local
COPY vendor/chuffed /usr/local

# Strip all binaries
RUN [ -f "/etc/alpine-release" ] && apk add --no-cache binutils || (apt-get update -y && apt-get install -y binutils)
RUN cd /usr/local/bin && strip minizinc mzn2doc fzn-chuffed fzn-gecode

# Generate resulting Docker image
FROM ${BASE} 

RUN [ ! -f "/etc/alpine-release" ] || apk add --no-cache libstdc++

COPY --from=composer /usr/local/bin/* /usr/local/bin/
COPY --from=composer /usr/local/share/minizinc /usr/local/share/minizinc
