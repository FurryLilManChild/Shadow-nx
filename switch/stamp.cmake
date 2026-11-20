# Writes build_stamp.h with the time and the git commit of this build, for the log's first line (which NRO ran,
# checked against GitHub by tools/check_log.py). Run at every build.
string(TIMESTAMP now "%Y-%m-%d %H:%M")
execute_process(COMMAND git -c safe.directory=* -C "${CMAKE_CURRENT_LIST_DIR}/.." rev-parse --short=12 HEAD
                OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -c safe.directory=* -C "${CMAKE_CURRENT_LIST_DIR}/.." status --porcelain --untracked-files=no
                OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT commit)
  set(commit "unknown")
endif()
if(dirty)
  set(commit "${commit}-dirty")
endif()
file(WRITE "${OUT}.tmp" "#define SHADOW_NX_STAMP \"${now}, commit ${commit}\"\n")
file(COPY_FILE "${OUT}.tmp" "${OUT}" ONLY_IF_DIFFERENT)
