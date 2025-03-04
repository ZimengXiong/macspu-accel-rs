.PHONY: check package dry-run publish probe clean

CRATE_NAME := $(shell sed -n 's/^name = "\(.*\)"/\1/p' Cargo.toml | head -n 1)
CRATE_VERSION := $(shell sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -n 1)
DIST_DIR := dist

check:
	cargo check

package:
	cargo package
	mkdir -p $(DIST_DIR)
	cp target/package/$(CRATE_NAME)-$(CRATE_VERSION).crate $(DIST_DIR)/

dry-run:
	cargo publish --dry-run

publish:
	cargo publish

probe:
	cargo run --example probe

clean:
	git clean -fdX .
