/////////////////////////////////////////////////////////////////////////////
//	Defines classes of variables that will link themselves with Tcl
// variables.  They are meant to make it easy to create new variables and
// link them with Tcl variables.
//	They provide two-way links, with change in either Tcl or C changing
// the other.
//	The Tclvar_init() routine must be called to link all of them.  Each
// time through the main loop, Tclvar_mainloop() must be called to make any
// changes.
/////////////////////////////////////////////////////////////////////////////

#ifndef	TCL_LINKVAR_H

#include	<tcl.h>
#include	<tk.h>
#include <stdlib.h>  // for NULL

//class Tcl_Interp;  // from tcl.h

extern	int	Tclvar_init(Tcl_Interp *tcl_interp);
extern	int	Tclvar_mainloop(void);

class	Tclvar_int_with_button;
class	Tclvar_selector;
class	Tclvar_checklist;

typedef	void	(*Linkvar_Intcall)(int new_value, void *userdata);
typedef	void	(*Linkvar_Floatcall)(float new_value, void *userdata);
typedef	void	(*Linkvar_Selectcall)(char *new_value, void *userdata);
typedef	void	(*Linkvar_Checkcall)(char *entry, int new_value,void *userdata);

// TCH 1 Oct 97
#define TCLVAR_STRING_LENGTH 128

class	Tclvar_int {
    public:
	Tclvar_int(char *tcl_varname, int default_value = 0,
		   Linkvar_Intcall c=NULL, void *ud = NULL);
	virtual	~Tclvar_int();

	inline void set_tcl_change_callback(Linkvar_Intcall c, void *ud) {
		callback = c;
		userdata = ud;
	};

	inline operator int() {return myint;};
	inline int operator =(int v) {return myint = v;};

	inline int operator ++() {return ++myint;};
	inline int operator ++(int) {return myint++;};

	char	*my_tcl_varname;

	int	myint;
	int	mylastint;

	Linkvar_Intcall	callback;
	void		*userdata;
};

class	Tclvar_int_with_button: public Tclvar_int {
    friend class Tclvar_checklist;
    public:
	Tclvar_int_with_button(char *tcl_varname, char *parent_name,
		int default_value = 0, Linkvar_Intcall c=NULL, void *ud = NULL);
	virtual ~Tclvar_int_with_button();

	int	initialize(Tcl_Interp *interpreter);

	inline operator int() {return myint;};
	inline int operator =(int v) {return myint = v;};

	inline int operator ++() {return ++myint;};
	inline int operator ++(int) {return myint++;};

	char            *tcl_widget_name;
};

class	Tclvar_float {
    public:
	Tclvar_float(char *tcl_varname, float default_value = 0,
		Linkvar_Floatcall c = NULL, void *ud = NULL);
	virtual	~Tclvar_float();

	inline void set_tcl_change_callback(Linkvar_Floatcall c, void *ud) {
		callback = c;
		userdata = ud;
	};

	inline operator float() {return myfloat;};
	inline float operator =(float v) {return myfloat = v;};

	char	*my_tcl_varname;

	float	myfloat;
	float	mylastfloat;

	Linkvar_Floatcall	callback;
	void			*userdata;
};

//	When variables of this class are declared, the calling program must
// have had Tcl load the file "russ_widgets.tcl" in order to include the
// floatscale function definition.
//	This variable type will automatically display a floatscale inside
// the parent widget (unless the parent has NULL name) to track the value and
// allow its control.

class	Tclvar_float_with_scale: public Tclvar_float {
    public:
	Tclvar_float_with_scale(char *tcl_varname, char *parent_name,
		float minval, float maxval, float default_value = 0,
		Linkvar_Floatcall c = NULL, void *ud = NULL);
	virtual	~Tclvar_float_with_scale();

	int	initialize(Tcl_Interp *interpreter);

	inline operator float() {return myfloat;};
	inline float operator =(float v) {return myfloat = v;};

	char		*tcl_widget_name;
	char		*tcl_label_name;
	float		minvalue, maxvalue;
};

//	This class maintains a list of strings that will appear in pull-down
// menus for the selector class.  It also contains a list of selectors that
// currently use it.
//	This class knows how to build and modify the pull-down menus of
// selectors that use it, and it does so when it first gets an interpreter
// and thereafter whenever an item is added to or removed from its list.

class	Tclvar_list_of_strings {
    friend class Tclvar_selector;
    public:
	Tclvar_list_of_strings() { num_selectors = 0; num_entries = 0; };
	~Tclvar_list_of_strings();

	int	Add_entry(const char entry[]);
	int	Delete_entry(const char entry[]);

    protected:
	int	Add_selector(Tclvar_selector *sel);
	int	Delete_selector(Tclvar_selector *sel);

	Tclvar_selector	* selectors[100];	// Who uses this list?
	int		num_selectors;		//   how many filled in?
	char		entries [500][TCLVAR_STRING_LENGTH + 1];
  	  // What's in the list?
	int		num_entries;		//   how many filled in?
};

//	This class maintains a link between a C++ string (character array)
// and a Tcl variable.  It also instantiates a pull-down menu to allow the
// user to select values for the variable from a Tclvar_list_of_strings.
//	If you give the selector a NULL parent name, then it does not
// provide a pull-down list for itself.  In that case, it acts just like
// a string that is connected between Tcl and C++.

class	Tclvar_selector {
    friend class Tclvar_list_of_strings;
    public:

	Tclvar_selector (const char * initial_value = "");

	Tclvar_selector(char *tcl_varname, char *parent_name,
		Tclvar_list_of_strings *list = NULL, char *initial_value = "",
		Linkvar_Selectcall c = NULL, void *ud = NULL);

	~Tclvar_selector (void);

	inline void set_tcl_change_callback(Linkvar_Selectcall c, void *ud) {
		callback = c;
		userdata = ud;
	};
	void initializeTcl (const char * tcl_varname,
                            const char * parent_name);
	int bindList (Tclvar_list_of_strings * list);


	inline	operator char*() {return mystring;};
	char* operator =(char* v);

	int	initialize(Tcl_Interp *interpreter);
	int	initializeList (void);

	void	Set(const char *value);

	char	mystring [TCLVAR_STRING_LENGTH + 1];	  // Enable appending the 0-terminating character
	char	mylaststring [TCLVAR_STRING_LENGTH + 1];  // Enable appending the 0-terminating character
	char	*my_tcl_varname;
	char	*tcl_widget_name;
	char	*tcl_label_name;

	Linkvar_Selectcall	callback;
	void			*userdata;

    protected:
	Tclvar_list_of_strings	*mylist;

    private:
        int d_initialized;
          // 0 until initialize() has been called;  1 thereafter
};

//	This class maintains a checklist of items, each of which has a
// string name and a state.  The state is either on or off (0 or 1).  It
// instantiates checkboxes to display the states.  It uses a list_of_strings
// as its items to choose from.

typedef	struct {
	char			name[60];	// Name of the entry
	Tclvar_int_with_button	*button;	// Button for the entry
} Tclvar_Check_Entry;

class	Tclvar_checklist {
    public:
	Tclvar_checklist(char *parent_name);
	~Tclvar_checklist();

	int	Add_entry (const char *entry_name, int value = 0);
	int	Remove_entry (const char *entry_name);
	int	Set_entry (const char *entry_name);
	int	Unset_entry (const char *entry_name);
	int	Is_set (const char *entry_name) const;

	int	Num_entries (void) const {return num_entries;};
	const char * Entry_name (const int which) const;
	int	Is_set (const int which) const;

	// This returns a pointer to the entry that changed.
	inline void set_tcl_change_callback(Linkvar_Checkcall c, void *ud) {
		callback = c;
		userdata = ud;
	};

	Linkvar_Checkcall	callback;
        void                    *userdata;

    protected:
	char			*tcl_parent_name;	// Parent widget
	Tclvar_Check_Entry	entries [500];
	int			num_entries;	// How many are in the list

	int	Lookup_entry (const char * name) const;
};

#define	TCL_LINKVAR_H
#endif

