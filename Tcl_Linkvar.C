#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	"Tcl_Linkvar.h"

#include <tcl.h>
#include <tk.h>

static	const int MAX_INTS =	1000;	// Max number of int variables
static	const int MAX_IB =	1000;	// Max number of int/buttons
static	const int MAX_FLOATS =	1000;	// Max number of float variables
static	const int MAX_FS =	1000;	// Max number of floatscale vars
static	const int MAX_SELS =	1000;	// Max number of selectors

static	int	num_ints = 0;			// Number of int variables
static	Tclvar_int	*int_list[MAX_INTS];	// Int variables

static	int	num_ib = 0;			// Number of ints with buttons
static	Tclvar_int_with_button	*ib_list[MAX_IB];// IB variables

static	int	num_floats = 0;			// Number of float variables
static	Tclvar_float	*float_list[MAX_FLOATS];// Float variables

static	int	num_fs = 0;			// Number of floats with scales
static	Tclvar_float_with_scale	*fs_list[MAX_FS];// Floatscale variables

static	int	num_sels = 0;			// Number of selectors
static	Tclvar_selector		*sel_list[MAX_SELS];

static	Tcl_Interp	*interpreter = NULL;	// Tcl interpreter used

// DO NOT USE commands such as Tcl_GetVar2 (with the 2 at the end)
// if we use Tcl_GetVar instead, we wil be able to link with array elements 
// in TCL. 

//	Update the integer variable in the handler that is pointed to
// when the variables changes.

static	char	*handle_int_value_change(ClientData clientData,
	Tcl_Interp *interp, char * /*name1*/, char * /*name2*/, int /*flags*/)
{
        char    *cvalue;
	int	value;
	Tclvar_int	*intvar = (Tclvar_int*)(clientData);

	// Look up the new value of the variable
	cvalue = Tcl_GetVar(interp, intvar->my_tcl_varname,
		 TCL_GLOBAL_ONLY);
	if (cvalue == NULL) {
		fprintf(stderr,"Warning!  Can't read %s from Tcl\n",
			intvar->my_tcl_varname);
	} else if (Tcl_GetInt(interp, cvalue, &value) != TCL_OK) {
		fprintf(stderr,"Warning!  %s not int from Tcl\n",
			intvar->my_tcl_varname);
	}
	intvar->myint = intvar->mylastint = value;

	// Yank the callback if it has been set
	if (intvar->callback) {
		intvar->callback(value, intvar->userdata);
	}

	return NULL;
};

//	Update the float variable in the handler that is pointed to
// when the variables changes.

static	char	*handle_float_value_change(ClientData clientData,
	Tcl_Interp *interp, char * /*name1*/, char * /*name2*/, int /*flags*/)
{
        char    *cvalue;
	double	value;
	Tclvar_float	*floatvar = (Tclvar_float*)(clientData);

// 	printf("floatvalchange: %s %s %s\n", name1, name2, floatvar->my_tcl_varname);
	// Look up the new value of the variable
	cvalue = Tcl_GetVar(interp, floatvar->my_tcl_varname, 
		 TCL_GLOBAL_ONLY);
	if (cvalue == NULL) {
		fprintf(stderr,"Warning!  Can't read %s from Tcl\n",
			floatvar->my_tcl_varname);
	} else if (Tcl_GetDouble(interp, cvalue, &value) != TCL_OK) {
		fprintf(stderr,"Warning!  %s not double from Tcl\n",
			floatvar->my_tcl_varname);
	}
	floatvar->myfloat = floatvar->mylastfloat = (float)(value);

	// Yank the callback if it has been set
	if (floatvar->callback) {
		floatvar->callback((float)(value), floatvar->userdata);
	}

	return NULL;
};

//	Update the string variable in the handler that is pointed to
// when the variables changes.

static	char	*handle_string_value_change(ClientData clientData,
	Tcl_Interp *interp, char * /*name1*/, char * /*name2*/, int /*flags*/)
{
        char    *cvalue;
	Tclvar_selector	*selvar = (Tclvar_selector*)(clientData);

	// Look up the new value of the variable
	cvalue = Tcl_GetVar(interp, selvar->my_tcl_varname, 
		 TCL_GLOBAL_ONLY);
	if (cvalue == NULL) {
		fprintf(stderr,"Warning!  Can't read %s from Tcl\n",
			selvar->my_tcl_varname);
	}
	strncpy(selvar->mystring, cvalue, TCLVAR_STRING_LENGTH);
	selvar->mystring[TCLVAR_STRING_LENGTH-1] = 0;
	strncpy(selvar->mylaststring, cvalue, TCLVAR_STRING_LENGTH);
	selvar->mylaststring[TCLVAR_STRING_LENGTH-1] = 0;

	// Yank the callback if it has been set
	if (selvar->callback) {
		selvar->callback(cvalue, selvar->userdata);
	}

	return NULL;
};

//	Set the interpreter to be used by the integers and floats.
//	Put together links for all of the variables that have been created
// before the interpreter was set.
//	Return 0 on success, -1 on failure.

int	Tclvar_init(Tcl_Interp *tcl_interp)
{
	int	i;
	char	cvalue[100];

//fprintf(stderr, "Tclvar_init() called\n");

	// Set the interpreter
	interpreter = tcl_interp;

	// Insert callbacks for the integer variables
	for (i = 0; i < num_ints; i++) {
		// initialize the tcl variable with the value from the
		// C variable
		sprintf(cvalue, "%d", int_list[i]->myint);
		Tcl_SetVar(interpreter, int_list[i]->my_tcl_varname,
			   cvalue, TCL_GLOBAL_ONLY);
		if (Tcl_TraceVar(interpreter, int_list[i]->my_tcl_varname,
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_int_value_change,
			(ClientData)((void*)(int_list[i]))) != TCL_OK) {
			fprintf(stderr, "Tcl_TraceVar(%s) failed: %s\n",
				int_list[i]->my_tcl_varname,
				interpreter->result);
			return(-1);
		}
	}

	// Insert callbacks for the float variables
	for (i = 0; i < num_floats; i++) {
		// initialize the tcl variable with the value from the
		// C variable
		sprintf(cvalue, "%g", float_list[i]->myfloat);
		Tcl_SetVar(interpreter, float_list[i]->my_tcl_varname,
			   cvalue, TCL_GLOBAL_ONLY);
		if (Tcl_TraceVar(interpreter, float_list[i]->my_tcl_varname,
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_float_value_change,
			(ClientData)((void*)(float_list[i]))) != TCL_OK) {
			fprintf(stderr, "Tcl_TraceVar(%s) failed: %s\n",
				float_list[i]->my_tcl_varname,
				interpreter->result);
			return(-1);
		}

	}

	// Set up the integer with buttons.
	for (i = 0; i < num_ib; i++) {
	    if (ib_list[i]->tcl_widget_name != NULL) {
		if (ib_list[i]->initialize(interpreter)) return(-1);
	    }
	}

	// Set up the floatscales.
	for (i = 0; i < num_fs; i++) {
	    if (fs_list[i]->tcl_widget_name != NULL) {
		if (fs_list[i]->initialize(interpreter)) return(-1);
	    }
	}

	// Insert callbacks for the selector variables
	// Set up the selectors.
	for (i = 0; i < num_sels; i++) {
            if (sel_list[i]->my_tcl_varname)  // TCH  6 April 98
		if (Tcl_TraceVar(interpreter, sel_list[i]->my_tcl_varname,
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_string_value_change,
			(ClientData)((void*)(sel_list[i]))) != TCL_OK) {
			fprintf(stderr, "Tcl_TraceVar(%s) failed: %s\n",
				sel_list[i]->my_tcl_varname,
				interpreter->result);
			return(-1);
		}
		if (sel_list[i]->tcl_widget_name != NULL) {
			if (sel_list[i]->initialize(interpreter)) return(-1);
		}
	}

	return	0;

}

int	Tclvar_mainloop(void)
{
	int	i;
	char	cvalue[100];

	// Make sure there is a valid interpeter
	if (interpreter == NULL) {
		return -1;
	}

	// Check for changes in the C integers and update the Tcl variables
	// when they occur.
	for (i = 0; i < num_ints; i++) {
		if (int_list[i]->myint != int_list[i]->mylastint) {
			int_list[i]->mylastint = int_list[i]->myint;
			sprintf(cvalue,"%d", int_list[i]->myint);
			Tcl_SetVar(interpreter, int_list[i]->my_tcl_varname,
				cvalue, TCL_GLOBAL_ONLY);
		}
	}

	// Check for changes in the C floats and update the Tcl variables
	// when they occur.
	for (i = 0; i < num_floats; i++) {
		if (float_list[i]->myfloat != float_list[i]->mylastfloat) {
			float_list[i]->mylastfloat = float_list[i]->myfloat;
			sprintf(cvalue,"%g", float_list[i]->myfloat);
			Tcl_SetVar(interpreter, float_list[i]->my_tcl_varname,
				cvalue, TCL_GLOBAL_ONLY);
		}
	}

	// Check for changes in the C selectors and update the Tcl variables
	// when they occur.
	for (i = 0; i < num_sels; i++) {
		if (strcmp(sel_list[i]->mystring,sel_list[i]->mylaststring)) {
			strncpy(sel_list[i]->mylaststring,sel_list[i]->mystring, TCLVAR_STRING_LENGTH);
			sel_list[i]->mylaststring[TCLVAR_STRING_LENGTH-1] = 0;
			Tcl_SetVar(interpreter, sel_list[i]->my_tcl_varname,
				sel_list[i]->mystring, TCL_GLOBAL_ONLY);
		}
	}

	return 0;
}

//	Add an entry into the list of active integer Tcl variables, and
// point it to this variable.
//	Add a Tcl callback, if an interpreter has been declared.

Tclvar_int::Tclvar_int(char *tcl_varname, int default_value,
	Linkvar_Intcall c, void *ud)
{
	callback = c;
	userdata = ud;

//fprintf(stderr, "Tclvar_int constructor\n");

	if (num_ints >= (MAX_INTS-1)) {
		fprintf(stderr,"Tclvar_int::Tclvar_int(): Can't link to %s\n",
			tcl_varname);
		fprintf(stderr,"                          (Out of storage)\n");
	} else {
		int_list[num_ints] = this;
		my_tcl_varname = new char[strlen(tcl_varname)+1];
		if (my_tcl_varname == NULL) {
			fprintf(stderr,"Tclvar_int::Tclvar_int(): Out of memory\n");
		} else {
			strcpy(my_tcl_varname, tcl_varname);
			myint = mylastint = default_value;
			num_ints++;
		}
	}

	// Add a callback for change if there is an interpreter
	if (interpreter != NULL) {
		if (Tcl_TraceVar(interpreter, my_tcl_varname,
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_int_value_change,
			(ClientData)((void*)(this))) != TCL_OK) {
			fprintf(stderr, "Tclvar_int::Tclvar_int(): Tcl_TraceVar(%s) failed: %s\n",
				my_tcl_varname, interpreter->result);
		}
	}
};

//	Remove the entry from the list of active integer Tcl variables.

Tclvar_int::~Tclvar_int()
{
	register int	i = 0;

	// Remove the trace callback, if there is an interpreter
	if (interpreter != NULL) {
		Tcl_UntraceVar(interpreter, my_tcl_varname,
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_int_value_change,
			(ClientData)((void*)(this)));
	}

	// Free the space for the name
	if (my_tcl_varname != NULL) {delete [] my_tcl_varname; }

	// Find this entry in the list of variables
	while ( (i < num_ints) && (int_list[i] != this) ) { i++; };
	if (i >= num_ints) {
		fprintf(stderr,"Tclvar_int::~Tclvar_int(): Internal error -- not in list\n");
		return;
	}

	// Move the last entry to this one
	int_list[i] = int_list[num_ints-1];

	// Reduce the number in the list
	num_ints--;
};

//	Add an entry into the list of active float Tcl variables, and
// point it to this variable.
//	Add a trace callback if there is an interpreter.

Tclvar_float::Tclvar_float(char *tcl_varname, float default_value,
	Linkvar_Floatcall c, void *ud)
{
	callback = c;
	userdata = ud;

//fprintf(stderr, "Tclvar_float constructor\n");

	if (num_floats >= (MAX_FLOATS-1)) {
		fprintf(stderr,"Tclvar_float::Tclvar_float(): Can't link to %s\n",
			tcl_varname);
		fprintf(stderr,"                          (Out of storage)\n");
	} else {
		float_list[num_floats] = this;
		my_tcl_varname = new char[strlen(tcl_varname)+1];
		if (my_tcl_varname == NULL) {
			fprintf(stderr,"Tclvar_float::Tclvar_float(): Out of memory\n");
		} else {
			strcpy(my_tcl_varname, tcl_varname);
			myfloat = mylastfloat = default_value;
			num_floats++;
		}
	}

        // Add a callback for change if there is an interpreter
        if (interpreter != NULL) {
                if (Tcl_TraceVar(interpreter, my_tcl_varname,
                        TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
                        handle_float_value_change,
			(ClientData)((void*)this)) != TCL_OK) {
                        fprintf(stderr, "Tclvar_float::Tclvar_float(): Tcl_TraceVar(%s) failed: %s\n",
                                my_tcl_varname, interpreter->result);
                }
        }
};

//	Remove the entry from the list of active float Tcl variables.

Tclvar_float::~Tclvar_float()
{
	register int	i = 0;

	// Remove the trace callback, if there is an interpreter
	if (interpreter != NULL) {
		Tcl_UntraceVar(interpreter, my_tcl_varname, 
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_float_value_change,
			(ClientData)((void*)this));
	}

	// Free the space for the name
	if (my_tcl_varname != NULL) { delete [] my_tcl_varname; }

	// Find this entry in the list of variables
	while ( (i < num_floats) && (float_list[i] != this) ) { i++; };
	if (i >= num_floats) {
		fprintf(stderr,"Tclvar_float::~Tclvar_float(): Internal error -- not in list\n");
		return;
	}

	// Move the last entry to this one
	float_list[i] = float_list[num_floats-1];

	// Reduce the number in the list
	num_floats--;
};

//	Add a trace callback and handler widget if there is an interpreter.

Tclvar_int_with_button::Tclvar_int_with_button(char *tcl_varname,
	char *parent_name, int default_value, Linkvar_Intcall c, void *ud) :
	Tclvar_int(tcl_varname, default_value, c, ud)
{
//fprintf(stderr, "Tclvar_int_with_button constructor\n");

	// Add this to the list of integers with buttons
	if (num_ib >= (MAX_IB-1)) {
		fprintf(stderr,"Tclvar_int_with_button::Tclvar_int_with_button(): Can't link to %s\n",
			tcl_varname);
		fprintf(stderr,"                          (Out of storage)\n");
	} else {
		ib_list[num_ib] = this;
		my_tcl_varname = new char[strlen(tcl_varname)+1];
		if (my_tcl_varname == NULL) {
			fprintf(stderr,"Tclvar_int_with_button::Tclvar_int_with_button(): Out of memory\n");
		} else {
			strcpy(my_tcl_varname, tcl_varname);
			myint = mylastint = default_value;
			num_ib++;
		}
	}

	// Set the widget name, if there is a parent specified
	// The widget name matches the tcl_varname, unless there are '.'s
	// in the name.  If there are '.'s, the name is just the part after
	// the last '.' (this strips away the prepended widget part).
	if (parent_name != NULL) {
		char	*last_part;

		last_part = strrchr(tcl_varname, '.');
		if (last_part == NULL) {
			last_part = tcl_varname;
		} else {
			last_part++;	// Skip the .
		}
		tcl_widget_name =
			new char[strlen(parent_name) + strlen(last_part) + 2];
		if (tcl_widget_name == NULL) {
			fprintf(stderr,"Tclvar_int_with_button::Tclvar_int_with_button(): Out of memory\n");
		}
		sprintf(tcl_widget_name,"%s.%s",parent_name,last_part);

		// Make sure widget name starts with a lower-case character.
		tcl_widget_name[strlen(parent_name)+1] =
		  tolower(tcl_widget_name[strlen(parent_name)+1]);
	} else {
		tcl_widget_name = NULL;
	}

	// Add and pack a checkbutton if there is an interpreter and a widget
	if ( (interpreter != NULL) && (tcl_widget_name != NULL) ) {
		initialize(interpreter);
	}
};

Tclvar_int_with_button::~Tclvar_int_with_button()
{
	register int	i = 0;

	// Unpack and destroy the checkbutton, if there is an
	// interpreter
	if (interpreter != NULL) {
		char	command[1000];
		sprintf(command,"destroy %s",tcl_widget_name);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
		}
	}

	// Free the space for the widget name
	if (tcl_widget_name != NULL) { delete [] tcl_widget_name; }

	// Find this entry in the list of variables
	while ( (i < num_ib) && (ib_list[i] != this) ) { i++; };
	if (i >= num_ib) {
		fprintf(stderr,"Tclvar_int_with_button::~Tclvar_int_with_button(): Internal error -- not in list\n");
		return;
	}

	// Move the last entry to this one
	ib_list[i] = ib_list[num_ib-1];

	// Reduce the number in the list
	num_ib--;
};


// Add and pack a checkbutton if there is an interpreter and a widget
// for each intwithbutton variable.  Note that the change callback will
// be handled by the fact that it is derived from an int var.
// Set the value of the variable so it shows up on the display.

int	Tclvar_int_with_button::initialize(Tcl_Interp *interpreter)
{
	char	command[1000];
	char	cvalue[100];
	char	*last_part;

	// Set the variable to its default value
	sprintf(cvalue,"%d", myint);
	Tcl_SetVar(interpreter, my_tcl_varname, cvalue, TCL_GLOBAL_ONLY);

	// Create the checkbutton.  Only use the truncated name for the
	// text in the widget (the part of the variable name after the
	// last '.').
	last_part = strrchr(my_tcl_varname, '.');
	if (last_part == NULL) {
		last_part = my_tcl_varname;
	} else {
		last_part++;	// Skip the .
	}
	sprintf(command,"checkbutton {%s} -text {%s} -variable {%s} -anchor w",
		tcl_widget_name, last_part, my_tcl_varname);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Pack the checkbutton
	sprintf(command,"pack {%s} -fill x\n", tcl_widget_name);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	return 0;
}


//	Add an entry into the list of active floatscale Tcl variables, and
// point it to this variable.  Set up the min and max floats.
//	Add a trace callback and handler widget if there is an interpreter.

Tclvar_float_with_scale::Tclvar_float_with_scale(char *tcl_varname,
	char *parent_name, float minval, float maxval, float default_value,
	Linkvar_Floatcall c, void *ud) :
	Tclvar_float(tcl_varname, default_value, c, ud)
{
//fprintf(stderr, "Tclvar_float_with_scale constructor\n");

	minvalue = minval;
	maxvalue = maxval;

	if (num_fs >= (MAX_FS-1)) {
		fprintf(stderr,"Tclvar_float_with_scale::Tclvar_float_with_scale(): Can't link to %s\n",
			tcl_varname);
		fprintf(stderr,"                          (Out of storage)\n");
	} else {
		fs_list[num_fs] = this;
		my_tcl_varname = new char[strlen(tcl_varname)+1];
		if (my_tcl_varname == NULL) {
			fprintf(stderr,"Tclvar_float_with_scale::Tclvar_float_with_scale(): Out of memory\n");
		} else {
			strcpy(my_tcl_varname, tcl_varname);
			myfloat = mylastfloat = default_value;
			num_fs++;
		}
	}

	// Set the widget and label names, if there is a parent specified
	if (parent_name != NULL) {
		tcl_widget_name =
			new char[strlen(parent_name) + strlen(tcl_varname) + 2];
		if (tcl_widget_name == NULL) {
			fprintf(stderr,"Tclvar_float_with_scale::Tclvar_float_with_scale(): Out of memory\n");
		}
		sprintf(tcl_widget_name,"%s.%s",parent_name,tcl_varname);

		tcl_label_name =
			new char[strlen(parent_name) + strlen(tcl_varname) +12];
		if (tcl_label_name == NULL) {
			fprintf(stderr,"Tclvar_float_with_scale::Tclvar_float_with_scale(): Out of memory\n");
		}
		sprintf(tcl_label_name,"%s.%slabel",parent_name,tcl_varname);
	} else {
		tcl_widget_name = NULL;
	}

	// Add and pack a floatscale if there is an interpreter and a widget
	if ( (interpreter != NULL) && (tcl_widget_name != NULL) ) {
		initialize(interpreter);
	}
};

//	Remove the entry from the list of active float Tcl variables.
//	Base class destructor handles the rest.

Tclvar_float_with_scale::~Tclvar_float_with_scale()
{
	register int	i = 0;

	// Unpack and destroy the floatscale and label, if there is an
	// interpreter
	if (interpreter != NULL) {
		char	command[1000];
		sprintf(command,"destroy %s",tcl_widget_name);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
		}
		sprintf(command,"destroy %s",tcl_label_name);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
		}
	}

	// Free the space for the widget name
	if (tcl_widget_name != NULL) { delete [] tcl_widget_name; }
	if (tcl_label_name != NULL) { delete [] tcl_label_name; }

	// Find this entry in the list of variables
	while ( (i < num_fs) && (fs_list[i] != this) ) { i++; };
	if (i >= num_fs) {
		fprintf(stderr,"Tclvar_float_with_scale::~Tclvar_float_with_scale(): Internal error -- not in list\n");
		return;
	}

	// Move the last entry to this one
	fs_list[i] = fs_list[num_fs-1];

	// Reduce the number in the list
	num_fs--;
};

// Add and pack a floatscale if there is an interpreter and a widget
// for each floatscale variable.  Note that the change callback will
// be handled by the fact that it is derived from a float var.
// Set the value of the variable so it shows up on the display.
// Put in a label with the variable's name

int	Tclvar_float_with_scale::initialize(Tcl_Interp *interpreter)
{
	char	command[1000];
	char	cvalue[100];

	// Set the variable to its default value
	sprintf(cvalue,"%g", myfloat);
	Tcl_SetVar(interpreter, my_tcl_varname, cvalue, TCL_GLOBAL_ONLY);

	// Create the floatscale
	sprintf(command,"floatscale %s %g %g 300 0 1 %s",
		tcl_widget_name, minvalue, maxvalue, my_tcl_varname);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Make the label for the floatscale
	sprintf(tcl_label_name,"%slabel",tcl_widget_name);
	sprintf(command,"label %s -text %s", tcl_label_name, my_tcl_varname);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Pack the label and floatscale
	sprintf(command,"pack %s %s\n", tcl_label_name, tcl_widget_name);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	return 0;
}

Tclvar_list_of_strings::~Tclvar_list_of_strings()
{
	// Delete all of the entries in the list.
	// num_entries will be adjusted by each delete, which makes the
	// loop control look strange.
	while (num_entries)
		Delete_entry(entries[0]);
	// TCH 26 June 98
	// better cleanup (for linux?)
	while (num_selectors)
		Delete_selector(selectors[0]);
}

// Returns 0 on success, 1 on list full, 2 on entry already in the list,
// -1 on other errors.
int	Tclvar_list_of_strings::Add_entry(const char entry[])
{
	int	i;

	// Make sure there is enough room for another entry
	if (num_entries == 500) {
		return(1);
	}

	// Make sure the entry is not already in the list
	for (i = 0; i < num_entries; i++) {
	  if (strncmp(entries[i], entry, TCLVAR_STRING_LENGTH) == 0) {
		return 2;
	  }
	}

	// Add the entry on the end of the list
	strncpy(entries[num_entries], entry, TCLVAR_STRING_LENGTH);
	entries[num_entries][TCLVAR_STRING_LENGTH-1] = 0;
	num_entries++;

//fprintf(stderr, "Added \"%s\".\n", entry);

	// If we have an interpreter and a widget, add the entry into the
	// menu for each of the selectors that are interested in this list
	if (interpreter) {
	  for (i = 0; i < num_selectors; i++) {
	    if (selectors[i]->tcl_widget_name) {
		char	command[1000];
		Tclvar_selector	*sel = selectors[i];

//fprintf(stderr, "  Updating %s with the new entry.\n", sel->my_tcl_varname);

		sprintf(command,"%s.menu add command -label {%s} -underline 0 -command \"set %s {%s}\"", sel->tcl_widget_name, entries[num_entries-1], sel->my_tcl_varname, entries[num_entries-1]);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
			return(-1);
		}
	    }
	  }
	}

	return 0;
}

// Returns 0 on success, 1 on string not found in list, -1 on other error
int	Tclvar_list_of_strings::Delete_entry(const char entry[])
{
	int	i, s;
	int	retval = 0;

	// Find the entry in the list, if we can
	for (i = 0; i < num_entries; i++) {
		if (strncmp(entries[i], entry, TCLVAR_STRING_LENGTH) == 0) {
			break;
		}
	}
	if (i == num_entries) {	// Didn't find it!
		return 1;
	}

	// If we have an interpreter, remove the entry from the menu for each
	// of the selectors that are interested in this list.
	if (interpreter) {
	  for (s = 0; s < num_selectors; s++) {
		char	command[1000];
		Tclvar_selector	*sel = selectors[s];

                // TCH 30 Sep 97:  added the "if (sel->tcl_widget_name)"
                // since Add_entry has the same and the lack here was causing
                // Tcl errors
                if (sel->tcl_widget_name) {
		  sprintf(command, "%s.menu delete {%s}",
                          sel->tcl_widget_name, entries[i]);
		  if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
			retval = -1;
		  }
                }
	  }
	}

	// Remove the entry from the list by moving the last entry to its
	// location and reducing the number of entries by one
	strncpy(entries[i], entries[num_entries-1], TCLVAR_STRING_LENGTH);
	entries[i][TCLVAR_STRING_LENGTH-1] = 0;
	num_entries--;

	return retval;
}

// Returns 0 on success, 1 on list full, 2 on entry already in the list,
// -1 on other errors.
int Tclvar_list_of_strings::Add_selector(Tclvar_selector *sel)
{
	int	i;

	// Make sure there is enough room for another entry
	if (num_selectors == 500) {
		return(1);
	}

	// Make sure the entry is not already in the list
	for (i = 0; i < num_selectors; i++) {
	  if (selectors[i] == sel) {
		return 2;
	  }
	}

//if (sel->my_tcl_varname)
//fprintf(stderr, "Added selector \"%s\".\n", sel->my_tcl_varname);
//else
//fprintf(stderr, "Added anonymous selector.\n");

	// Add the entry on the end of the list
	selectors[num_selectors] = sel;
	num_selectors++;

	return 0;
}

// Returns 0 on success, 1 on selector not found in list, -1 on other error
int	Tclvar_list_of_strings::Delete_selector(Tclvar_selector *sel)
{
	int	i;

	if (!sel) return 1;

	// Find the entry in the list, if we can
	for (i = 0; i < num_selectors; i++) {
		if (selectors[i] == sel) {
			break;
		}
	}
	if (i == num_selectors) {	// Didn't find it!
		return 1;
	}

	// Remove the entry from the list by moving the last entry to its
	// location and reducing the number of entries by one
	selectors[i] = selectors[num_selectors - 1];
	num_selectors--;

	// TCH 26 June 98
	// better cleanup (for linux?)
	sel->mylist = NULL;

	return 0;
}

//	Creates a selector, linking it to the list of strings that it
// uses to select from.  Adds the selector to the list of selectors,
// so if we don't have an interpreter yet, it will be called when the
// interpreter is set.
//	If we do have an interpreter, go ahead and build the menu for
// the variable and do all of the cross-linking to ensure that updates
// will happen.

Tclvar_selector::Tclvar_selector (const char * initial_value) :
        my_tcl_varname (NULL),
        tcl_widget_name (NULL),
        tcl_label_name (NULL),
        callback (NULL),
        userdata (NULL),
        mylist (NULL),
        d_initialized (0) {

  if (num_sels >= (MAX_SELS-1)) {
    fprintf(stderr, "Tclvar_selector:  "
                    "Can't link to nameless variable\n");
    fprintf(stderr,"                          (Out of storage)\n");
    return;
  }

  sel_list[num_sels] = this;
  num_sels++;

  Set(initial_value);
}


Tclvar_selector::Tclvar_selector (char * tcl_varname, char * parent_name,
	Tclvar_list_of_strings * list, char * initial_value,
        Linkvar_Selectcall c, void *ud) :
    mylist (NULL),
    tcl_widget_name(NULL),
    tcl_label_name(NULL),
    d_initialized (0)
{
	callback = c;
	userdata = ud;

//fprintf(stderr, "Tclvar_selector constructor\n");

	tcl_widget_name = NULL;

	// Put us on the list and set the Tcl variable name and parent name.
	if (num_sels >= (MAX_SELS-1)) {
		fprintf(stderr,"Tclvar_selector::Tclvar_selector(): Can't link to %s\n",
			tcl_varname);
		fprintf(stderr,"                          (Out of storage)\n");
	} else {
		sel_list[num_sels] = this;
		Set(initial_value);
		num_sels++;
		initializeTcl(tcl_varname, parent_name);

	        // Store which list of strings we are to use
		// NULL list is legal and expected now,
		// but we want to call this only if it's non-NULL.
	        if (list) bindList(list);
	}

}

void	Tclvar_selector::Set (const char *value)
{
	strncpy(mystring, value, TCLVAR_STRING_LENGTH);
	mystring[TCLVAR_STRING_LENGTH-1] = 0;
	strncpy(mylaststring, value, TCLVAR_STRING_LENGTH);
	mylaststring[TCLVAR_STRING_LENGTH-1] = 0;
}

Tclvar_selector::~Tclvar_selector (void)
{
	int	i = 0;

	// Remove the trace callback, if there is an interpreter
	if (interpreter) {
		Tcl_UntraceVar(interpreter, my_tcl_varname, 
			TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
			handle_string_value_change,
			(ClientData)((void*)(this)));
	}

	// Free the space for the name
	if (my_tcl_varname != NULL) { delete [] my_tcl_varname; }

	// Unpack and destroy the menu and label, if there is an
	// interpreter and a widget
	if (interpreter && tcl_widget_name) {
		char	command[1000];
		sprintf(command,"destroy %s",tcl_widget_name);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
		}
		sprintf(command,"destroy %s",tcl_label_name);
		if (Tcl_Eval(interpreter, command) != TCL_OK) {
			fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
				interpreter->result);
		}
	}

	// Free the space for the widget name
	if (tcl_widget_name != NULL) { delete [] tcl_widget_name; }
	if (tcl_label_name != NULL) { delete [] tcl_label_name; }

	// Remove this selector from its list of strings
        if (mylist)
	  mylist->Delete_selector(this);

	// Find this entry in the list of variables
	while ( (i < num_sels) && (sel_list[i] != this) ) { i++; };
	if (i >= num_sels) {
		fprintf(stderr,"Tclvar_selector::~Tclvar_selector(): Internal error -- not in list\n");
		return;
	}

	// Move the last entry to this one
	sel_list[i] = sel_list[num_sels-1];

	// Reduce the number in the list
	num_sels--;
}


void Tclvar_selector::initializeTcl (const char * tcl_varname,
                                     const char * parent_name) {

  if (!tcl_varname) {
    fprintf(stderr, "Tclvar_selector::initializeTcl:  "
                    "NULL variable name!\n");
    return;
  }

  my_tcl_varname = new char [strlen(tcl_varname) + 1];
  if (my_tcl_varname == NULL) {
    fprintf(stderr,"Tclvar_selector:  Out of memory\n");
    return;
  } 
  strcpy(my_tcl_varname, tcl_varname);

  // Set the widget and label names, if there is a parent specified
  if (parent_name) {
    tcl_widget_name =
            new char [strlen(parent_name) + strlen(tcl_varname) + 2];
    if (tcl_widget_name == NULL) {
      fprintf(stderr,"Tclvar_selector:  Out of memory\n");
      return;
    }
    sprintf(tcl_widget_name,"%s.%s",parent_name,tcl_varname);

    tcl_label_name =
      new char[strlen(parent_name) + strlen(tcl_varname) +12];
    if (tcl_label_name == NULL) {
      fprintf(stderr,"Tclvar_selector:  Out of memory\n");
      return;
    }
    sprintf(tcl_label_name,"%s.%slabel",parent_name,tcl_varname);
  } else {
    tcl_widget_name = NULL;
  }

  // Add a callback for change if there is an interpreter
  if (interpreter) {
    if (Tcl_TraceVar(interpreter, my_tcl_varname,
                  TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
                  handle_string_value_change,
                  (ClientData)((void*)(this))) != TCL_OK) {
      fprintf(stderr, "Tclvar_selector:  Tcl_TraceVar(%s) failed: %s\n",
              my_tcl_varname, interpreter->result);
      return;
    }
  }

  // Build the menu for this selector, if there is an interpreter
  // and a widget name
  if (interpreter && tcl_widget_name) initialize(interpreter);
}

int Tclvar_selector::bindList (Tclvar_list_of_strings * list) {
  int ret = 0;

  if (mylist) {
    fprintf(stderr, "Tclvar_selector::bindList:  Calling twice.\n");
    return -1;
  }

  if (!list) {
    fprintf(stderr, "Tclvar_selector::bindList:  "
                    "Calling with empty input.\n");
    return -1;
  }

  // Set the list of strings used, and insert this selector into
  // the ones the list knows is interested in it
  mylist = list;
  list->Add_selector(this);

  if (d_initialized)
    ret = initializeList();

  return ret;
}




//	Create the menu for the selector, placing it in the parent.
// Use the list of strings to fill in the menu choices, each of which
// will set the variable to its value when selected.
int	Tclvar_selector::initialize(Tcl_Interp *interpreter)
{
	char	command[1000];
	int ret;

	// Set the variable to its default value
	Tcl_SetVar(interpreter, my_tcl_varname, mystring, TCL_GLOBAL_ONLY);

	// Create the menu button that invokes the menu
	sprintf(command,
	    "menubutton %s -textvariable %s -bd 2 -relief raised -menu %s.menu",
	    tcl_widget_name, my_tcl_varname, tcl_widget_name);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Cause button 3 to bring up a dialog box, in case the menu gets
	// so long that you can't pick all of its entries
	sprintf(command,
	    "bind %s <Button-3> { newvalue_dialogue %s }",
	    tcl_widget_name, my_tcl_varname);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Create the menu and fill it with the list of strings
	sprintf(command,"menu %s.menu", tcl_widget_name);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

        if (mylist) {
          ret = initializeList();
          if (ret < 0)
            return -1;
        }


	// Make the label for the menu
	sprintf(tcl_label_name,"%slabel",tcl_widget_name);
	sprintf(command,"label %s -text %s", tcl_label_name, my_tcl_varname);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	// Pack the label and menu
	sprintf(command,"pack %s %s\n", tcl_label_name, tcl_widget_name);
	if (Tcl_Eval(interpreter, command) != TCL_OK) {
		fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
			interpreter->result);
		return(-1);
	}

	d_initialized = 1;

	return 0;
}

int Tclvar_selector::initializeList (void) {
  char command [1000];
  int i;

  for (i = 0; i < mylist->num_entries; i++) {
    sprintf(command, "%s.menu add command -label {%s} -underline 0 "
                     "-command \"set %s {%s}\"",
            tcl_widget_name, mylist->entries[i],
            my_tcl_varname, mylist->entries[i]);
    if (Tcl_Eval(interpreter, command) != TCL_OK) {
      fprintf(stderr, "Tcl_Eval(%s) failed: %s\n", command,
              interpreter->result);
      return(-1);
    }
  }

  return 0;
}

char	*Tclvar_selector::operator =(char *v)
{
	if (v == NULL) {
		mystring[0] = '\0';
	} else {
		strncpy(mystring, v, TCLVAR_STRING_LENGTH);
		mystring[TCLVAR_STRING_LENGTH-1] = 0;
	}
	return mystring;
}


//-----------------------------------------------------------------------
// Checklist class.  First is listed the type of the data sent to the callback
// and the callback handler for the entries in each list.

typedef	struct {
	Tclvar_checklist	*whos;
	Tclvar_Check_Entry	*which;
} Check_Parm;

static	void	checklist_callback(int value, void *userdata)
{
	Check_Parm	*parm = (Check_Parm*)(userdata);

	value = value;	// Keep the compiler happy
	if (parm->whos->callback != NULL) {
		parm->whos->callback(parm->which->name,
				     (int)(*(parm->which->button)),
				     parm->whos->userdata);
	}
}

Tclvar_checklist::Tclvar_checklist(char *parent_name)
{
	// Set the defaults
	num_entries = 0;
	callback = NULL;
	userdata = NULL;

//fprintf(stderr, "Tclvar_checklist constructor\n");

	// Set the parent name.
	if (parent_name) {
		tcl_parent_name = new char[strlen(parent_name) + 1];
		if (tcl_parent_name == NULL) {
		  fprintf(stderr,
		    "Tclvar_checklist::Tclvar_checklist(): Out of memory\n");
		  return;
		}
		sprintf(tcl_parent_name,"%s",parent_name);
	} else {
		tcl_parent_name = NULL;
	}
};

Tclvar_checklist::~Tclvar_checklist()
{
	int	i = 0;

	// Free the space for the name
	if (tcl_parent_name != NULL) { delete [] tcl_parent_name; }

	// Destroy all of the entries
	for (i = 0; i < num_entries; i++) {
		delete entries[i].button;
	}
}

int	Tclvar_checklist::Lookup_entry (const char * entry_name) const
{
	int	i = 0;

	// Are there any entries?
	if (num_entries == 0) return -1;

	// Find this entry in the list
	while ( (i < num_entries) &&
		strcmp(entry_name,entries[i].name) ) {
		i++;
	};
	if (i >= num_entries) {
		return -1;	// No such entry
	}

	return i;
}

int	Tclvar_checklist::Add_entry (const char * entry_name, int value)
{
	Check_Parm	*parm = new Check_Parm;
	char		button_name[5000];

	if (parm == NULL) {
	    fprintf(stderr,"Tclvar_checklist::Add_entry(): Can't do parm\n");
	    return -1;
	}

	if ( (entry_name == NULL) || (strlen(entry_name) == 0) ) {
	    fprintf(stderr,"Tclvar_checklist::Add_entry(): Empty name\n");
	    return -1;
	}

	// Make sure it's not already in the list
	if (Lookup_entry(entry_name) != -1) {
	    fprintf(stderr,"Tclvar_checklist::Add_entry(): Already there\n");
	    return -1;
	}

	// Make sure there is enough room for the new entry
	if (num_entries >= 500) {
		fprintf(stderr,"Tclvar_checklist::Add_entry(): Too many\n");
		return -1;
	}

	// Create the new entry.
	// The name is that given as a parameter (truncated to fit within the
	// name length).  The name of the button has the parent name prepended,
	// to distinguish the entries of different checklists.
	strncpy(entries[num_entries].name, entry_name,
		sizeof(entries[num_entries].name) - 1);
	entries[num_entries].name[sizeof(entries[num_entries].name) - 1] = '\0';

	if (tcl_parent_name) {
		strncpy(button_name, tcl_parent_name, sizeof(button_name));
		button_name[TCLVAR_STRING_LENGTH-1] = 0;
	} else {
		button_name[0] = '\0';
	}
	if ( (strlen(button_name)+1) < sizeof(button_name)) {
		strcat(button_name,".");
	}
	strncat(button_name, entry_name,
		sizeof(button_name)-strlen(button_name) - 2);
	entries[num_entries].button =
		new Tclvar_int_with_button(button_name,tcl_parent_name, value);
	if (entries[num_entries].button == NULL) {
		fprintf(stderr,"Tclvar_checklist::Add_entry(): Out of mem\n");
		return -1;
	}
	num_entries++;

	// Set up the callback for this variable.  This tells which checklist
	// set the callback and for which entry it was set.  This lets the
	// callback route the result correctly.
	parm->whos = this;
	parm->which = &entries[num_entries-1];
	entries[num_entries-1].button->set_tcl_change_callback(
					checklist_callback, parm);

	return 0;
};

int	Tclvar_checklist::Remove_entry (const char * entry_name)
{
	int	i = Lookup_entry(entry_name);

	// Ensure that it is really in the list
	if (i == -1) { return -1; }

	// Delete the entry, move the last entry to this one
	// and decrement counter
	delete entries[i].button;
	entries[i] = entries[num_entries-1];
	num_entries--;

	return 0;
};

int	Tclvar_checklist::Set_entry (const char * entry_name)
{
	int i = Lookup_entry(entry_name);

	if (i == -1) { return -1; }

	*(entries[i].button) = 1;

	return 0;
}

int	Tclvar_checklist::Unset_entry (const char * entry_name)
{
	int i = Lookup_entry(entry_name);

	if (i == -1) { return -1; }

	*(entries[i].button) = 0;

	return 0;
}

int	Tclvar_checklist::Is_set (const char *entry_name) const
{
	int i = Lookup_entry(entry_name);

	if (i == -1) { return -1; }

	return *(entries[i].button);
}

const char * Tclvar_checklist::Entry_name (const int which) const
{
	if ( (which < 0) || (which >= num_entries) ) { return NULL; }

	return entries[which].name;
}

int	Tclvar_checklist::Is_set (const int which) const
{
	if ( (which < 0) || (which >= num_entries) ) { return -1; }

	return *(entries[which].button);
}

