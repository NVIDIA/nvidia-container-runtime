# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

.PHONY: all build builder test
.DEFAULT_GOAL := all


##### Global variables #####

DOCKERFILE  ?= $(CURDIR)/docker/Dockerfile
DOCKERDEVEL ?= $(CURDIR)/docker/Dockerfile.builder

REGISTRY    ?= nvidia
IMAGE       ?= $(REGISTRY)/container-toolkit:docker19.03
BUILDER     ?= $(REGISTRY)/container-toolkit:builder


##### Public rules #####

all: build

build:
	docker build -f $(DOCKERFILE) -t $(IMAGE) .

builder:
	docker build -f $(DOCKERDEVEL) -t $(BUILDER) .

test: build
	$(CURDIR)/test/docker_test.sh $(CURDIR)/shared $(IMAGE)
