package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/opencontainers/runtime-spec/specs-go"
	"github.com/pelletier/go-toml"
)

const (
	configOverride = "XDG_CONFIG_HOME"
	configFilePath = "nvidia-container-runtime/config.toml"

	hookDefaultFilePath = "/usr/bin/nvidia-container-runtime-hook"
)

var (
	configDir = "/etc/"
)

var logger = NewLogger()

type args struct {
	bundleDirPath string
	cmd           string
}

type config struct {
	debugFilePath string
}

func getConfig() (*config, error) {
	cfg := &config{}

	if XDGConfigDir := os.Getenv(configOverride); len(XDGConfigDir) != 0 {
		configDir = XDGConfigDir
	}

	configFilePath := path.Join(configDir, configFilePath)

	tomlContent, err := ioutil.ReadFile(configFilePath)
	if err != nil {
		return nil, err
	}

	toml, err := toml.Load(string(tomlContent))
	if err != nil {
		return nil, err
	}

	cfg.debugFilePath = toml.GetDefault("nvidia-container-runtime.debug", "/dev/null").(string)

	return cfg, nil
}

// getArgs checks the specified slice of strings (argv) for a 'bundle' flag and a 'create'
// command line argument as allowed by runc.
// The following are supported:
// --bundle{{SEP}}BUNDLE_PATH
// -bundle{{SEP}}BUNDLE_PATH
// -b{{SEP}}BUNDLE_PATH
// where {{SEP}} is either ' ' or '='
func getArgs(argv []string) (*args, error) {
	args := &args{}

	for i := 0; i < len(argv); i++ {
		param := argv[i]
		if param == "create" {
			args.cmd = param
			continue
		}

		if !strings.HasPrefix(param, "-") {
			continue
		}

		trimmed := strings.TrimLeft(param, "-")
		if len(trimmed) == 0 {
			continue
		}

		parts := strings.SplitN(trimmed, "=", 2)
		if parts[0] != "bundle" && parts[0] != "b" {
			continue
		}

		if len(parts) == 2 {
			args.bundleDirPath = parts[1]
			continue
		}

		if len(argv)-i <= 1 {
			return nil, fmt.Errorf("bundle option needs an argument")
		}
		args.bundleDirPath = argv[i+1]
		i++
	}

	return args, nil
}

func exitOnError(err error, msg string) {
	if err != nil {
		logger.Fatalf("ERROR: %s: %s: %v\n", os.Args[0], msg, err)
	}
}

// execRuncAndExit discovers the runc binary and issues an exec syscall.
// If this is not successful, the program is exited.
func execRuncAndExit() {
	runcCandidates := []string{
		"docker-runc",
		"runc",
	}

	var err error
	var runcPath string
	for _, candidate := range runcCandidates {
		logger.Printf("Looking for \"%v\" binary", candidate)
		runcPath, err = exec.LookPath(candidate)
		if err == nil {
			break
		}
		logger.Printf("\"%v\" binary not found", candidate)
	}
	exitOnError(err, "find runc path")

	logger.Printf("Runc path: %s\n", runcPath)

	err = syscall.Exec(runcPath, append([]string{runcPath}, os.Args[1:]...), os.Environ())

	// syscall.Exec is not expected to return. This is an error state regardless of whether
	// err is nil or not.
	logger.Fatalf("could not exec %v from %v: %v", runcPath, os.Args[0], err)
}

func addNVIDIAHook(spec *specs.Spec) error {
	path, err := exec.LookPath("nvidia-container-runtime-hook")
	if err != nil {
		path = hookDefaultFilePath
		_, err = os.Stat(path)
		if err != nil {
			return err
		}
	}

	logger.Printf("prestart hook path: %s\n", path)

	args := []string{path}
	if spec.Hooks == nil {
		spec.Hooks = &specs.Hooks{}
	} else if len(spec.Hooks.Prestart) != 0 {
		for _, hook := range spec.Hooks.Prestart {
			if !strings.Contains(hook.Path, "nvidia-container-runtime-hook") {
				continue
			}
			logger.Println("existing nvidia prestart hook in OCI spec file")
			return nil
		}
	}

	spec.Hooks.Prestart = append(spec.Hooks.Prestart, specs.Hook{
		Path: path,
		Args: append(args, "prestart"),
	})

	return nil
}

func main() {
	cfg, err := getConfig()
	exitOnError(err, "fail to get config")

	logFile, err := os.OpenFile(cfg.debugFilePath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	exitOnError(err, "fail to open debug log file")
	defer logFile.Close()

	logger.SetOutput(logFile)

	logger.Printf("Running %s\n", os.Args[0])

	args, err := getArgs(os.Args)
	exitOnError(err, "fail to get args")

	if args.cmd != "create" {
		logger.Println("Command is not \"create\", executing runc doing nothing")
		execRuncAndExit()
	}

	configFilePath, err := args.getConfigFilePath()
	exitOnError(err, "error getting config file path")

	logger.Printf("Using OCI specification file path: %v", configFilePath)

	jsonFile, err := os.OpenFile(configFilePath, os.O_RDWR, 0644)
	exitOnError(err, "open OCI spec file")

	defer jsonFile.Close()

	jsonContent, err := ioutil.ReadAll(jsonFile)
	exitOnError(err, "read OCI spec file")

	var spec specs.Spec
	err = json.Unmarshal(jsonContent, &spec)
	exitOnError(err, "unmarshal OCI spec file")

	err = addNVIDIAHook(&spec)
	exitOnError(err, "inject NVIDIA hook")

	jsonOutput, err := json.Marshal(spec)
	exitOnError(err, "marchal OCI spec file")

	_, err = jsonFile.WriteAt(jsonOutput, 0)
	exitOnError(err, "write OCI spec file")

	logger.Print("Prestart hook added, executing runc")
	execRuncAndExit()
}

func (a args) getConfigFilePath() (string, error) {
	configRoot := a.bundleDirPath
	if configRoot == "" {
		logger.Printf("Bundle directory path is empty, using working directory.")
		workingDirectory, err := os.Getwd()
		if err != nil {
			return "", fmt.Errorf("error getting working directory: %v", err)
		}
		configRoot = workingDirectory
	}

	logger.Printf("Using bundle directory: %v", configRoot)

	configFilePath := filepath.Join(configRoot, "config.json")

	return configFilePath, nil
}
