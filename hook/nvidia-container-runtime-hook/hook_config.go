package main

import (
	"log"
	"os"

	"github.com/BurntSushi/toml"
)

const (
	configPath = "/etc/nvidia-container-runtime/config.toml"
)

// CLIConfig: options for nvidia-container-cli.
type CLIConfig struct {
	Root        *string  `toml:"root"`
	Path        *string  `toml:"path"`
	Environment []string `toml:"environment"`
	Debug       *string  `toml:"debug"`
	Ldcache     *string  `toml:"ldcache"`
	LoadKmods   bool     `toml:"load-kmods"`
	User        *string  `toml:"user"`
	Ldconfig    *string  `toml:"ldconfig"`
}

type HookConfig struct {
	DisableRequire bool    `toml:"disable-require"`
	SwarmResource  *string `toml:"swarm-resource"`

	NvidiaContainerCLI CLIConfig `toml:"nvidia-container-cli"`
}

func getDefaultHookConfig() (config HookConfig) {
	return HookConfig{
		DisableRequire: false,
		SwarmResource:  nil,
		NvidiaContainerCLI: CLIConfig{
			Root:        nil,
			Path:        nil,
			Environment: []string{},
			Debug:       nil,
			Ldcache:     nil,
			LoadKmods:   true,
			User:        nil,
			Ldconfig:    nil,
		},
	}
}

func getHookConfig() (config HookConfig) {
	config = getDefaultHookConfig()
	_, err := toml.DecodeFile(configPath, &config)
	if err != nil && !os.IsNotExist(err) {
		log.Panicln("couldn't open configuration file:", err)
	}

	return config
}
