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

