
typedef	unsigned char	P_COLOR[3];	/* Red, Green, Blue */
typedef	unsigned 	P_BIGCOLOR[3];

typedef	P_COLOR		*PPMROW;

class	PPM {
    private:

    protected:
	PPMROW	*pixels;	// The rows of pixel values
	int	nx,ny;		// Number of pixels in X and Y

    public:
	PPM(const char *filename);
	int	Value_at_normalized(double nx, double ny, int *red,
			int *green, int *blue) const;
	int	Tellppm(int x, int y, int *red, int *green, int *blue) const;
	int	Nx(void) const { return nx; }
	int	Ny(void) const { return ny; }
	int	Putppm(int x, int y,  int red, int green, int blue);
	int	Write_to(char *filename);
};

