/////////////////////////////////////////////////////////////////////////////
// Name:        ZGuesser.cpp
// Purpose:     Given a focus measure, this class provides a good guess of
//				the actual z offset
// Author:      Ryan Schubert
// Created:     4/5/07
// Modified by:
/////////////////////////////////////////////////////////////////////////////

#include "ZGuesser.h"

ZGuesser::ZGuesser(vector<float> focus, float stepsize)
{
	vals = focus;
	step = stepsize;

	int i;

	maxi = 0;
	max = vals[0];
	for (i = 1; i < vals.size(); ++i)
	{
		if (vals[i] > max)
		{
			max = vals[i];
			maxi = i;
		}
	}

	i = 0;
	while (vals[i] < max * 0.10)
	{
		++i;
	}

	loweri = i;
	lower = vals[i];

	i = vals.size() - 1;
	while (vals[i] < max * 0.10)
	{
		--i;
	}

	upperi = i;
	upper = vals[i];

	printf("loweri = %i, maxi = %i, upperi = %i\n", loweri, maxi, upperi);
	printf("lower = %f, max = %f, upper = %f\n", lower, max, upper);
}

float ZGuesser::guessOffset(float focus, bool above)
{
	float guess = 0;
	if (focus > max * 0.10)
	{
		float bottom;
		int bottomi;
		if (above)
		{
			bottom = upper;
			bottomi = upperi;
		}
		else
		{
			bottom = lower;
			bottomi = loweri;
		}
		guess = maxi + (bottomi-maxi) * ((focus-bottom)/(max-bottom));
	}
	return guess;
}