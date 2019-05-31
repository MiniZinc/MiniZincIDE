FROM alpine:latest AS composer

# Install MiniZinc toolchain
COPY minizinc/bin/* /usr/local/bin/
COPY minizinc/share/minizinc /usr/local/share/minizinc

# Install vendor solvers
COPY vendor/gecode/bin/fzn-gecode /usr/local/bin/
COPY vendor/gecode/share/gecode/mznlib /usr/local/share/minizinc/gecode
COPY resources/solvers/gecode.msc /usr/local/share/minizinc/solvers/

COPY vendor/chuffed/bin/fzn-chuffed /usr/local/bin/
COPY vendor/chuffed/share/chuffed/mznlib /usr/local/share/minizinc/chuffed
COPY resources/solvers/chuffed.msc /usr/local/share/minizinc/solvers/

# Copy MiniZinc settings files
COPY resources/Preferences.json /usr/local/share/minizinc/

# Strip all binaries
RUN apk add --no-cache binutils
RUN cd /usr/local/bin && strip minizinc mzn2doc fzn-chuffed fzn-gecode

# Generate resulting Docker image
FROM alpine:latest

RUN apk add --no-cache libstdc++

COPY --from=composer /usr/local/bin/* /usr/local/bin/
COPY --from=composer /usr/local/share/minizinc /usr/local/share/minizinc
