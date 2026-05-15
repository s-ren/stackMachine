#!/bin/bash
set -euo pipefail

STACK_LLVM_APT_VERSION="${STACK_LLVM_APT_VERSION:-18}"
stack_ran_apt_update=0

has_command() {
	command -v "$1" >/dev/null 2>&1
}

is_apt_package_installed() {
	dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "install ok installed"
}

run_apt() {
	if [[ "${EUID}" -eq 0 ]]; then
		apt-get "$@"
		return
	fi

	if has_command sudo; then
		sudo apt-get "$@"
		return
	fi

	echo "error: apt-get requires root privileges; rerun as root or install sudo" >&2
	exit 1
}

ensure_apt_package() {
	local package_name="$1"
	if is_apt_package_installed "$package_name"; then
		return
	fi

	if [[ "${stack_ran_apt_update}" -eq 0 ]]; then
		run_apt update
		stack_ran_apt_update=1
	fi

	run_apt install -y "$package_name"
}

ensure_system_dependencies() {
	if has_command apt-get; then
		ensure_apt_package build-essential
		ensure_apt_package cmake
		ensure_apt_package python3
		ensure_apt_package "llvm-${STACK_LLVM_APT_VERSION}-dev"
		ensure_apt_package "libmlir-${STACK_LLVM_APT_VERSION}-dev"
		ensure_apt_package "mlir-${STACK_LLVM_APT_VERSION}-tools"
		return
	fi

	local missing_tools=()
	for tool_name in cmake ctest python3 llvm-config mlir-tblgen c++; do
		if ! has_command "$tool_name"; then
			missing_tools+=("$tool_name")
		fi
	done

	if [[ "${#missing_tools[@]}" -ne 0 ]]; then
		echo "error: missing required tools: ${missing_tools[*]}" >&2
		echo "Install them manually or run this script on a Debian/Ubuntu system with apt-get." >&2
		exit 1
	fi
}

ensure_system_dependencies

cmake -B build
cmake --build build --target install
ctest --test-dir build --output-on-failure
