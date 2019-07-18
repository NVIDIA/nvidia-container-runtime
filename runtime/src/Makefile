RUNTIME=nvidia-container-runtime
MOCK_RUNC=$(CURDIR)/runc

all:
	@go build  -o ${RUNTIME}

mock-runc:
	@(echo '#!/bin/bash\necho mock runc') > ${MOCK_RUNC}
	@chmod +x ${MOCK_RUNC}

test: all mock-runc
	@go test -v
	@${RM} ${MOCK_RUNC}

clean:
	@${RM} ${RUNTIME}
