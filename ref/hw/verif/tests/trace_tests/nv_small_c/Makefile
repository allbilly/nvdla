GCC:=/home/fedora/.local/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc
CFLAGS:=-Wall -O0 -g
LDFLAGS:=-static
VP_DEPLOY:=../../nv_small_tests

# Find all tests by looking for subdirectories with _test.c files
TESTS:=$(patsubst %/,%,$(dir $(wildcard */)))
TEST_BINS:=$(addsuffix _test,$(TESTS))

.PHONY: all clean deploy

all: $(TEST_BINS)

# Pattern rule: each binary is built inside its own subdirectory
%_test: %/%_test.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $*/$@ $<
	@echo "Built $*/$@"

deploy: $(TEST_BINS)
	@mkdir -p $(VP_DEPLOY)
	@for t in $(TESTS); do \
		mkdir -p $(VP_DEPLOY)/$$t; \
		cp -f $$t/$${t}_test $(VP_DEPLOY)/$$t/; \
		cp -f $$t/*_load.sh $(VP_DEPLOY)/$$t/ 2>/dev/null || true; \
		echo "Deployed $$t"; \
	done
	@echo "All tests deployed to $(VP_DEPLOY)/"

clean:
	@for t in $(TESTS); do \
		rm -f $$t/$${t}_test; \
	done
	@echo "Cleaned all test binaries"
