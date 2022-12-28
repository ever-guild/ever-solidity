UNAME := $(shell uname)

ifeq ($(UNAME), win)
TARGET = sold.exe
else
TARGET = sold
endif

.deps-ready:
	bash script/toolchain.sh -s
	touch .deps-ready

clean:
	rm -fr release
	cargo clean

test: .deps-ready
	cargo test

fmt:
	cargo fmt

lint: .deps-ready
	cargo fmt --all -- --check
	cargo clippy --all-targets

qa: lint test

target/release/$(TARGET): .deps-ready
	cargo build --release

release: target/release/$(TARGET)
	mkdir -p release
	cp target/release/$(TARGET) release/

docker-build:
	docker build --tag ghcr.io/everscaleguild/ever-solidity:0.66.0 --file Dockerfile release

docker-push:
	docker tag ghcr.io/everscaleguild/ever-solidity:0.66.0 ghcr.io/everscaleguild/ever-solidity:latest
	docker push ghcr.io/everscaleguild/ever-solidity:0.66.0
	docker push ghcr.io/everscaleguild/ever-solidity:latest
