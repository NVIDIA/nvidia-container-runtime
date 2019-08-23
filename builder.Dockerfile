FROM golang:1.10

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    	    unzip \
	    wget

RUN wget https://releases.hashicorp.com/terraform/0.12.6/terraform_0.12.6_linux_amd64.zip -O tf.zip && \
    unzip tf.zip && mv terraform /usr/bin && rm tf.zip

ARG PKG_PATH
WORKDIR ${PKG_PATH}

COPY . .

RUN go get -u golang.org/x/lint/golint

CMD ["bash"]
