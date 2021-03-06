#############################################################################
# Sets up the control panels for the CISMM Video Optimizer program.

# Global variable to remember where they are saving files.
set fileinfo(open_dir) "C:\\"

###########################################################
# Put in a big "Quit" button at the top of the main window.

button .quit -text "Quit" -command { set quit 1 }
pack .quit -side top -fill x

###########################################################
# Put in a radiobutton to select the color channel

frame .colorpick -relief raised -borderwidth 1
radiobutton .colorpick.r -variable red_green_blue -text R -value 0
radiobutton .colorpick.g -variable red_green_blue -text G -value 1
radiobutton .colorpick.b -variable red_green_blue -text B -value 2
pack .colorpick
pack .colorpick.r -side left
pack .colorpick.g -side left
pack .colorpick.b -side left

###########################################################
# Put the places for the controls to let the user pick a kernel.

toplevel .kernel
wm geometry .kernel +195+10
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

# Hide the kernel control window if the tracker is hidden.
trace variable show_tracker w update_kernel_window_visibility

proc update_kernel_window_visibility {nm el op} {
	global show_tracker
	if { $show_tracker } {
		wm deiconify .kernel
	} else {
		wm withdraw .kernel
	}
}

###########################################################
# Put the place for the controls for the clipping.
# This window should only be visible when clipping is turned on.

toplevel .clipping
wm geometry .clipping +810+10
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
# Put the place for the controls for the contrast/gain.
# This window should only be visible when gain_control is turned on.

toplevel .gain
wm geometry .gain +195+10
wm withdraw .gain
set show_gain_control 0
frame .gain.low
pack .gain.low -side left
frame .gain.high
pack .gain.high -side left
checkbutton .gain.auto -text auto -variable auto_gain
pack .gain.auto -side left

trace variable show_gain_control w update_gain_window_visibility

proc update_gain_window_visibility {nm el op} {
	global show_gain_control
	if { $show_gain_control } {
		wm deiconify .gain
	} else {
		wm withdraw .gain
	}
}

###########################################################
# Put the place for the controls for the image mixtures.
# This window should only be visible when imagemix_control is turned on.

toplevel .imagemix
wm geometry .imagemix +610+10
wm withdraw .imagemix
frame .imagemix.display
pack .imagemix.display -side left
radiobutton .imagemix.display.computed -variable display -text show_computed -value 0
pack .imagemix.display.computed -anchor w
radiobutton .imagemix.display.min -variable display -text show_min -value 1
pack .imagemix.display.min -anchor w
radiobutton .imagemix.display.max -variable display -text show_max -value 2
pack .imagemix.display.max -anchor w
radiobutton .imagemix.display.mean -variable display -text show_mean -value 3
pack .imagemix.display.mean -anchor w
set show_imagemix_control 0
frame .imagemix.subtract
pack .imagemix.subtract -side left
radiobutton .imagemix.subtract.none -variable subtract -text subtract_none -value 0
pack .imagemix.subtract.none -anchor w
radiobutton .imagemix.subtract.min -variable subtract -text subtract_min -value 1
pack .imagemix.subtract.min -anchor w
radiobutton .imagemix.subtract.max -variable subtract -text subtract_max -value 2
pack .imagemix.subtract.max -anchor w
radiobutton .imagemix.subtract.mean -variable subtract -text subtract_mean -value 3
pack .imagemix.subtract.mean -anchor w
radiobutton .imagemix.subtract.single -variable subtract -text subtract_single -value 4
pack .imagemix.subtract.single -anchor w
radiobutton .imagemix.subtract.neighbors -variable subtract -text subtract_neighbors -value 5
pack .imagemix.subtract.neighbors -anchor w
frame .imagemix.statistics
pack .imagemix.statistics -side left

trace variable show_imagemix_control w update_imagemix_window_visibility

proc update_imagemix_window_visibility {nm el op} {
	global show_imagemix_control
	if { $show_imagemix_control } {
		wm deiconify .imagemix
	} else {
		wm withdraw .imagemix
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
wm geometry .log +195+130
trace variable logging w logging_changed
checkbutton .log.button -text "Logging to file sequence named " -variable logging -anchor w
pack .log.button -side left -fill x
label .log.label -textvariable logfilename
pack .log.label -side left -fill x
checkbutton .log.psf -text "Log Point-spread" -variable pointspread_log -anchor w
pack .log.psf -side right -fill x
checkbutton .log.sixteenbits -text "Log 16 bits" -variable sixteenbit_log -anchor w
pack .log.sixteenbits -side right -fill x
checkbutton .log.monochrome -text "Monochrome" -variable monochrome_log -anchor w
pack .log.monochrome -side right -fill x

# Quit the program if this window is destroyed
bind .log <Destroy> {global quit ; set quit 1} 

proc logging_changed { varName index op } {
    global logging logfilename fileinfo

    if {$logging == 1} {
	set types { {"CISMM Video Optimizer TIF files" "*.*"} }
	set filename [tk_getSaveFile -filetypes $types \
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
# Ask user for the name of the video file they want to optimize,
# or else set the quit value.  The variable to set for the
# name is "device_filename".

set device_filename ""
proc ask_user_for_filename { } {
	global device_filename quit fileinfo
		
	set types { {"Image Stack Files" "*.avi *.tif *.bmp *.raw"} }
	set device_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".avi" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a video file to optimize"]
	# If we don't have a name, quit.
	if {$device_filename == ""} {
		set quit 1
	} 	
}
