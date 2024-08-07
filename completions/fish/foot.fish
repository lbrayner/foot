complete -c foot -x                            -a "(__fish_complete_subcommand)"
complete -c foot -r -s c -l config                                                                    -d "path to configuration file (XDG_CONFIG_HOME/foot/foot.ini)"
complete -c foot    -s C -l check-config                                                              -d "verify configuration and exit with 0 if ok, otherwise exit with 1"
complete -c foot -x -s o -l override                                                                  -d "configuration option to override, in form SECTION.KEY=VALUE"
complete -c foot -x -s f -l font               -a "(fc-list : family | sed 's/,/\n/g' | sort | uniq)" -d "font name and style in fontconfig format (monospace)"
complete -c foot -x -s t -l term               -a '(find /usr/share/terminfo -type f -printf "%f\n")' -d "value to set the environment variable TERM to (foot)"
complete -c foot -x -s T -l title                                                                     -d "initial window title"
complete -c foot -x -s a -l app-id                                                                    -d "value to set the app-id property on the Wayland window to (foot)"
complete -c foot    -s m -l maximized                                                                 -d "start in maximized mode"
complete -c foot    -s F -l fullscreen                                                                -d "start in fullscreen mode"
complete -c foot    -s L -l login-shell                                                               -d "start shell as a login shell"
complete -c foot -F -s D -l working-directory                                                         -d "initial working directory for the client application (CWD)"
complete -c foot -x -s w -l window-size-pixels                                                        -d "window WIDTHxHEIGHT, in pixels (700x500)"
complete -c foot -x -s W -l window-size-chars                                                         -d "window WIDTHxHEIGHT, in characters (not set)"
complete -c foot -F -s s -l server                                                                    -d "run as server; open terminals by running footclient"
complete -c foot    -s H -l hold                                                                      -d "remain open after child process exits"
complete -c foot -r -s p -l print-pid                                                                 -d "print PID to this file or FD when up and running (server mode only)"
complete -c foot -x -s d -l log-level           -a "info warning error none"                          -d "log-level (warning)"
complete -c foot -x -s l -l log-colorize        -a "always never auto"                                -d "enable or disable colorization of log output on stderr"
complete -c foot    -s S -l log-no-syslog                                                             -d "disable syslog logging (server mode only)"
complete -c foot -r      -l pty                                                                       -d "display an existing pty instead of creating one"
complete -c foot    -s v -l version                                                                   -d "show the version number and quit"
complete -c foot    -s h -l help                                                                      -d "show help message and quit"
