/*
# Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
*/

package main

import (
	"fmt"
	"os"

	"github.com/sirupsen/logrus"
	"github.com/tsaikd/KDGoLib/logrusutil"
)

type Logger struct {
	*logrus.Logger
	logFile *os.File
}

func NewLogger() *Logger {
	logrusLogger := logrus.New()

	formatter := &logrusutil.ConsoleLogFormatter{
		TimestampFormat: "2006/01/02 15:04:07",
		Flag:            logrusutil.Ltime,
	}

	logger := &Logger{
		Logger: logrusLogger,
	}
	logger.SetFormatter(formatter)

	return logger
}

func (l *Logger) LogToFile(filename string) error {
	logFile, err := os.OpenFile(filename, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return fmt.Errorf("error opening debug log file: %v", err)
	}

	l.logFile = logFile
	l.SetOutput(logFile)

	return nil
}

func (l *Logger) CloseFile() error {
	if l.logFile == nil {
		return nil
	}
	return l.logFile.Close()
}
