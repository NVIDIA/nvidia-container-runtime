RUNTIME=nvidia-container-runtime
MOCK_RUNC=$(CURDIR)/runc

all: build verify test
verify: fmt lint vet assign

mock-runc:
	@(printf '#!/bin/bash\necho mock runc') > ${MOCK_RUNC}
	@chmod +x ${MOCK_RUNC}

mock-hook:
	[ ! -e /etc/nvidia-container-runtime ] && mkdir /etc/nvidia-container-runtime || true
	[ ! -e /etc/nvidia-container-runtime/config.toml ] && echo "" > /etc/nvidia-container-runtime/config.toml || true
	[ ! -e /usr/bin/nvidia-container-runtime-hook ] && echo "" > /usr/bin/nvidia-container-runtime-hook && chmod +x /usr/bin/nvidia-container-runtime-hook || true

test: build mock-runc mock-hook
	@go test -v
	@${RM} ${MOCK_RUNC}

clean:
	@${RM} ${RUNTIME}


fmt:
	find . -not \( \( -wholename './.*' -o -wholename '*/vendor/*' \) -prune \) -name '*.go' \
		| sort -u | xargs gofmt -s -l

lint:
	find . -not \( \( -wholename './.*' -o -wholename '*/vendor/*' \) -prune \) -name '*.go' \
		| sort -u | xargs golint

vet:
	go list ./... | grep -v "vendor" | xargs go vet

assign:
	find . -not \( \( -wholename './.*' -o -wholename '*/vendor/*' \) -prune \) -name '*.go' \
		| sort -u | xargs ineffassign
