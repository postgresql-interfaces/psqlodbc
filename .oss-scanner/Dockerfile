# Build image for the Anthropic OSS Scanner (https://github.com/anthropics/oss-scanner).
# The scanner builds this with the repository as the build context and removes network access afterwards,
# so everything the build and the regression tests need is installed here.
FROM debian:bookworm
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates build-essential autoconf automake libtool pkg-config git python3 \
      libpq-dev unixodbc unixodbc-dev \
      postgresql postgresql-client \
    && rm -rf /var/lib/apt/lists/*

# configure runs odbc_config, which Debian's unixodbc-dev does not ship. The same stand-in is used by
# .github/workflows/linux.yml.
RUN if ! command -v odbc_config >/dev/null 2>&1; then \
      printf '%s\n' '#!/bin/sh' \
        'case $1 in' \
        '  --include-prefix) echo /usr/include ;;' \
        '  --lib-prefix) echo /usr/lib ;;' \
        '  --libs) echo -lodbc ;;' \
        '  *) echo "$0: unknown argument $1"; exit 1 ;;' \
        'esac' > /usr/local/bin/odbc_config \
      && chmod +x /usr/local/bin/odbc_config; \
    fi

# scanner contract: the checkout lives inside the image, at /src
COPY . /src
WORKDIR /src

# Build both drivers (psqlodbcw.so and psqlodbca.so in .libs/) with debug info and frame pointers.
RUN autoreconf -fi \
    && ./configure CFLAGS="-g -O2 -fno-omit-frame-pointer" \
    && make -j"$(nproc)"

# The regression tests need a running server. Create a superuser for root and the contrib_regression
# database, build the test programs, and run the suite once. A failing test does not fail the build.
# The server does not survive into the image; start it again with
#   pg_ctlcluster "$(pg_lsclusters -h | awk 'NR==1{print $1}')" main start
RUN cluster="$(pg_lsclusters -h | awk 'NR==1{print $1, $2}')" \
    && pg_ctlcluster $cluster start \
    && su postgres -c 'createuser -s root' \
    && createdb contrib_regression \
    && (cd test && make installcheck || echo "WARNING: regression tests reported failures") \
    && pg_ctlcluster $cluster stop
