#
# Helper procedure that will bring up a dialogue box to edit a value.
# It assumes that the variable whose name is passed to it is the one it
# it to assign a new float value to.  It allows cancellation (returning the
# old value).
#
# Author: Russ Taylor
# Last changed by : Russ Taylor
#

proc newvalue_dialogue {varname} {
	upvar $varname direalvar
	set w .nfd$varname
	global nfdbutton

	toplevel $w -class Dialog
	wm title $w "Set value"

	# Make the two frames, top for the value and bottom for the buttons
	frame $w.top -relief raised -bd 1
	pack $w.top -side top -fill both
	frame $w.bottom -relief raised -bd 1
	pack $w.bottom -side bottom -fill both

	# Make a place to enter the new value, with the old one set there
	global nfdvar
	set nfdvar ""
	entry $w.top.entry -relief sunken -bd 2 -textvariable nfdvar
	pack $w.top.entry

	# Make Set and Cancel buttons.
	# Make set what happens on <Return>
	# Make Cancel what happens on <Escape>
	button $w.bottom.button1 -text "Set" -command "set nfdbutton Set"
	button $w.bottom.button2 -text "Cancel" -command "set nfdbutton Cancel"
	pack $w.bottom.button1 -side left -fill x
	pack $w.bottom.button2 -side right
	bind $w <Return> "$w.bottom.button1 invoke"
	bind $w <Escape> "$w.bottom.button2 invoke"
	bind $w.top.entry <Return> "$w.bottom.button1 invoke"
	bind $w.top.entry <Escape> "$w.bottom.button2 invoke"

	# Focus input on the dialog box and wait for the response
	set oldFocus [focus]
	focus $w.top.entry
	tkwait variable nfdbutton

	# Unfocus the input and either set or reset the variable
	focus $oldFocus
	if {$nfdbutton == "Set"} {
		if {$nfdvar != ""} {    # Don't set it if it's blank.
			set direalvar $nfdvar
		}
	} else {
		# Revert to the old value of the variable (nothing to do)
	}
	destroy $w
}

#
# Helper procedure for floatscale that modifies the global variable when
# the scale changes.  It assumes that it is passed the whole record which
# contains various fields of information that it needs.
#
# This routine does nothing (except reset the skip indicator) if the
# skip_this_change entry has been set.  This indicates that the slider was
# set in response to a change in the variable, rather than vice-versa.
#
# Author: Russ Taylor
# Last changed by : Russ Taylor
#

proc floatscale.scalechanged {name element op} {
	set temp ${name}(realvar); upvar $temp varname; upvar $varname realvar
	set temp ${name}(value); upvar $temp value
	set temp ${name}(min); upvar $temp min
	set temp ${name}(max); upvar $temp max
	set temp ${name}(scale); upvar $temp scale
	set temp ${name}(skip_this_change); upvar $temp skip

	if {$skip} {
		# Nothing to do here
	} else {
		set newval [expr $min + $scale * $value]
		if {$newval > $max} {set newval [expr double($max)]}
		set realvar $newval
	}
	# Don't skip it the next time
	set skip 0
}

#
# This procedure creates a scale that locks itself onto a floating-point
# variable.  When the scale is moved, the variable is updated.  When the
# variable is set, the scale position is not changed, but the label on the
# slider is updated to show the new value.
# If the max or min variable changes value (the routine traces this), the
# slider adjusts its endpoints to match.
# This widget only works to control global variables, maybe.
#
# Author: Russ Taylor
# Last changed by : Russ Taylor
#

proc floatscale {name min max steps labelfirst showrange variable} {
	# Create the parent window to hold the scale and label
	frame $name

	# Create the scale and label subwindows.
	scale $name.scale -from 0 -to [expr $steps - 1] \
		-orient horizontal -showvalue 0 \
		-command "set $name.record(value)"
	uplevel label $name.label -textvariable $variable
	if {$showrange} {
		label $name.minlabel -text $min
		label $name.maxlabel -text $max
	}

	# Set the initial value for the slider if the variable exists and
	# has a value.
	global $variable
	$name.scale set \
	  [expr round( 1.0*([set $variable]-($min))/($max-($min)) * ($steps-1))]

	# Pack them into the parent window.
	if {$labelfirst} {
		pack $name.label -fill x -side top -fill x
	} else {
		pack $name.label -fill x -side bottom -fill x
	}
	if {$showrange} {
		pack $name.minlabel -side left
	}
	pack $name.scale -side left
	if {$showrange} {
		pack $name.maxlabel -side left
	}

	# Squirrel away information that we will need later
	global $name.record
	set $name.record(min) $min
	set $name.record(max) $max
	set $name.record(scale) \
		[expr 1.00001 * double(($max)-($min)) / double($steps-1)]
		# Scaling so that truncation won't make it impossible to reach
		# the highest value
	set $name.record(realvar) $variable
	# Skip the change that happens when the slider is first packed.
	set $name.record(skip_this_change) 1

	# Set up a linkage between the "local" variable and the variable that
	# the slider is to control so that the slider controls it.
	# (Remove any prior trace first, in case this widget has been
	#  destroyed and then restarted with the same name)
	trace vdelete $name.record(value) w floatscale.scalechanged
	trace variable $name.record(value) w floatscale.scalechanged

	# Set up a callback for button 1 pressed within the label that brings
	# up a dialogue box to allow the user to type in the new value
	bind $name.label <Button-1> "newvalue_dialogue $variable"

	# Set up a callback so that if the variable is changed by another
	# (like the dialog box) the slider setting moves to match.
	# Tell the local/remote callback that this it should not reset the
	# global based on this change in the slider.
	proc $name.varchanged {nm element op} "
	    global $name.record
	    global $variable
	    set $name.record(skip_this_change) 1
	    $name.scale set \
	      \[expr round(1.0*(\$$variable-($min))/($max-($min))*($steps-1))\]
	"
	# (Remove any prior trace first, in case this widget has been
	#  destroyed and then restarted with the same name)
	trace vdelete $variable w $name.varchanged
	trace variable $variable w $name.varchanged
}


#
# This creates a pair of sliders with the same end values to control the min
# and max settings (stored in two other variables).
# This widget only works to control global variables, maybe.
#
# Author: Russ Taylor
# Last changed by : Russ Taylor
#

proc minmaxscale {name min max steps var1 var2} {
	# Create the parent frame to hold both of the floatscales
	frame $name

	# Create the two floatscale widgets to live under here
	floatscale $name.min $min $max $steps 0 1 $var1
	floatscale $name.max $min $max $steps 1 1 $var2
	pack $name.max $name.min -side top

	# Make sure that max stays more than min when min is moved
	proc $name.guardmax {nm element op} "
		global $var1 $var2
		if {\$$var1 > \$$var2} { set $var2 \$$var1 }
	"
	global $var1
	# (Remove any prior trace first, in case this widget has been
	#  destroyed and then restarted with the same name)
	trace vdelete $var1 w $name.guardmax
	trace variable $var1 w $name.guardmax

	# Make sure that min stays less than max when max is moved
	proc $name.guardmin {nm element op} "
		global $var1 $var2
		if {\$$var1 > \$$var2} { set $var1 \$$var2 }
	"
	global $var2
	# (Remove any prior trace first, in case this widget has been
	#  destroyed and then restarted with the same name)
	trace vdelete $var2 w $name.guardmin
	trace variable $var2 w $name.guardmin
}

