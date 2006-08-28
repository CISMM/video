#############################################################################
# Sets up the control panel for the Stack Collector program.
# XXX Eventually, it should handle all of the controls.

###########################################################
# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Put in a big "Quit" button at the top of the main window.

button .quit -text "Quit" -command { set quit 1 }
pack .quit -side top -fill x

###########################################################
# Put in a "story-like" interface to describe what happens
# during a stack collection, including the ability to type
# in new values into the particular elements.

frame .story -relief groove -borderwidth 2
pack .story
#wm geometry .story +410+10

frame .story.sec1
pack .story.sec1 -fill x
label .story.sec1.part1 -text "Collect a set of stacks of images"
pack .story.sec1.part1
label .story.sec1.part2 -text "starting"
pack .story.sec1.part2 -side left
entry .story.sec1.part3 -textvariable focus_lower_microns -width 5
pack .story.sec1.part3 -side left
label .story.sec1.part4 -text "microns below focus"
pack .story.sec1.part4 -side left

###########################################################
# Put in a report of where the focus is currently set
# This will track when the user moves the focus, and is
# a viewer but not a controller for the focus value.  This
# is separate from the "focus command", which sends requests
# to the stage, and breaks the infinite loop of command, 
# response from VRPN, send to Tcl, update Tcl, causes command ...

frame .story.sec2
pack .story.sec2 -fill x
label .story.sec2.part2 -text "(currently at"
pack .story.sec2.part2 -side left
label .story.sec2.part3 -textvariable focus_microns
pack .story.sec2.part3 -side left
label .story.sec2.part4 -text "microns"
pack .story.sec2.part4 -side left

frame .story.sec2b
pack .story.sec2b -fill x
label .story.sec2b.part2 -text "with offset of"
pack .story.sec2b.part2 -side left
label .story.sec2b.part3 -textvariable focus_offset_microns
pack .story.sec2b.part3 -side left
label .story.sec2b.part4 -text "microns)"
pack .story.sec2b.part4 -side left

frame .story.sec3
pack .story.sec3 -fill x
label .story.sec3.part2 -text "up to"
pack .story.sec3.part2 -side left
entry .story.sec3.part3 -textvariable focus_raise_microns -width 5
pack .story.sec3.part3 -side left
label .story.sec3.part4 -text "microns above focus"
pack .story.sec3.part4 -side left

frame .story.sec4
pack .story.sec4 -fill x
label .story.sec4.part2 -text "in steps of"
pack .story.sec4.part2 -side left
entry .story.sec4.part3 -textvariable focus_step_microns -width 5
pack .story.sec4.part3 -side left
label .story.sec4.part4 -text "microns."
pack .story.sec4.part4 -side left

frame .story.sec5
pack .story.sec5
label .story.sec5.part1 -text "Take"
pack .story.sec5.part1 -side left
entry .story.sec5.part2 -textvariable images_per_step -width 4
pack .story.sec5.part2 -side left
label .story.sec5.part3 -text "images per step,"
pack .story.sec5.part3 -side left

frame .story.sec6
pack .story.sec6 -fill x
label .story.sec6.part1 -text "exposing for"
pack .story.sec6.part1 -side left
entry .story.sec6.part2 -textvariable exposure_millisecs -width 4
pack .story.sec6.part2 -side left
label .story.sec6.part3 -text "milliseconds,"
pack .story.sec6.part3 -side left

frame .story.sec6b
pack .story.sec6b -fill x
label .story.sec6b.part1 -text "with at least "
pack .story.sec6b.part1 -side left
entry .story.sec6b.part2 -textvariable min_image_wait_millisecs -width 4
pack .story.sec6b.part2 -side left
label .story.sec6b.part3 -text "milliseconds"
pack .story.sec6b.part3 -side left

frame .story.sec6c
pack .story.sec6c -fill x
label .story.sec6c.part1 -text "between successive images."
pack .story.sec6c.part1 -side left

frame .story.sec7
pack .story.sec7
label .story.sec7.part1 -text "Take"
pack .story.sec7.part1 -side left
entry .story.sec7.part2 -textvariable number_of_stacks -width 4
pack .story.sec7.part2 -side left
label .story.sec7.part3 -text "stacks, pausing"
pack .story.sec7.part3 -side left

frame .story.sec8
pack .story.sec8 -fill x
label .story.sec8.part1 -text "at least"
pack .story.sec8.part1 -side left
entry .story.sec8.part2 -textvariable min_repeat_wait_secs -width 4
pack .story.sec8.part2 -side left
label .story.sec8.part3 -text "seconds between."
pack .story.sec8.part3 -side left

###########################################################
# Put the controls that will let the user store a log file.
# It puts a checkbox down at the bottom of the main menu
# that causes a dialog box to come up when it is turned on.
# The dialog box fills in a non-empty value into the global
# variable "logfilename" if one is created.  The callback
# clears the variable "logfilename" when logging is turned off.

set logfilename ""
toplevel .log
wm geometry .log +200+10
label .log.label -textvariable logfilename
pack .log.label -side bottom -fill x
trace variable logging w logging_changed
button .log.button -text "Take stack to files" -command request_stack -anchor w
pack .log.button -side left -fill x
checkbutton .log.sixteen -text "Save 16 bits" -variable save_sixteen_bits -anchor w
pack .log.sixteen -side left -fill x

# Quit the program if this window is destroyed
bind .log <Destroy> {global quit ; set quit 1} 

proc request_stack { } {
    global logging logfilename fileinfo

	set types { {"Stack of TIFFs" "*.*"} }
	set filename [tk_getSaveFile -filetypes $types \
		-initialdir $fileinfo(open_dir) \
		-title "Base name for stack"]
	if {$filename != ""} {
	    # setting this variable triggers a callback in C code
	    # which opens the file.
	    # dialog check whether file exists.
	    set logfilename $filename
	    set fileinfo(open_dir) [file dirname $filename]
	} else {
	    # Blank file name, cancel stack
	    set logfilename ""
	}
}
