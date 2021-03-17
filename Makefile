# Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

DOCKER ?= docker
MKDIR  ?= mkdir
DIST_DIR ?= $(CURDIR)/dist

LIB_NAME := nvidia-container-runtime
LIB_VERSION := 3.4.2
PKG_REV := 1

TOOLKIT_VERSION := 1.4.2
GOLANG_VERSION  := 1.15.6

# By default run all native docker-based targets
docker-native:
include $(CURDIR)/docker/docker.mk

binary:
	cd src && go build -ldflags "-s -w" -o ../"$(LIB_NAME)" main.go
