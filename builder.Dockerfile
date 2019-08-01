FROM golang:1.10

ARG PKG_PATH
WORKDIR ${PKG_PATH}

COPY . .

RUN go get -u golang.org/x/lint/golint

CMD ["bash"]
