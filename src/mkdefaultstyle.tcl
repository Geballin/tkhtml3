
proc FileToDefine {file define} {
  set fd [open $file]
  append ret "#define $define \\"
  while {![eof $fd]} {
      set line [string map [list \" \\\" \\ \\\\] [gets $fd]]
      append ret "\n        \"$line\\n\" \\"
  }
  append ret "\n\n\n"

  return $ret
}

set css_file [file join [file dirname [info script]] .. tests html.css]
set tcl_file [file join [file dirname [info script]] .. tests tkhtml.tcl]
set quirks_file [file join [file dirname [info script]] quirks.css]

puts ""
puts [FileToDefine $tcl_file    HTML_DEFAULT_TCL]
puts [FileToDefine $css_file    HTML_DEFAULT_CSS]
puts [FileToDefine $quirks_file HTML_DEFAULT_QUIRKS]

