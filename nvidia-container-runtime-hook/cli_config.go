package main

import (
	"log"
	"os"
	"os/exec"

	"github.com/BurntSushi/toml"
)

const (
	configPath = "/etc/nvidia-container-runtime/config.toml"
)

type ConfigureOptions struct {
	Ldconfig *string
}

type CLIConfig struct {
	Path           string
	Environment    []string
	LoadKmods      bool `toml:"load-kmods"`
	DisableRequire bool `toml:"disable-require"`
	Debug          *string

	Configure ConfigureOptions
}

func getDefaultCLIConfig() (config *CLIConfig) {
	return &CLIConfig{
		Path:           "",
		Environment:    []string{},
		LoadKmods:      true,
		DisableRequire: false,
		Debug:          nil,
		Configure: ConfigureOptions{
			Ldconfig: nil,
		},
	}
}

func getCLIConfig() (config *CLIConfig) {
	config = getDefaultCLIConfig()
	_, err := toml.DecodeFile(configPath, &config)
	if err != nil && !os.IsNotExist(err) {
		log.Panicln("couldn't open configuration file:", err)
	}
	if len(config.Path) == 0 {
		config.Path, err = exec.LookPath("nvidia-container-cli")
		if err != nil {
			log.Panicln("couldn't find binary nvidia-container-cli:", err)
		}
	}

	return config
}
