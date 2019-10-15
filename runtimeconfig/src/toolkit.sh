#! /bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.

packages=("/usr/bin/nvidia-container-runtime" \
	"/usr/bin/nvidia-container-toolkit" \
	"/usr/bin/nvidia-container-cli" \
	"/etc/nvidia-container-runtime/config.toml" \
	"/usr/lib/x86_64-linux-gnu/libnvidia-container.so.1")

toolkit::install() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "${FUNCNAME[0]} $*"

	mkdir -p "/nvidia" "${destination}"
	mount --rbind "/nvidia" "${destination}"
	mount --make-private "${destination}"
	mount --make-runbindable "${destination}"

	log INFO "Mount point ${destination} contains : $(ls -la ${destination})"

	mkdir -p "${destination}"
	mkdir -p "${destination}/.config/nvidia-container-runtime"

	# Note: Bash arrays start at 0 (zsh arrays start at 1)
	for ((i=0; i < ${#packages[@]}; i++)); do
		packages[$i]=$(readlink -f "${packages[$i]}")
	done

	cp "${packages[@]}" "${destination}"
	mv "${destination}/config.toml" "${destination}/.config/nvidia-container-runtime/"
}

toolkit::uninstall() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "${FUNCNAME[0]} $*"

	if findmnt -r -o TARGET | grep "${destination}" > /dev/null; then
		umount -l -R "${destination}" || true
	fi
}

toolkit::setup::config() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	local -r config_path="${destination}/.config/nvidia-container-runtime/config.toml"
	log INFO "${FUNCNAME[0]} $*"

	sed -i 's/^#root/root/;' "${config_path}"
	sed -i "s@/run/nvidia/driver@${RUN_DIR}/driver@;" "${config_path}"
	sed -i "s;@/sbin/ldconfig.real;@${RUN_DIR}/driver/sbin/ldconfig.real;" "${config_path}"
}

toolkit::setup::cli_binary() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "${FUNCNAME[0]} $*"

	# Setup links to the real binaries to ensure that variables and configs
	# are pointing to the right path
	mv "${destination}/nvidia-container-cli" \
		"${destination}/nvidia-container-cli.real"

	# Setup aliases so as to ensure that the path is correctly set
	cat <<- EOF | tr -s ' \t' > ${destination}/nvidia-container-cli
		#! /bin/sh
		LD_LIBRARY_PATH="${destination}" \
		PATH="\$PATH:${destination}" \
		${destination}/nvidia-container-cli.real \
			"\$@"
	EOF

	# Make sure that the alias files are executable
	chmod +x "${destination}/nvidia-container-cli"
}

toolkit::setup::toolkit_binary() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "${FUNCNAME[0]} $*"

	mv "${destination}/nvidia-container-toolkit" \
		"${destination}/nvidia-container-toolkit.real"

	cat <<- EOF | tr -s ' \t' > ${destination}/nvidia-container-toolkit
		#! /bin/sh
		PATH="\$PATH:${destination}" \
		${destination}/nvidia-container-toolkit.real \
			-config "${destination}/.config/nvidia-container-runtime/config.toml" \
			"\$@"
	EOF

	chmod +x "${destination}/nvidia-container-toolkit"
}

toolkit::setup::runtime_binary() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "${FUNCNAME[0]} $*"

	mv "${destination}/nvidia-container-runtime" \
		"${destination}/nvidia-container-runtime.real"

	cat <<- EOF | tr -s ' \t' > ${destination}/nvidia-container-runtime
		#! /bin/sh
		PATH="\$PATH:${destination}" \
		XDG_CONFIG_HOME="${destination}/.config" \
		${destination}/nvidia-container-runtime.real \
			"\$@"
	EOF

	chmod +x "${destination}/nvidia-container-runtime"
}

toolkit::setup() {
	local -r destination="${1:-"${TOOLKIT_DIR}"}"
	log INFO "Installing the NVIDIA Container Toolkit"

	toolkit::install "${destination}"

	toolkit::setup::config "${destination}"
	toolkit::setup::cli_binary "${destination}"
	toolkit::setup::toolkit_binary "${destination}"
	toolkit::setup::runtime_binary "${destination}"

	# The runtime shim is still looking for the old binary
	# Move to ${destination} to get expanded
	# Make symlinks local so that they still refer to the
	# local target when mounted on the host
	cd "${destination}"
	ln -s "./nvidia-container-toolkit" \
		"${destination}/nvidia-container-runtime-hook"
	ln -s "./libnvidia-container.so.1."* \
		"${destination}/libnvidia-container.so.1"
	cd -

	log INFO "Done setting up the NVIDIA Container Toolkit"
}
