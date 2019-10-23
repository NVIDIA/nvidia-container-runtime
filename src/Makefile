RUNTIME=nvidia-container-runtime
MOCK_RUNC=$(CURDIR)/runc

all: build verify test
verify: fmt lint vet assign

build:
	@go build  -o ${RUNTIME}

mock-runc:
	@(echo '#!/bin/bash\necho mock runc') > ${MOCK_RUNC}
	@chmod +x ${MOCK_RUNC}

test: build mock-runc
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
