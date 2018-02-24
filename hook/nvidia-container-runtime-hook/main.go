
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"runtime"
	"runtime/debug"
	"strconv"
	"strings"
	"syscall"
)

var (
	debugflag = flag.Bool("debug", false, "enable debug output")
)

func exit() {
	if err := recover(); err != nil {
		if _, ok := err.(runtime.Error); ok {
			log.Println(err)
		}
		if *debugflag {
			log.Printf("%s", debug.Stack())
		}
		os.Exit(1)
	}
	os.Exit(0)
}

func capabilityToCLI(cap string) string {
	switch cap {
	case "compute":
		return "--compute"
	case "compat32":
		return "--compat32"
	case "graphics":
		return "--graphics"
	case "utility":
		return "--utility"
	case "video":
		return "--video"
	default:
		log.Panicln("unknown driver capability:", cap)
	}
	return ""
}

func doPrestart(state HookState) {
	log.SetFlags(0)

	hook := getHookConfig()
	cli := hook.NvidiaContainerCLI

	container := getContainerConfig(hook, state)
	nvidia := container.Nvidia
	if nvidia == nil {
		// Not a GPU container, nothing to do.
		return
	}

	args := []string{cli.Path}
	if cli.LoadKmods {
		args = append(args, "--load-kmods")
	}
	if *debugflag {
		args = append(args, "--debug=/dev/stderr")
	} else if cli.Debug != nil {
		args = append(args, fmt.Sprintf("--debug=%s", *cli.Debug))
	}
	if cli.Ldcache != nil {
		args = append(args, fmt.Sprintf("--ldcache=%s", *cli.Ldcache))
	}
	args = append(args, "configure")

	if cli.Ldconfig != nil {
		args = append(args, fmt.Sprintf("--ldconfig=%s", *cli.Ldconfig))
	}

	if len(nvidia.Devices) > 0 {
		args = append(args, fmt.Sprintf("--device=%s", nvidia.Devices))
	}

	for _, cap := range strings.Split(nvidia.Capabilities, ",") {
		if len(cap) == 0 {
			break
		}
		args = append(args, capabilityToCLI(cap))
	}

	if !hook.DisableRequire && !nvidia.DisableRequire {
		for _, req := range nvidia.Requirements {
			args = append(args, fmt.Sprintf("--require=%s", req))
		}
	}

	args = append(args, fmt.Sprintf("--pid=%s", strconv.FormatUint(uint64(container.Pid), 10)))
	args = append(args, container.Rootfs)

	log.Printf("exec command: %v", args)
	env := append(os.Environ(), cli.Environment...)
	err := syscall.Exec(cli.Path, args, env)
	log.Panicln("exec failed:", err)
}

func usage() {
	fmt.Fprintf(os.Stderr, "Usage of %s:\n", os.Args[0])
	flag.PrintDefaults()
}

func main() {
	defer exit()
	flag.Usage = usage
	flag.Parse()

	var state HookState
	d := json.NewDecoder(os.Stdin)
	if err := d.Decode(&state); err != nil {
		log.Panicln("could not decode container state:", err)
	}

	switch state.Status {
	case "created":
		doPrestart(state)
		os.Exit(0)
	case "creating": fallthrough
	case "running": fallthrough
	case "stopped":
		os.Exit(0)
	default:
		flag.Usage()
		os.Exit(2)
	}
}
