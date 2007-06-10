namespace eval hv3 { set {version($Id: hv3_log.tcl,v 1.14 2007/06/10 07:53:03 danielk1977 Exp $)} 1 }

namespace eval hv3 { set {version($Id: hv3_log.tcl,v 1.14 2007/06/10 07:53:03 danielk1977 Exp $)} 1 }

source [file join [file dirname [info script]] hv3_widgets.tcl]

proc ::hv3::log_window {html} {
  toplevel .event_log

  ::hv3::scrolled ::hv3::text .event_log.text -width 400 -height 300 -wrap none

  frame .event_log.button
  ::hv3::button .event_log.button.dismiss -text Dismiss 
  ::hv3::button .event_log.button.clear -text Clear 
  .event_log.button.clear configure -command {.event_log.text delete 0.0 end}
  .event_log.button.dismiss configure -command {destroy .event_log}

  pack .event_log.button.dismiss -side left -fill x -expand true
  pack .event_log.button.clear -side left -fill x -expand true
  pack .event_log.button -fill x -side bottom
  pack .event_log.text   -fill both -expand true

  bind .event_log <Destroy> [list ::hv3::destroy_log_window $html]

  $html configure -logcmd ::hv3::log_window_log
}

proc ::hv3::destroy_log_window {html} {
  $html configure -logcmd ""
}

set ::hv3::event_log_subjects {
  ACTION
}

proc ::hv3::log_window_log {args} {
  if {[string match *ENGINE [lindex $args 0]]} return

  set subject [lindex $args 0]
  if {[lsearch -exact $::hv3::event_log_subjects $subject] >= 0} {
    .event_log.text insert end "$args\n"
  }
}

