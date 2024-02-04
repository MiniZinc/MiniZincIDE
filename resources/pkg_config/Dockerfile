ARG BASE=ubuntu:latest
FROM ${BASE} AS composer
ARG TARGETPLATFORM

# Install MiniZinc toolchain
COPY ${TARGETPLATFORM}/minizinc/bin/* /usr/local/bin/
COPY ${TARGETPLATFORM}/minizinc/share/minizinc /usr/local/share/minizinc

# Install vendor solvers
COPY ${TARGETPLATFORM}/vendor/gecode /usr/local
COPY ${TARGETPLATFORM}/vendor/chuffed /usr/local
COPY ${TARGETPLATFORM}/vendor/or-tools /or-tools
COPY ${TARGETPLATFORM}/vendor/highs /usr/local

# Strip all binaries
RUN [ -f "/etc/alpine-release" ] && apk add --no-cache binutils || (apt-get update -y && apt-get install -y binutils)
RUN cd /usr/local/bin && strip minizinc mzn2doc fzn-chuffed fzn-gecode /or-tools/bin/fzn-cp-sat

# Generate resulting Docker image
FROM ${BASE} 

RUN [ ! -f "/etc/alpine-release" ] || apk add --no-cache libstdc++

COPY --from=composer /usr/local/bin/* /usr/local/bin/
COPY --from=composer /usr/local/share/minizinc /usr/local/share/minizinc
COPY --from=composer /usr/local/lib*/libhighs.so /usr/local/lib/
COPY --from=composer /or-tools/bin/fzn-cp-sat /usr/local/bin/
COPY --from=composer /or-tools/share/minizinc /usr/local/share/minizinc
