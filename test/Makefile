TESTS = connect stmthandles select commands multistmt getresult prepare \
	params notice arraybinding insertreturning dataatexecution \
	boolsaschar cvtnulldate alter quotes cursors positioned-update \
	catalogfunctions

TESTBINS = $(patsubst %,src/%-test, $(TESTS))
TESTSQLS = $(patsubst %,sql/%.sql, $(TESTS))

REGRESS = sampletables $(TESTS)

all: $(TESTBINS) $(TESTSQLS)

installcheck: all

override CFLAGS += -Wno-pointer-sign

src/common.o: src/common.c

# For each test file, compile the .c file, and create a .sql file that
# when executed from psql, just runs the binary.
src/%-test sql/%.sql: src/%-test.c src/common.o
	$(CC) $(CFLAGS) src/$*-test.c src/common.o -o src/$*-test -lodbc
	echo "\! ./src/$*-test" > sql/$*.sql

EXTRA_CLEAN = $(TESTBINS) $(TESTSQLS) src/common.o

REGRESS_OPTS = --launcher=./launcher

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

