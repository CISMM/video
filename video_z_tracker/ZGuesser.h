/////////////////////////////////////////////////////////////////////////////
// Name:        ZGuesser.h
// Purpose:     Given a focus measure, this class provides a good guess of
//				the actual z offset
// Author:      Ryan Schubert
// Created:     4/5/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#pragma once


#include <vector>


using namespace std;


class ZGuesser
{
public:
	ZGuesser(vector<float> focus, float stepsize);

	float guessOffset(float focus, bool above);

protected:

	float step;
	vector<float> vals;

	float lower, max, upper;
	int loweri, maxi, upperi;


private:

};