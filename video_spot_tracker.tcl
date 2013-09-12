#############################################################################
# Sets up the control panel for the Video Spot Tracker program.
# XXX Eventually, it should handle all of the controls.

###########################################################
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
wm geometry .kernel +185+10
frame .kernel.bottom
pack .kernel.bottom -side bottom -fill x

frame .kernel.bottom.state -relief raised -borderwidth 1
pack .kernel.bottom.state -side left
button .kernel.bottom.state.save_state -text "Save State" -command { set save_state 1 }
pack .kernel.bottom.state.save_state -side top -fill x
button .kernel.bottom.state.load_state -text "Load State" -command { set load_state 1 }
pack .kernel.bottom.state.load_state -side top -fill x

frame .kernel.bottom.log -relief raised -borderwidth 1
pack .kernel.bottom.log -side left -fill x -fill y

frame .kernel.options
checkbutton .kernel.options.invert -text dark_spot -variable dark_spot
pack .kernel.options.invert -anchor w
checkbutton .kernel.options.interp -text interpolate -variable interpolate
pack .kernel.options.interp -anchor w
pack .kernel.options -side left

frame .kernel.type -relief raised -borderwidth 1
frame .kernel.type.left
frame .kernel.type.right
frame .kernel.type.top
radiobutton .kernel.type.left.disc -variable kerneltype -text disc -value 0
radiobutton .kernel.type.left.cone -variable kerneltype -text cone -value 1
radiobutton .kernel.type.top.symmetric -variable kerneltype -text symmetric -value 2
radiobutton .kernel.type.top.fiona -variable kerneltype -text FIONA -value 3
radiobutton .kernel.type.right.image -variable kerneltype -text image -value 4 -command update_image_window_visibility
radiobutton .kernel.type.right.imageor -variable kerneltype -text imageor -value 5 -command update_imageor_window_visibility
pack .kernel.type.left.disc -anchor w
pack .kernel.type.left.cone -anchor w
pack .kernel.type.top.symmetric -anchor w
pack .kernel.type.top.fiona -anchor w
pack .kernel.type.right.image -anchor w
pack .kernel.type.right.imageor -anchor w
pack .kernel.type.left -side left
pack .kernel.type.right -side right
pack .kernel.type.top -side top
pack .kernel.type -side left

frame .kernel.options2
checkbutton .kernel.options2.follow_jumps -text follow_jumps -variable follow_jumps
pack .kernel.options2.follow_jumps -anchor w
checkbutton .kernel.options2.predict -text predict -variable predict
pack .kernel.options2.predict -anchor w
checkbutton .kernel.options2.rod3 -text rod3 -variable rod3
pack .kernel.options2.rod3 -anchor w
pack .kernel.options2 -side left

frame .kernel.radius
pack .kernel.radius -side left

frame .kernel.track
frame .kernel.track.top
frame .kernel.track.bottom
frame .kernel.track.top.x -relief raised -borderwidth 1
pack .kernel.track.top.x -side left
label .kernel.track.top.x.label -text X
label .kernel.track.top.x.value -width 10 -textvariable x
pack .kernel.track.top.x.label
pack .kernel.track.top.x.value
frame .kernel.track.top.y -relief raised -borderwidth 1
pack .kernel.track.top.y -side left
label .kernel.track.top.y.label -text Y
label .kernel.track.top.y.value -width 10 -textvariable y
pack .kernel.track.top.y.label
pack .kernel.track.top.y.value
frame .kernel.track.top.z -relief raised -borderwidth 1
pack .kernel.track.top.z -side left
label .kernel.track.top.z.label -text Z
label .kernel.track.top.z.value -width 10 -textvariable z
pack .kernel.track.top.z.label
pack .kernel.track.top.z.value
pack .kernel.track.top -side top
label .kernel.track.bottom.errlabel -text Fitness
pack .kernel.track.bottom.errlabel -side left
label .kernel.track.bottom.err -width 15 -textvariable error
pack .kernel.track.bottom.err -side left
pack .kernel.track.bottom -side top
pack .kernel.track -side left

frame .kernel.optimize
pack .kernel.optimize -side left

# Quit the program if this window is destroyed
bind .kernel <Destroy> {global quit ; set quit 1} 

###########################################################
# Put the places for the controls for going to specific frames.
# This window should only be visible when go_to_frame is turned on.

set go_to_frame_number 0

proc update_goto_window_visibility {args} {
		global go_to_frame_number
		newvalue_dialogue go_to_frame_number
}

###########################################################
# Put the places for the controls for the rod kernels.
# This window should only be visible when rod3 is turned on.

toplevel .rod3
wm geometry .rod3 +860+10
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
# Put the places for the controls for the image-based kernel.
# This window should only be visible when image is turned on.

toplevel .image
wm geometry .image +860+10
wm withdraw .image

proc update_image_window_visibility {args} {
	wm deiconify .image
}

###########################################################
# Put the places for the controls for the oriented image kernel.
# This window should only be visible when imageor is turned on.

toplevel .imageor
wm geometry .imageor +860+10
wm withdraw .imageor

proc update_imageor_window_visibility {args} {
	wm deiconify .imageor
}

###########################################################
# Puts the controls for displaying the tracker and doing
# small area and such.

frame .kernel.bottom.parms -relief raised -borderwidth 1
pack .kernel.bottom.parms -side right
frame .kernel.bottom.parms.left
pack .kernel.bottom.parms.left -side left
checkbutton .kernel.bottom.parms.left.show_tracker -text show_trackers -variable show_tracker
pack .kernel.bottom.parms.left.show_tracker
checkbutton .kernel.bottom.parms.left.round_cursor -text round_cursor -variable round_cursor
pack .kernel.bottom.parms.left.round_cursor
frame .kernel.bottom.parms.right
pack .kernel.bottom.parms.right -side left
checkbutton .kernel.bottom.parms.right.small_area -text small_area -variable small_area
pack .kernel.bottom.parms.right.small_area
checkbutton .kernel.bottom.parms.right.full_area -text full_area -variable full_area
pack .kernel.bottom.parms.right.full_area

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
# Put the place for the controls for handling lost and found trackers.
# This window should only be visible when show_lost_and_found is turned on.

set lost_behavior 0
toplevel .lost_and_found_controls
frame .lost_and_found_controls.behavior -relief raised -borderwidth 1
radiobutton .lost_and_found_controls.behavior.stop -variable lost_behavior -text Stop -value 0
radiobutton .lost_and_found_controls.behavior.delete -variable lost_behavior -text Delete -value 1
radiobutton .lost_and_found_controls.behavior.hover -variable lost_behavior -text Hover -value 2
pack .lost_and_found_controls.behavior
pack .lost_and_found_controls.behavior.stop -side left
pack .lost_and_found_controls.behavior.delete -side left
pack .lost_and_found_controls.behavior.hover -side left
frame .lost_and_found_controls.top -relief raised -borderwidth 1
pack .lost_and_found_controls.top -side top
frame .lost_and_found_controls.middle -relief raised -borderwidth 1
pack .lost_and_found_controls.middle -side left
frame .lost_and_found_controls.bottom -relief raised -borderwidth 1
pack .lost_and_found_controls.bottom -side right
wm withdraw .lost_and_found_controls
set show_lost_and_found 0
trace variable show_lost_and_found w update_lost_and_found_window_visibility

proc update_lost_and_found_window_visibility {nm el op} {
	global show_lost_and_found
	if { $show_lost_and_found } {
		wm deiconify .lost_and_found_controls
	} else {
		wm withdraw .lost_and_found_controls
	}
}

###########################################################
# Put the controls that will let the user store a log file.
# It puts a checkbutton down at the bottom of the main menu
# that causes a dialog box to come up when it is turned on.
# The dialog box fills in a non-empty value into the global
# variable "logfilename" if one is created.  The callback
# clears the variable "logfilename" when logging is turned off.

set logging 0
set logfilename ""
label .kernel.bottom.log.label -textvariable logfilename
pack .kernel.bottom.log.label -side bottom -fill x
trace variable logging w logging_changed
checkbutton .kernel.bottom.log.button -text "Logging" -variable logging -anchor w
pack .kernel.bottom.log.button -side left -fill x
checkbutton .kernel.bottom.log.relative -text "relative to active tracker start" -variable logging_relative -anchor w
pack .kernel.bottom.log.relative -side left -fill x
checkbutton .kernel.bottom.log.withoutopt -text "when not optimizing" -variable logging_without_opt -anchor w
pack .kernel.bottom.log.withoutopt -side left -fill x
checkbutton .kernel.bottom.log.video -text "video" -variable logging_video -anchor w
pack .kernel.bottom.log.video -side left -fill x

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
	} else {
	  set logging 0
	  .kernel.bottom.log.button deselect
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
		
	set types { {"Image Stack Files" "*.avi *.tif *.bmp *.raw *.spe"} }
	set device_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".avi" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a video file to track in"]
	# If we don't have a name, quit.
	if {$device_filename == ""} {
		set quit 1
	} 	
}

###########################################################
# Ask user for the parameters for the raw file that is
# being opened.  Put in defaults for the Pulnix camera.
# 

set raw_params_set 0
proc ask_user_for_raw_file_params { } {
	global raw_params_set quit raw_numx raw_numy raw_bitdepth raw_channels
	global raw_headersize raw_frameheadersize
	set raw_numx 648
	set raw_numy 484
	set raw_bitdepth 8
	set raw_channels 1
	set raw_headersize 0
	set raw_frameheadersize 0

	toplevel .raw
	wm geometry .raw +185+170
	frame .raw.bottom
	pack .raw.bottom -side bottom -fill x

	# Pack the sliders that lets the user set the variables

	label .raw.xlabel -text NumX
	pack .raw.xlabel
	floatscale .raw.numx 0 2048 2048 0 1 raw_numx
	pack .raw.numx

	label .raw.ylabel -text NumY
	pack .raw.ylabel
	floatscale .raw.numy 0 2048 2048 0 1 raw_numy
	pack .raw.numy

	label .raw.headerlabel -text skip_at_file_start
	pack .raw.headerlabel
	floatscale .raw.headersize 0 2048 2048 0 1 raw_headersize
	pack .raw.headersize

	label .raw.frameheaderlabel -text skip_for_each_frame
	pack .raw.frameheaderlabel
	floatscale .raw.frameheadersize 0 2048 2048 0 1 raw_frameheadersize
	pack .raw.frameheadersize

	button .raw.bottom.okay -text "Okay" -command { set raw_params_set 1; wm withdraw .raw }
	pack .raw.bottom.okay -side left -fill x
	button .raw.bottom.quit -text "Quit" -command { set quit 1 }
	pack .raw.bottom.quit -side left -fill x
}


###########################################################
# Ask user for the name of the PSF file they want to open,
# or else set it to "NONE".  The variable to set for the
# name is "psf_filename".

set psf_filename "NONE"
proc ask_user_for_psf_filename { } {
	global psf_filename fileinfo
		
	set types { {"Image Stack Files" "*.avi *.tif *.bmp *.raw *.spe"} }
	set psf_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".tif" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a PSF file for Z tracking"]
	# If we don't have a name, quit.
	if {$psf_filename == ""} {
		set psf_filename "NONE"
	} 	
}

###########################################################
# Ask user for the name of the state file they want to write,
# or else set it to "NONE".  The variable to set for the
# name is "save_state_filename".

set save_state_filename "NONE"
proc ask_user_for_save_state_filename { } {
	global save_state_filename fileinfo
		
	set types { {"State Files" "*.cfg"} }
	set save_state_filename [tk_getSaveFile -filetypes $types \
		-defaultextension ".cfg" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a State file to save"]
	# If we don't have a name, set it to "NONE.
	if {$save_state_filename == ""} {
		set save_state_filename "NONE"
	} 	
}

###########################################################
# Ask user for the name of the state file they want to read,
# or else set it to "NONE".  The variable to set for the
# name is "load_state_filename".

set load_state_filename "NONE"
proc ask_user_for_load_state_filename { } {
	global load_state_filename fileinfo
		
	set types { {"State Files" "*.cfg"} }
	set load_state_filename [tk_getOpenFile -filetypes $types \
		-defaultextension ".cfg" \
		-initialdir $fileinfo(open_dir) \
		-title "Specify a State file to load"]
	# If we don't have a name, set it to "NONE.
	if {$load_state_filename == ""} {
		set load_state_filename "NONE"
	} 	
}
