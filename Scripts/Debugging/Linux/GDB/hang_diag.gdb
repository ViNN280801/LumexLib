# hang_diag.gdb - batch snapshot of a hung process or core dump (sourced by hang_diag.sh).
# Pure GDB commands only (no Python required).
# Variables (set via -ex before -x):
#   $hang_log, $hang_solib, $hang_verbose, $hang_bt_depth, $hang_mode (0 live, 1 core, 2 run)
#   $hang_shell_log (1 = stdout only; hang_diag.sh uses tee when sudo attach)

set pagination off
set print demangle on
set print pretty on
set print thread-events off
set print elements 0

if $hang_shell_log == 0
  eval "set logging file %s", $hang_log
  set logging overwrite on
  set logging redirect on
  set logging enabled on
else
  echo \n(shell tee logging; GDB internal logging disabled for sudo attach)\n
end

echo \n========================================================================\n hang_diag snapshot\n========================================================================\n

echo \n========== gdb version ==========\n
show version

echo \n========== target executable ==========\n
info proc exe

if $hang_mode == 0
  echo \n========== live process (proc) ==========\n
  info proc cmdline
  info proc cwd
  echo \n(proc environ is appended by hang_diag.sh after detach)\n
end

echo \n========== process (summary) ==========\n
info proc

echo \n========== memory mappings ==========\n
info proc mappings

if $hang_mode == 0
  echo \n========== process stat ==========\n
  info proc stat

  echo \n========== process status ==========\n
  info proc status
end

echo \n========== solib path ==========\n
printf "solib-search-path: %s\n", $hang_solib

eval "set solib-search-path %s", $hang_solib
sharedlibrary

echo \n========== shared libraries ==========\n
info sharedlibrary

echo \n========== threads ==========\n
info threads

if $hang_mode == 2
  echo \n========== run target ==========\n
  run
end

if $hang_mode != 0
  echo \n========== crash context ==========\n
  echo Stop reason / signal (see below)\n
  info program

  echo \n========== crash: signal table ==========\n
  info signals

  echo \n========== crash: faulting frame ==========\n
  frame
  info args
  info locals

  echo \n========== crash: disassembly at PC ==========\n
  x/8i $pc

  echo \n========== crash: memory at PC ==========\n
  x/16gx $pc-64

  echo \n========== crash: faulting thread bt full ==========\n
  bt full
end

echo \n========== current thread registers ==========\n
info registers

echo \n========== all threads bt (depth "
printf "%d", $hang_bt_depth
echo ) ==========\n
thread apply all bt $hang_bt_depth

if $hang_verbose != 0
  echo \n========== all threads bt full ==========\n
  thread apply all bt full

  echo \n========== verbose: thread frames ==========\n
  thread apply all frame

  echo \n========== verbose: thread args ==========\n
  thread apply all info args

  echo \n========== verbose: thread locals ==========\n
  thread apply all info locals

  echo \n========== verbose: all thread registers ==========\n
  thread apply all info registers
else
  if $hang_mode == 0
    echo \n========== all threads bt full ==========\n
    thread apply all bt full
  end
end

echo \n========== sanitizer / stderr hints ==========\n
echo Search this log for MemorySanitizer, AddressSanitizer, UndefinedBehaviorSanitizer,\n
echo ThreadSanitizer, ERROR:, SUMMARY: lines. Inferior stderr is merged when using hang_diag.sh tee.\n

echo \n========== DONE ==========\n
if $hang_shell_log == 0
  set logging enabled off
end

if $hang_mode == 0
  detach
end

quit
