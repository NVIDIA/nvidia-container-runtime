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

// CLIConfig: options for nvidia-container-cli.
type CLIConfig struct {
	Path        string   `toml:"path"`
	Environment []string `toml:"environment"`
	Debug       *string  `toml:"debug"`
	LoadKmods   bool     `toml:"load-kmods"`
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
			Path:        "",
			Environment: []string{},
			Debug:       nil,
			LoadKmods:   true,
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
	if len(config.NvidiaContainerCLI.Path) == 0 {
		config.NvidiaContainerCLI.Path, err = exec.LookPath("nvidia-container-cli")
		if err != nil {
			log.Panicln("couldn't find binary nvidia-container-cli:", err)
		}
	}

	return config
}
