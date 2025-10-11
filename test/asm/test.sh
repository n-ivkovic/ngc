#!/bin/sh

# Exit with error message
# $1 exit value
# $2 error message
_exit_err() {
	printf "%s: %s\n" "$0" "$2" >&2
	exit "$1"
}

# Set terminal foreground color
# $1 color
_term_color() {
	tput setaf "$1" 2>/dev/null || printf "%b[3%sm" "\033" "$1"
}

# Set config based on environment variables
[ -z "$NO_COLOR" ] && term_color=1 || term_color=0
[ "${LANG#*UTF-8}" != "$LANG" ] && term_unicode=1 || term_unicode=0

# Convert long options to short options
for arg in "$@"; do
	shift
	case "$arg" in
		"--ascii")    set -- "$@" "-a" ;;
		"--no-color") set -- "$@" "-p" ;;
		*)            set -- "$@" "$arg" ;;
	esac
done
OPTIND=1

# Parse options
while getopts ":ap" opt; do
	case "$opt" in
		a) term_unicode=0 ;;
		p) term_color=0 ;;
		\?) _exit_err 2 "-${OPTARG}: Option invalid" ;;
		:) _exit_err 2 "-${OPTARG}: Option requires an argument" ;;
	esac
done
shift $((OPTIND - 1))

# Get + validate executable file
exe_path="$1" && readonly exe_path
[ -z "$exe_path" ] && _exit_err 2 "Executable file not given"
! [ -f "$exe_path" ] && _exit_err 2 "${exe_path}: File not found"
! [ -x "$exe_path" ] && _exit_err 2 "${exe_path}: File not executable"

# Get test files
base_path="$(dirname "$0")" && readonly base_path
pos_path="${base_path}/positive" && readonly pos_path
neg_path="${base_path}/negative" && readonly neg_path
in_ext='.asm' && readonly in_ext
out_ext='.out' && readonly out_ext
err_ext='.err' && readonly err_ext

# Init test results
total_count=0
passed_count=0
failed_count=0
failed_names=

# Execute positive tests
pos_in_files="$(find "$pos_path" -type f -name "*${in_ext}")"
for in_file in $pos_in_files; do
	total_count=$((total_count + 1))

	# Arrange - Find file storing expected stdout
	out_file="$(printf "%s" "$in_file" | sed "s/${in_ext}\$/${out_ext}/")"
	! [ -f "$out_file" ] && _exit_err 3 "${out_file}: File not found"

	# Arrange - Build expected stdout
	out_expected="$(cat "$out_file")"

	# Act - Execute and concat both stdout and stderr
	exe_result="$("$exe_path" "$in_file" 2>&1)"

	# Assert
	# - Execution should return expected stdout
	# - Execution should return no stderr - any error should cause assertion to fail
	if [ "$exe_result" = "$out_expected" ]; then
		passed_count=$((passed_count + 1))
	else
		failed_count=$((failed_count + 1))
		failed_names="${failed_names}${pos_path}/$(basename "$in_file" | sed "s/${in_ext}\$//")\n"
	fi
done

# Execute negative tests
neg_in_files="$(find "$neg_path" -type f -name "*${in_ext}")"
for in_file in $neg_in_files; do
	total_count=$((total_count + 1))

	# Arrange - Find file storing expected stderr
	err_file="$(printf "%s" "$in_file" | sed "s/${in_ext}\$/${err_ext}/")"
	! [ -f "$err_file" ] && _exit_err 3 "${err_file}: File not found"

	# Arrange - Build expected stderr
	err_expected="$(printf "%s%s" "$in_file" "$(cat "$err_file")")"

	# Act - Execute and concat both stdout and stderr
	exe_result="$("$exe_path" "$in_file" 2>&1)"

	# Assert
	# - Execution should return expected stderr
	# - Execution should return no stdout - any output should cause assertion to fail
	if [ "$exe_result" = "$err_expected" ]; then
		passed_count=$((passed_count + 1))
	else
		failed_count=$((failed_count + 1))
		failed_names="${failed_names}${neg_path}/$(basename "$in_file" | sed "s/${in_ext}\$//")\n"
	fi
done

# Init output
passed_prefix=
failed_prefix=
suffix=

if [ "$term_color" -eq 1 ]; then
	if [ "$failed_count" -gt 0 ]; then
		passed_prefix="${passed_prefix}$(_term_color 3)"
		failed_prefix="${failed_prefix}$(_term_color 1)"
	else
		passed_prefix="${passed_prefix}$(_term_color 2)"
	fi

	suffix="$(tput sgr0 2>/dev/null || printf "%b[m" "\033")"
fi

if [ "$term_unicode" -eq 1 ]; then
	passed_prefix="${passed_prefix}\0342\0234\0224 "
	failed_prefix="${failed_prefix}\0342\0234\0230 "
fi

# Output test results
echo "${passed_prefix}Passed: ${passed_count}/${total_count}${suffix}"

if [ "$failed_count" -gt 0 ]; then
	echo "${failed_prefix}Failed: ${failed_count}/${total_count}${suffix}"
	printf "%b" "$failed_names" | while read -r failed_name; do
		echo "${failed_name}"
	done
	exit 1
else
	exit 0
fi
