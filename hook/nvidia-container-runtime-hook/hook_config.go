package main

import (
	"log"
	"os"
	"path"

	"github.com/BurntSushi/toml"
)

// CLIConfig: options for nvidia-container-cli.
type CLIConfig struct {
	Root        *string  `toml:"root"`
	Path        *string  `toml:"path"`
	Environment []string `toml:"environment"`
	Debug       *string  `toml:"debug"`
	Ldcache     *string  `toml:"ldcache"`
	LoadKmods   bool     `toml:"load-kmods"`
	NoCgroups   bool     `toml:"no-cgroups"`
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
			NoCgroups:   false,
			User:        nil,
			Ldconfig:    nil,
		},
	}
}

func defaultPaths() []string {
	var paths []string
	const c = "nvidia-container-runtime/config.toml"
	if e := os.Getenv("XDG_RUNTIME_DIR"); e != "" {
		paths = append(paths, path.Join(e, "nvidia/driver/etc", c))
	}
	if e := os.Getenv("XDG_CONFIG_HOME"); e != "" {
		paths = append(paths, path.Join(e, c))
	}
	if e := os.Getenv("HOME"); e != "" {
		paths = append(paths, path.Join(e, ".config", c))
	}
	paths = append(paths, []string{path.Join("/run/nvidia/driver/etc", c), path.Join("/etc", c)}...)
	return paths
}

func getHookConfig() (config HookConfig) {
	var err error

	if len(*configflag) > 0 {
		config = getDefaultHookConfig()
		_, err = toml.DecodeFile(*configflag, &config)
		if err != nil {
			log.Panicln("couldn't open configuration file:", err)
		}
	} else {
		for _, p := range defaultPaths() {
			config = getDefaultHookConfig()
			_, err = toml.DecodeFile(p, &config)
			if err == nil {
				break
			} else if !os.IsNotExist(err) {
				log.Panicln("couldn't open default configuration file:", err)
			}
		}
	}

	return config
}
