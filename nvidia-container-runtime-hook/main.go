package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"
	"syscall"
)

var (
	prestart = flag.Bool("prestart", false, "run the prestart hook")
)

func capToCLI(cap string) string {
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
		log.Fatalln("unknown driver capability:", cap)
	}
	return ""
}

func doPrestart() {
	log.SetFlags(0)

	cli := getCLIConfig()
	config := getContainerConfig()

	nvidia := config.nvidia
	if nvidia == nil {
		// Not a GPU container, nothing to do.
		return
	}

	args := []string{cli.Path}
	if cli.LoadKmods {
		args = append(args, "--load-kmods")
	}
	if cli.Debug != nil {
		args = append(args, fmt.Sprintf("--debug=%s", *cli.Debug))
	}
	args = append(args, "configure")

	if cli.Configure.Ldconfig != nil {
		args = append(args, fmt.Sprintf("--ldconfig=%s", *cli.Configure.Ldconfig))
	}

	if len(nvidia.devices) > 0 {
		args = append(args, fmt.Sprintf("--device=%s", nvidia.devices))
	}

	for _, cap := range strings.Split(nvidia.caps, ",") {
		if len(cap) == 0 {
			break
		}
		args = append(args, capToCLI(cap))
	}

	if len(nvidia.cudaVersion) > 0 {
		var vmaj, vmin int
		if _, err := fmt.Sscanf(nvidia.cudaVersion, "%d.%d", &vmaj, &vmin); err != nil {
			log.Fatalln("invalid CUDA version:", nvidia.cudaVersion)
		}
		args = append(args, fmt.Sprintf("--require=cuda>=%d.%d", vmaj, vmin))
	}

	args = append(args, fmt.Sprintf("--pid=%s", strconv.FormatUint(uint64(config.pid), 10)))
	args = append(args, config.rootfs)

	log.Printf("exec command: %v", args)
	env := append(os.Environ(), cli.Environment...)
	err := syscall.Exec(cli.Path, args, env)
	log.Fatalln("exec failed:", err)
}

func main() {
	flag.Parse()

	if *prestart {
		doPrestart()
	}
}
