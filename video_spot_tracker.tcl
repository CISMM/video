#############################################################################
# Sets up the control panel for the Video Spot Tracker program.
# XXX Eventually, it should handle all of the controls.

# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Put the places for the controls to let the user pick a kernel.

toplevel .kernel
wm geometry .kernel +170+10
frame .kernel.options
checkbutton .kernel.options.invert -text dark_spot -variable dark_spot
pack .kernel.options.invert -anchor w
checkbutton .kernel.options.interp -text interpolate -variable interpolate
pack .kernel.options.interp -anchor w
checkbutton .kernel.options.areamax -text follow_jumps -variable areamax
pack .kernel.options.areamax -anchor w
pack .kernel.options -side left
frame .kernel.type -relief raised -borderwidth 1
radiobutton .kernel.type.disc -variable kerneltype -text disc -value 0
radiobutton .kernel.type.cone -variable kerneltype -text cone -value 1
radiobutton .kernel.type.symmetric -variable kerneltype -text symmetric -value 2
pack .kernel.type.disc -anchor w
pack .kernel.type.cone -anchor w
pack .kernel.type.symmetric -anchor w
pack .kernel.type -side left
frame .kernel.rod3
pack .kernel.rod3 -side left
frame .kernel.radius
pack .kernel.radius -side left
frame .kernel.x -relief raised -borderwidth 1
pack .kernel.x -side left
label .kernel.x.label -text X
label .kernel.x.value -width 10 -textvariable x
pack .kernel.x.label
pack .kernel.x.value
frame .kernel.y -relief raised -borderwidth 1
pack .kernel.y -side left
label .kernel.y.label -text Y
label .kernel.y.value -width 10 -textvariable y
pack .kernel.y.label
pack .kernel.y.value
frame .kernel.optimize
pack .kernel.optimize -side left

# Quit the program if this window is destroyed
bind .kernel <Destroy> {global quit ; set quit 1} 

###########################################################
# Put the places for the controls for the rod kernels.
# This window should only be visible when rod3 is turned on.

toplevel .rod3
wm geometry .rod3 +800+10
wm withdraw .rod3
set rod3 0
trace variable rod3 w update_rod_window_visibility

proc update_rod_window_visibility {nm el op} {
	global rod3
	if { $rod3 } {
		wm deiconify .rod3
	} else {
		wm withdraw .rod3
	}
}

###########################################################
# Put the place for the controls for the clipping.
# This window should only be visible when clipping is turned on.

toplevel .clipping
wm geometry .clipping +800+10
wm withdraw .clipping
set show_clipping 0
trace variable show_clipping w update_clipping_window_visibility

proc update_clipping_window_visibility {nm el op} {
	global show_clipping
	if { $show_clipping } {
		wm deiconify .clipping
	} else {
		wm withdraw .clipping
	}
}

###########################################################
# Put the controls that will let the user store a log file.
# It puts a checkbox down at the bottom of the main menu
# that causes a dialog box to come up when it is turned on.
# The dialog box fills in a non-empty value into the global
# variable "logfilename" if one is created.  The callback
# clears the variable "logfilename" when logging is turned off.

set logging 0
set logfilename ""
toplevel .log
wm geometry .log +170+110
label .log.label -textvariable logfilename
pack .log.label -side bottom -fill x
trace variable logging w logging_changed
checkbutton .log.button -text "Logging to file named below" -variable logging -anchor w
pack .log.button -side left -fill x
checkbutton .log.relative -text "Relative to active tracker start" -variable logging_relative -anchor w
pack .log.relative -side left -fill x

# Quit the program if this window is destroyed
bind .log <Destroy> {global quit ; set quit 1} 

proc logging_changed { varName index op } {
    global logging logfilename fileinfo

    if {$logging == 1} {
	set types { {"Spot tracker log files" "*.vrpn"} }
	set filename [tk_getSaveFile -filetypes $types \
		-defaultextension ".vrpn" \
		-initialdir $fileinfo(open_dir) \
		-title "Name for log file"]
	if {$filename != ""} {
	    # setting this variable triggers a callback in C code
	    # which opens the file.
	    # dialog check whether file exists.
	    set logfilename $filename
	    set fileinfo(open_dir) [file dirname $filename]
	}
    } else {
	set logfilename ""
    }
}

###########################################################
# Ask user for the name of the video file they want to open,
# or else set the quit value.  The variable to set for the
# name is "device_filename".

set device_filename ""
proc ask_user_for_filename { } {
	global device_filename quit fileinfo
		
	set types { {"Image Stack Files" "*.avi *.tif *.bmp .raw *.stk"} }
	set device_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".avi" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a video file to track in"]
	# If we don't have a name, quit.
	if {$device_filename == ""} {
		set quit 1
	} 	
}

