namespace eval hv3 { set {version($Id: hv3_dom.tcl,v 1.50 2007/06/04 08:22:31 danielk1977 Exp $)} 1 }

#--------------------------------------------------------------------------
# Snit types in this file:
#
#     ::hv3::dom                       -- One object per js interpreter. 
#     ::hv3::dom::logdata              -- Data for js debugger
#     ::hv3::dom::logwin               -- Js debugger GUI
#
# Special widgets used in the ::hv3::dom::logwin GUI:
#
#     ::hv3::dom::stacktrace           -- Debugger "Stack" tab
#     ::hv3::dom::searchbox            -- Debugger "Search" tab
#


package require snit


#-------------------------------------------------------------------------
# Class ::hv3::dom
#
#     set dom [::hv3::dom %AUTO% $hv3]
#
#     $dom script   ATTR SCRIPT
#     $dom noscript ATTR SCRIPT
#
#     $dom javascript SCRIPT
#     $dom event EVENT NODE
#     $dom reset
#
#     $dom destroy
#
#  Debugging aids:
#     $dom javascriptlog
#     $dom eventreport NODE
#
#
snit::type ::hv3::dom {
  variable mySee ""

  # Boolean option. Enable this DOM implementation or not.
  option -enable -default 0

  # Variable used to accumulate the arguments of document.write() and
  # document.writeln() invocations from within [script].
  #
  variable myWriteVar ""

  # Document window associated with this scripting environment.
  variable myHv3 ""

  # Javascript window command.
  variable myWindow ""

  variable myNextCodeblockNumber 1

  constructor {hv3 args} {
    set myHv3 $hv3

    set myLogData [::hv3::dom::logdata %AUTO% $self]

    $self configurelist $args
    $self reset
  }

  destructor { 
    $self clear
    catch { $myLogData destroy }
  }


  # Return true if the Tclsee extension is available and 
  # the user has foolishly enabled it.
  method HaveScripting {} {
    return [expr {$options(-enable) && [info commands ::see::interp] ne ""}]
  }

  # Return the javascript object command for the global "window"
  # object supplied to the interpreter.
  method window {} { list ::hv3::DOM::Window $self $myHv3 }

  # Delete any allocated interpreter and objects. This is called in
  # two circumstances: when deleting this object and from within
  # the [reset] method.
  #
  method clear {} {
    # Delete the old interpreter and the various objects, if they exist.
    # They may not exist, if this is being called from within the
    # object constructor or scripting is disabled.
    if {$mySee ne ""} {

      # Delete all of the event-target objects allocated since last 
      # reset. This strategy won't work in the long term (because of
      # AJAX apps that run for a long time), but it's good enough for 
      # now.
      foreach et [array names myEventTargetList] {
        $myEventTargetList($et) destroy
      }
      array unset myEventTargetList

      $mySee destroy
      set mySee ""
    }
  }

  method reset {} {

    $self clear

    if {[$self HaveScripting]} {
      # Set up the new interpreter with the global "Window" object.
      set mySee [::see::interp]
      $mySee global [$self window]

      # Reset the debugger.
      $self LogReset
    }

    # Reset the counter used to name blobs of code. This way, if
    # a page is reloaded the names are the same, which makes it
    # possible to set breakpoints in the debugger before reloading
    # a page. 
    #
    # Of course, the network might deliver external scripts
    # in a different order. Maybe there should be a debugging option
    # to block while downloading all scripts, even if the "defer"
    # attribute is set.
    #
    set myNextCodeblockNumber 1
  }

  # Array used by the [getEventTarget] and [queryEventTarget] methods.
  #
  variable myEventTargetList -array [list]

  # Return the event-target object associated with argument $object,
  # allocating it if necessary . An event-target is a Tcl object 
  # implemented in C to help implement the EventTarget interface 
  # (DOM Level 2 Events). Variable $pIsNew in the parent context is
  # set to true if a new allocation was required, or false otherwise.
  #
  # The $object argument should be one of the following:
  #
  #     * A Tkhtml3 node-handle,
  #     * The literal string "::hv3::DOM::Document", or
  #     * The literal string "::hv3::DOM::Window".
  #
  # All event-target objects are destroyed when the [reset] method is 
  # called. This approach to memory-management will one day cause a 
  # problem for applications that run in web-pages. Worry about that later.
  #
  method getEventTarget {object pIsNew} {
    upvar $pIsNew isNew 
    set isNew false
    if {![info exists myEventTargetList($object)]} {
      set myEventTargetList($object) [$mySee eventtarget]
      set isNew true
    }
    return $myEventTargetList($object)
  }

  # Return the event-target object associated with arguement $object
  # (see description of getEventTarget above), or an empty string if
  # no such event-target has been allocated.
  #
  method queryEventTarget {object} {
    if {![info exists myEventTargetList($object)]} {
      return ""
    }
    return $myEventTargetList($object)
  }

  method NewFilename {} {
    return "blob[incr myNextCodeblockNumber]"
  }
  
  # This method is called as a Tkhtml3 "script handler" for elements
  # of type <SCRIPT>. I.e. this should be registered with the html widget
  # as follows:
  #
  #     $htmlwidget handler script script [list $dom script]
  #
  # This is done externally, not by code within this type definition.
  # If scripting is not enabled in this browser, this method is a no-op.
  #
  method script {attr script} {
    if {$mySee ne ""} {
      $myHv3 write wait
      array set a $attr
      if {[info exists a(src)]} {
        set fulluri [$myHv3 resolve_uri $a(src)]
        set handle [::hv3::download %AUTO%             \
            -uri         $fulluri                      \
            -mimetype    text/javascript               \
            -cachecontrol normal                       \
        ]
        $handle configure -finscript [mymethod scriptCallback $attr $handle]
        $myHv3 makerequest $handle
        # $self Log "Dispatched script request - $handle" "" "" ""
      } else {
        return [$self scriptCallback $attr "" $script]
      }
    }
    return ""
  }

  # Script handler for <noscript> elements. If javascript is enabled,
  # do nothing (meaning don't process the contents of the <noscript>
  # block). On the other hand, if js is disabled, feed the contents
  # of the <noscript> block back to the parser using the same
  # Tcl interface used for document.write().
  #
  method noscript {attr script} {
    if {$mySee ne ""} {
      return ""
    } else {
      [$myHv3 html] write text $script
    }
  }
  
  # If a <SCRIPT> element has a "src" attribute, then the [script]
  # method will have issued a GET request for it. This is the 
  # successful callback.
  #
  method scriptCallback {attr downloadHandle script} {
    if {$downloadHandle ne ""} { 
      $downloadHandle destroy 
    }

    if {$::hv3::dom::reformat_scripts_option} {
      set script [string map {"\r\n" "\n"} $script]
      set script [string map {"\r" "\n"} $script]
      set script [::see::format $script]
    }

    set name [$self NewFilename]
    set rc [catch {$mySee eval -file $name $script} msg]

    set attributes ""
    foreach {a v} $attr {
      append attributes " [htmlize $a]=\"[htmlize $v]\""
    }
    set title "<SCRIPT$attributes>"
    $myLogData Log $title $name $script $rc $msg

    $myHv3 write continue
  }

  method javascript {script} {
    set msg ""
    if {$mySee ne ""} {
      set name [$self NewFilename]
      set rc [catch {$mySee eval -file $name $script} msg]
    }
    return $msg
  }

  # This method is called when one an event of type $event occurs on the
  # document node $node. Argument $event is one of the following:
  #
  #     onload
  #     onsubmit
  #     onchange
  #
  method event {event node} {
    if {$mySee eq ""} {return ""}

    # Strip off the "on", if present. i.e. "onsubmit" -> "submit"
    #
    if {[string first on $event]==0} {set event [string range $event 2 end]}

    if {$event eq "load"} {
      # The Hv3 layer passes the <BODY> node along with the onload
      # event, but DOM Level 0 browsers raise this event against
      # the Window object (the onload attribute of a <BODY> element
      # may be used to set the onload property of the corresponding
      # Window object).
      #
      # According to "DOM Level 2 Events", the load event does not
      # bubble and is not cancelable.
      #
      set js_obj [$self window]
    } else {
      set js_obj [$self node_to_dom $node]
    }

    # Dispatch the event.
    set rc [catch {
      ::hv3::dom::dispatchHtmlEvent $self $event $js_obj
    } msg]

    # If an error occured, log it in the debugging window.
    #
    if {$rc} {
      set name [string map {blob error} [$self NewFilename]]
      $myLogData Log "$node $event event" $name "event-handler" $rc $msg
      $myLogData Popup
      set msg ""
    }

    return $msg
  }

  # This method is called by the ::hv3::mousemanager object to 
  # dispatch a mouse-event into DOM country. Argument $event
  # is one of the following:
  #
  #     onclick 
  #     onmouseout 
  #     onmouseover
  #     onmouseup 
  #     onmousedown 
  #     onmousemove 
  #     ondblclick
  #
  # Argument $node is the Tkhtml node that the mouse-event is 
  # to be dispatched against. $x and $y are the viewport coordinates
  # as returned by the Tk [bind] commands %x and %y directives. Any
  # addition arguments ($args) are passed through to 
  # [::hv3::dom::dispatchMouseEvent].
  #
  method mouseevent {event node x y args} {
    if {$mySee eq ""} {return 1}

    # This can happen if the node is deleted by a DOM event handler
    # invoked by the same logical GUI event as this DOM event.
    #
    if {"" eq [info commands $node]} {return 1}

    if {$node eq ""} {
      set Node [lindex [eval [$self window] Get document] 1]
    } else {
      set Node [$self node_to_dom $node]
    }

    set rc [catch {
      ::hv3::dom::dispatchMouseEvent $self $event $Node $x $y $args
    } msg]
    if {$rc} {
      set name [string map {blob error} [$self NewFilename]]
      $myLogData Log "$node $event event" $name "event-handler" $rc $msg
      $myLogData Popup
    }
  }

  #------------------------------------------------------------------
  # setWindowEvent/getWindowEvent
  #
  #     Get/set the javascript object command that will be returned
  #     for Get requests on the "window.event" property.
  #
  variable myWindowEvent ""
  method setWindowEvent {event} {
    set ret $myWindowEvent
    set myWindowEvent $event
    return $ret
  }
  method getWindowEvent {} {
    return $myWindowEvent
  }

  #------------------------------------------------------------------
  # method node_to_dom
  #
  #     This is a factory method for HTMLElement/Text DOM wrappers
  #     around html widget node-handles.
  #
  method node_to_dom {node} {
    ::hv3::dom::createWidgetNode $self $node
  }

  #----------------------------------------------------------------
  # Given an html-widget node-handle, return the corresponding 
  # ::hv3::DOM::HTMLDocument object. i.e. the thing needed for
  # the Node.ownerDocument javascript property of an HTMLElement or
  # Text Node.
  #
  method node_to_document {node} {
    # TODO: Fix this for when there are other Document objects than
    # the main one floating around. i.e. This will fail if the node-handle
    # comes from a different <FRAME> than the script is being executed
    # in.
    #
    lindex [eval [$self window] Get document] 1
  }

  #----------------------------------------------------------------
  # Given an html-widget node-handle, return the corresponding 
  # ::hv3::hv3 object. i.e. the owner of the node-handle.
  #
  method node_to_hv3 {node} {
    # TODO: Same fix as for [node_to_document] is required.
    return $myHv3
  }

  method see    {} { return $mySee }
  method hv3    {} { return $myHv3 }

  #------------------------------------------------------------------
  # Logging system follows.
  #
  variable myLogData

  # This variable contains the current javascript debugging log in HTML 
  # form. It is appended to by calls to [Log] and truncated to an
  # empty string by [LogReset]. If the debugging window is displayed,
  # the contents are identical to that of this variable.
  #
  variable myLogDocument ""

  method Log {heading script rc result} {
    $myLogData Log $heading $script $rc $result
    return
  }


  method LogReset {} {
    $myLogData Reset
    return
  }

  method javascriptlog {} {
    $myLogData Popup
    return
  }

  # Called by the tree-browser to get event-listener info for the
  # javascript object associated with the specified tkhtml node.
  #
  method eventdump {node} {
    if {$mySee eq ""} {return ""}
    set Node [$self node_to_dom $node]
    return "TODO"
  }
}

#-----------------------------------------------------------------------
# ::hv3::dom::logdata
# ::hv3::dom::logscript
#
#     Javascript debugger state.
#
# ::hv3::dom::logwin
#
#     Toplevel window widget that implements the javascript debugger.
#
snit::type ::hv3::dom::logscript {
  option -rc      -default ""
  option -heading -default "" 
  option -script  -default "" 
  option -result  -default "" 
  option -name    -default "" 
}

snit::type ::hv3::dom::logdata {

  # Handle for associated (owner) ::hv3::dom object.
  #
  variable myDom ""

  # Ordered list of ::hv3::dom::logscript objects. The scripts 
  # that make up the application being debugged.
  #
  variable myLogScripts [list]

  # Name of top-level debugging GUI window. At any time, this window
  # may or may not exist. This object is responsible for creating
  # it when it is required.
  #
  variable myWindow ""

  # Array of breakpoints set in the script. The array key is
  # "${zFilename}:${iLine}". i.e. to test if there is a breakpoint
  # in file "blob4" line 10, do:
  #
  #     if {[info exists myBreakpoints(blob4:10)]} {
  #         ... breakpoint processing ...
  #     }
  #
  variable myBreakpoints -array [list]

  variable myInReload 0

  constructor {dom} {
    set myDom $dom
    set myWindow ".[string map {: _} $self]_logwindow"
  }

  method Log {heading name script rc result} {
    set ls [::hv3::dom::logscript %AUTO% \
      -rc $rc -name $name -heading $heading -script $script -result $result
    ]
    lappend myLogScripts $ls
  }

  method dom {} {
    return $myDom
  }

  method Reset {} {
    foreach ls $myLogScripts {
      $ls destroy
    }
    set myLogScripts [list]

    if {$myInReload} {
      if {[llength [array names myBreakpoints]] > 0} {
        [$myDom see] trace [mymethod SeeTrace]
      }
    } else {
      array unset myBreakpoints
    }
  }

  method Popup {} {
    if {![winfo exists $myWindow]} {
      ::hv3::dom::logwin $myWindow $self
    } 
    wm state $myWindow normal
    raise $myWindow
    $myWindow Populate
  }

  destructor {
    $self Reset
  }


  method GetList {} {
    return $myLogScripts
  }

  method Evaluate {script} {
    set res [$myDom javascript $script]
    return $res
  }

  method BrowseToNode {node} {
    ::HtmlDebug::browse [$myDom hv3] $node
  }

  method SeeTrace {eEvent zFilename iLine} {
    if {$eEvent eq "statement"} {
      if {[info exists myBreakpoints(${zFilename}:$iLine)]} {
        # There is a breakpoint set on this line. Pop up the
        # debugging GUI and set the breakpoint stack.
        $self Popup
        $myWindow Breakpoint $zFilename $iLine
      }
    }
  }

  method breakpoints {} {
    return [array names myBreakpoints]
  }

  method add_breakpoint {blobid lineno} {
    set myBreakpoints(${blobid}:${lineno}) 1
    [$myDom see] trace [mymethod SeeTrace]
  }

  method clear_breakpoint {blobid lineno} {
    unset -nocomplain myBreakpoints(${blobid}:${lineno})
    if {[llength [$self breakpoints]]==0} {
      [$myDom see] trace ""
    }
  }

  # Called by the GUI when the user issues a "reload" command.
  # The difference between this and clicking the reload button
  # in the GUI is that breakpoints are persistent when this
  # command is issued.
  #
  method reload {} {
    set myInReload 1
    gui_current reload
    set myInReload 0
  }
}

snit::widget ::hv3::dom::searchbox {

  variable myLogwin 

  constructor {logwin} {
    ::hv3::label ${win}.label
    ::hv3::scrolled listbox ${win}.listbox

    set myLogwin $logwin

    pack ${win}.label   -fill x
    pack ${win}.listbox -fill both -expand true

    ${win}.listbox configure -background white
    bind ${win}.listbox <<ListboxSelect>> [mymethod Select]
  }

  method Select {} {
    set idx  [lindex [${win}.listbox curselection] 0]
    set link [${win}.listbox get $idx]
    set link [string range $link 0 [expr [string first : $link] -1]]
    $myLogwin GotoCmd -silent $link
    ${win}.listbox selection set $idx
  }

  method Search {str} {
    ${win}.listbox delete 0 end

    set nHit 0
    foreach ls [$myLogwin GetList] {
      set blobid [$ls cget -name]
      set iLine 0
      foreach line [split [$ls cget -script] "\n"] {
        incr iLine
        if {[string first $str $line]>=0} {
          ${win}.listbox insert end "$blobid $iLine: $line"
          incr nHit
        }
      }
    }

    return $nHit
  }
}

snit::widget ::hv3::dom::stacktrace {

  variable myLogwin ""

  constructor {logwin} {
    ::hv3::label ${win}.label
    ::hv3::scrolled listbox ${win}.listbox

    set myLogwin $logwin

    pack ${win}.label -fill x
    pack ${win}.listbox -fill both -expand true

    ${win}.listbox configure -background white
    bind ${win}.listbox <<ListboxSelect>> [mymethod Select]
  }

  method Select {} {
    set idx  [lindex [${win}.listbox curselection] 0]
    set link [${win}.listbox get $idx]
    $myLogwin GotoCmd -silent $link
    ${win}.listbox selection set $idx
  }

  method Populate {title stacktrace} {
    ${win}.label configure -text $title
    ${win}.listbox delete 0 end
    foreach {blobid lineno calltype callname} $stacktrace {
      ${win}.listbox insert end "$blobid $lineno"
    }
  }
}

#-------------------------------------------------------------------------
# ::hv3::dom::logwin --
#
#     Top level widget for the debugger window.
#
snit::widget ::hv3::dom::logwin {
  hulltype toplevel

  # Internal widgets from left-hand pane:
  #
  variable myFileList ""                            ;# Listbox with file-list
  variable mySearchbox ""                           ;# Search results
  variable myStackList ""                           ;# Stack trace widget.

  # The text widget used to display code and the label displayed above it.
  #
  variable myCode ""
  variable myCodeTitle ""

  # The two text widgets: One for input and one for output.
  #
  variable myInput ""
  variable myOutput ""

  # ::hv3::dom::logdata object containing the debugging
  # data for the DOM environment being debugged.
  #
  variable myData

  # Index in [$myData GetList] of the currently displayed file:
  #
  variable myCurrentIdx 0

  # Current point in stack trace. Tag this line with "tracepoint"
  # in the $myCode widget.
  #
  variable myTraceFile ""
  variable myTraceLineno ""

  variable myStack ""

  constructor {data} {
    
    set myData $data

    panedwindow ${win}.pan -orient horizontal
    panedwindow ${win}.pan.right -orient vertical

    set nb [::hv3::tile_notebook ${win}.pan.left]
    set myFileList [::hv3::scrolled listbox ${win}.pan.left.files]

    set mySearchbox [::hv3::dom::searchbox ${win}.pan.left.search $self]
    set myStackList [::hv3::dom::stacktrace ${win}.pan.left.stack $self]

    $nb add $myFileList  -text "Files"
    $nb add $mySearchbox -text "Search"
    $nb add $myStackList -text "Stack"

    $myFileList configure -bg white
    bind $myFileList <<ListboxSelect>> [mymethod PopulateText]

    frame ${win}.pan.right.top 
    set myCode [::hv3::scrolled ::hv3::text ${win}.pan.right.top.code]
    set myCodeTitle [::hv3::label ${win}.pan.right.top.label -anchor w]
    pack $myCodeTitle -fill x
    pack $myCode -fill both -expand 1
    $myCode configure -bg white

    $myCode tag configure linenumber -foreground darkblue
    $myCode tag configure tracepoint -background skyblue
    $myCode tag configure stackline  -background wheat
    $myCode tag configure breakpoint -background red

    $myCode tag bind linenumber <1> [mymethod ToggleBreakpoint %x %y]

    frame ${win}.pan.right.bottom 
    set myOutput [::hv3::scrolled ::hv3::text ${win}.pan.right.bottom.output]
    set myInput  [::hv3::text ${win}.pan.right.bottom.input -height 3]
    $myInput configure -bg white
    $myOutput configure -bg white -state disabled
    $myOutput tag configure commandtext -foreground darkblue

    # Set up key bindings for the input window:
    bind $myInput <Return> [list after idle [mymethod Evaluate]]
    bind $myInput <Up>     [list after idle [mymethod HistoryBack]]
    bind $myInput <Down>   [list after idle [mymethod HistoryForward]]

    pack $myInput -fill x -side bottom
    pack $myOutput -fill both -expand 1

    ${win}.pan add ${win}.pan.left -width 200
    ${win}.pan add ${win}.pan.right

    ${win}.pan.right add ${win}.pan.right.top  -height 200 -width 600
    ${win}.pan.right add ${win}.pan.right.bottom -height 350

    pack ${win}.pan -fill both -expand 1

    bind ${win} <Escape> [list destroy ${win}]

    focus $myInput
    $myInput insert end "help"
    $self Evaluate
  }

  method GetList {} { return [$myData GetList] }

  method Populate {} {
    $myFileList delete 0 end

    # Populate the "Files" list-box.
    #
    foreach ls [$myData GetList] {
      set name    [$ls cget -name] 
      set heading [$ls cget -heading] 
      set rc   [$ls cget -rc] 

      $myFileList insert end "$name - $heading"

      if {$name eq $myTraceFile} {
        $myFileList selection set end
      }
  
      if {$rc} {
        $myFileList itemconfigure end -foreground red -selectforeground red
      }
    }
  }

  method PopulateText {} {
    $myCode configure -state normal
    $myCode delete 0.0 end
    set idx [lindex [$myFileList curselection] 0]
    if {$idx ne ""} {
      set ls [lindex [$myData GetList] $idx]
      $myCodeTitle configure -text [$ls cget -heading]

      set name [$ls cget -name]
      set stacklines [list]
      foreach {n l X Y} $myStack {
        if {$n eq $name} { lappend stacklines $l }
      }

      set script [$ls cget -script]
      set N 1
      foreach line [split $script "\n"] {
        $myCode insert end [format "% 5d   " $N] linenumber
        if {[$ls cget -name] eq $myTraceFile && $N == $myTraceLineno} {
          $myCode insert end "$line\n" tracepoint
        } elseif {[lsearch $stacklines $N] >= 0} {
          $myCode insert end "$line\n" stackline
        } else {
          $myCode insert end "$line\n"
        }
        incr N
      }
      set myCurrentIdx $idx
    }

    # The following line throws an error if no chars are tagged "tracepoint".
    catch { $myCode yview -pickplace tracepoint.first }
    $myCode configure -state disabled
    $self PopulateBreakpointTag
  }

  # This proc is called after either:
  #
  #     * The contents of the code window change, or
  #     * A breakpoint is added or cleared from the debugger.
  #
  # It ensures the "breakpoint" tag is set on the line-number
  # of any line that has a breakpoint set.
  #
  method PopulateBreakpointTag {} {
    set ls     [lindex [$myData GetList] $myCurrentIdx]
    set blobid [$ls cget -name]

    $myCode tag remove breakpoint 0.0 end
    foreach key [$myData breakpoints] {
      foreach {b i} [split $key :] {}
      if {$b eq $blobid} {
        $myCode tag add breakpoint ${i}.0 ${i}.5
      }
    }
  }

  method ToggleBreakpoint {x y} {
    set ls     [lindex [$myData GetList] $myCurrentIdx]
    set lineno [lindex [split [$myCode index @${x},${y}] .] 0]
    set blobid [$ls cget -name]

    if {[lsearch [$myData breakpoints] "${blobid}:${lineno}"]>=0} {
      $myData clear_breakpoint ${blobid} ${lineno}
    } else {
      $myData add_breakpoint ${blobid} ${lineno}
    }
    $self PopulateBreakpointTag
  }

  # This is called when the user issues a [result] command.
  #
  method ResultCmd {cmd} {
    # The (optional) argument is one of the blob names. If
    # it is not specified, use the currently displayed js blob 
    # as a default.
    #
    set arg [lindex $cmd 0]
    if {$arg eq ""} {
      set idx $myCurrentIdx
      if {$idx eq ""} {set idx [llength [$myData GetList]]}
    } else {
      set idx 0
      foreach ls [$myData GetList] {
        if {[$ls cget -name] eq $arg} break
        incr idx
      }
    }
    set ls [lindex [$myData GetList] $idx]
    if {$ls eq ""} {
      error "No such file: \"$arg\""
    }

    # Echo the command to the output panel.
    #
    $myOutput insert end "result: [$ls cget -name]\n" commandtext

    $myOutput insert end "   rc       : " commandtext
    $myOutput insert end "[$ls cget -rc]\n"
    if {[$ls cget -rc] && [lindex [$ls cget -result] 0] eq "JS_ERROR"} {
      set res [$ls cget -result]
      set msg        [lindex $res 1]
      set zErrorInfo [lindex $res 2]
      set stacktrace [lrange $res 3 end]
      set stack ""
      foreach {file lineno a b} $stacktrace {
        set stack "-> $file:$lineno $stack"
      }
      $myOutput insert end "   result   : " commandtext
      $myOutput insert end "$msg\n"
      $myOutput insert end "   stack    : " commandtext
      $myOutput insert end "[string range $stack 3 end]\n"
      $myOutput insert end "   errorInfo: " commandtext
      $myOutput insert end "$zErrorInfo\n"

      set blobid ""
      set r [$ls cget -result]
      regexp {(blob[[:digit:]]*):([[:digit:]]*):} $r X blobid lineno

      if {$blobid ne ""} {
        set stacktrace [linsert $stacktrace 0 $blobid $lineno "" ""]
      }

      if {[llength $stacktrace] > 0} {
        set blobid [lindex $stacktrace 0]
        set lineno [lindex $stacktrace 1]
        set myStack $stacktrace
        $myStackList Populate "Result of [$ls cget -name]:" $stacktrace
        [winfo parent $myStackList] select $myStackList
      }

      if {$blobid ne ""} {
        $self GotoCmd -silent [list $blobid $lineno]
      }
      
    } else {
      $myOutput insert end "   result: [$ls cget -result]\n"
    }
  }
 
  # This is called when the user issues a [clear] command.
  #
  method ClearCmd {cmd} {
    $myOutput delete 0.0 end
  }

  # This is called when the user issues a [javascript] command.
  #
  method JavascriptCmd {cmd} {
    set js [string trim $cmd]
    set res [$myData Evaluate $js]
    $myOutput insert end "javascript: $js\n" commandtext
    $myOutput insert end "    [string trim $res]\n"
  }

  method GotoCmd {cmd {cmd2 ""}} {
    if {$cmd2 ne ""} {set cmd $cmd2}
    set blobid [lindex $cmd 0]
    set lineno [lindex $cmd 1]
    if {$lineno eq ""} {
      set lineno 1
    }

    if {$cmd2 eq ""} {
      $myOutput insert end "goto: $blobid $lineno\n" commandtext
    }

    set idx 0
    set ok 0
    foreach ls [$myData GetList] {
      if {[$ls cget -name] eq $blobid} {set ok 1 ; break}
      incr idx
    }

    if {!$ok} {
      $myOutput insert end "        No such blob: $blobid"
      return
    }

    set myTraceFile $blobid
    set myTraceLineno $lineno
    if {$cmd2 eq ""} {
      $self Populate
    } else {
      $myFileList selection clear 0 end
      $myFileList selection set $idx
    }
    $self PopulateText
  }

  method ErrorCmd {cmd} {
  }

  method SearchCmd {cmd} {
    set n [$mySearchbox Search $cmd]
    ${win}.pan.left select $mySearchbox
    $myOutput insert end "search: $cmd\n" commandtext
    $myOutput insert end "    $n hits.\n"
  }

  method TreeCmd {node} {
    $myOutput insert end "tree: $node\n" commandtext
    $myData BrowseToNode $node
  }

  method TclCmd {args} {
    $myOutput insert end "tcl: $args\n" commandtext
    set res [eval uplevel #0 $args]
    $myOutput insert end "    $res\n" 
  }

  method ReloadCmd {args} {
    $myData reload
  }

  method DebugCmd {args} {
    $myOutput insert end "debug: $args\n" commandtext
    set see [[$myData dom] see] 
    switch -- [lindex $args 0] {

      alloc {
        set data [eval $see debug $args]
        foreach {key value} $data {
          set key [format "    % -30s" $key]
          $myOutput insert end $key commandtext 
          $myOutput insert end ": $value\n"
        }
      }

      default {
        set objlist [eval [[$myData dom] see] debug $args]
        foreach obj $objlist {
          $myOutput insert end "    $obj\n"
        }
      }
    }
  }

  method Evaluate {} {
    set script [string trim [$myInput get 0.0 end]]
    $myInput delete 0.0 end
    $myInput mark set insert 0.0

    set idx [string first " " $script]
    if {$idx < 0} {
      set idx [string length $script]
    }
    set zWord [string range $script 0 [expr $idx-1]]
    set nWord [string length $zWord]

    $myOutput configure -state normal

    set cmdlist [list \
      clear      2 ""                     ClearCmd      \
      cont       2 ""                     ContCmd       \
      goto       1 "BLOBID ?LINE-NUMBER?" GotoCmd       \
      js         1 "JAVASCRIPT..."        JavascriptCmd \
      debug      1 "SUB-COMMAND"          DebugCmd      \
      reload     2 ""                     ReloadCmd     \
      result     1 "BLOBID"               ResultCmd     \
      search     1 "STRING"               SearchCmd     \
      tree       2 "NODE"                 TreeCmd       \
      tcl        2 "TCL-SCRIPT"           TclCmd        \
    ]
    set done 0
    foreach {cmd nMin NOTUSED method} $cmdlist {
      if {$nWord>=$nMin && [string first $zWord $cmd]==0} {
        $self $method [string trim [string range $script $nWord end]]
        set done 1
        break
      }
    }
    
    if {!$done} {
        # An unknown command (or "help"). Print a summary of debugger commands.
        #
        $myOutput insert end "help:\n" commandtext
        foreach {cmd NOTUSED help NOTUSED} $cmdlist {
        $myOutput insert end "          $cmd $help\n"
      }
      set x "Unambiguous prefixes of the above commands are also accepted.\n"
      $myOutput insert end "        $x"
    }

    $myOutput yview -pickplace end
    $myOutput insert end "\n"
    $myOutput configure -state disabled

    $self HistoryAdd $script
  }


  # History list for input box. Interface is:
  #
  #     $self HistoryAdd $script       (add command to end of history list)
  #     $self HistoryBack              (bind to <back> key>)
  #     $self HistoryForward           (bind to <forward> key)
  #
  variable myHistory [list]
  variable myHistoryIdx 0
  method HistoryAdd {script} {
    lappend myHistory $script
    set myHistoryIdx [llength $myHistory]
  }
  method HistoryBack {} {
    if {$myHistoryIdx==0} return
    incr myHistoryIdx -1
    $myInput delete 0.0 end
    $myInput insert end [lindex $myHistory $myHistoryIdx]
  }
  method HistoryForward {} {
    if {$myHistoryIdx==[llength $myHistory]} return
    incr myHistoryIdx
    $myInput delete 0.0 end
    $myInput insert end [lindex $myHistory $myHistoryIdx]
  }
  #
  # End of history list for input box.


  # Variable used for breakpoint code.
  variable myBreakpointVwait
  method ContCmd {args} { 
    $myOutput insert end "cont:\n" commandtext
    set myBreakpointVwait 1 

    set myTraceFile ""
    set myTraceLineno ""
    set myStack ""
    $myStackList Populate "" $myStack
    $myCode tag remove tracepoint 0.0 end
    $myCode tag remove stackline 0.0 end
  }

  # This is called when the SEE interpreter has been stopped at
  # a breakpoint. Js execution will continue after this method 
  # returns.
  #
  method Breakpoint {zBlobid iLine} {
    set myBreakpointVwait 0

    set myStack [list $zBlobid $iLine "" ""]
    $myStackList Populate "Breakpoint $zBlobid $iLine" $myStack
    [winfo parent $myStackList] select $myStackList
    $self GotoCmd -silent [list $zBlobid $iLine]

    $myOutput configure -state normal
    $myOutput insert end "breakpoint: $zBlobid $iLine\n\n" commandtext
    $myOutput configure -state disabled

    vwait [myvar myBreakpointVwait]
  }
}
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# Pull in the object definitions.
#
source [file join [file dirname [info script]] hv3_dom_compiler.tcl]

foreach f [list \
  hv3_dom_containers.tcl \
  hv3_dom_core.tcl \
  hv3_dom_html.tcl \
  hv3_dom_style.tcl \
  hv3_dom_ns.tcl \
  hv3_dom_events.tcl \
  hv3_dom_xmlhttp.tcl \
] {
  source [file join [file dirname [info script]] $f]
}
set classlist [concat \
  HTMLCollection HTMLElement HTMLDocument \
  [::hv3::dom::getHTMLElementClassList]   \
  [::hv3::dom::getNSClassList]            \
  Text NodePrototype NodeList             \
  CSSStyleDeclaration                     \
  Event MouseEvent                        \
]
foreach c $classlist {
  eval [::hv3::dom2::compile $c]
}
#puts [::hv3::dom2::compile Window]

::hv3::dom2::cleanup

#-----------------------------------------------------------------------
# Initialise the scripting environment. This tries to load the javascript
# interpreter library. If it fails, then we have a scriptless browser. 
# The test for whether or not the browser is script-enabled is:
#
#     if {[info commands ::see::interp] ne ""} {
#         puts "We have scripting :)"
#     } else {
#         puts "No scripting here. Probably better that way."
#     }
#
proc ::hv3::dom::init {} {
  # Load the javascript library.
  #
  catch { load [file join tclsee0.1 libTclsee.so] }
  catch { package require Tclsee }
}
::hv3::dom::init

set ::hv3::dom::reformat_scripts_option 0
#set ::hv3::dom::reformat_scripts_option 1

