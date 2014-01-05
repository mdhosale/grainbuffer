
//*  grainbuffer~.c
//*  grainbuffer
//*
//*  Originally Created by MarkDavid Hosale on 11/29/05.
//*  updated 1/1/2014
//*	Thanks to Graham Wakefield for helping me fix the grain scheduler
//*	and helping me get through the Max/MSP nooks and crannys
//*  Copyright 2005 -2014

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h" // contains CLAMP and MAX macro
#include "z_dsp.h"
#include "ext_buffer.h"	// this defines our buffer's data structure and other goodies
#include <time.h>

#define GRAIN_LIST_SIZE 512
#define MAX_OUTPUT_CHANNELS 32

//enum for envelopetype
enum envType {sine, linear, exponential, trapezoid, parabolic, percussive, evissucrep, random_env};

// the struct that defines the grain creation grain_parameters
typedef struct _grain_param{
	
	t_double upper_freq;		// frequency (used to derive the index increment)
	t_double lower_freq;		// frequency (used to derive the index increment)
	
	t_double upper_dur;		// duration
	t_double lower_dur;		// duration
	
	t_double upper_disp;		// grain deltas (current wait time)
	t_double lower_disp;		// grain deltas (current wait time)
	
	t_double upper_amp;		// amplitude
	t_double lower_amp;		// amplitude
	
	t_double upper_pan;		// pan
	t_double lower_pan;		// pan
	
	enum envType env_type;		// envelope type
	
	t_double buf_rand;		// amount of randomness in the buffer start point
	
	// global params
	int grain_population;	// grain population
	
	t_double sampling_rate;	// the sampling rate
	long frames;			// number of frames in the current buffer
} t_grain_param;


// the grain struct that defines each grain
typedef struct _grain{
	//int grain_id;				//grain_id
	t_double index;				// current index in the buffer
	t_double index_incr;			// the index increment (freq)
	unsigned count_down;		// the current count_down, is the grain active, 0 or > 0
	
	t_double dur;					// duration
	t_double amp;					// amplitude
	t_double pan;
	enum envType env_type;		// envelope type
} t_grain;

// ***************************************************** //
// the struct that defines this external
typedef struct _grainbuffer{
    t_pxobject l_obj;	// first element represents the Max/MSP object
    t_buffer_ref *l_buffer_reference;	// the buffer~
    long l_bufchan;		// buffer channel
    bool t_m4l;
    
	t_grain grain_array[GRAIN_LIST_SIZE]; // the grain list ptr
	t_grain_param grain_param;
	t_double t_index;		// the current index
	t_double t_rate;		// the index increment
	t_double t_sampling_rate; // the sampling rate
	t_double t_time_til_next;	// time until next grain
	t_double t_start_loop; // start loop (a float from 0-1 that determines the point in the sample)
	t_double t_end_loop;	// end loop (a float from 0-1 that determines the point in the sample)
} t_grainbuffer;

// ************************************************ //
// *** FUNCTION DECLARATIONS ***

// the callback
void grainbuffer_perform64(t_grainbuffer *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

// how it registers the callback
void grainbuffer_dsp64(t_grainbuffer *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);

// sets the buffer~ we're going to access
void grainbuffer_set(t_grainbuffer *x, t_symbol *s);

// how Max/MSP instantiates the object
void *grainbuffer_new(t_symbol *s, long chan); // let's make bufchan an attribute and chan == the number of outlets

void grainbuffer_free(t_grainbuffer *x);
t_max_err grainbuffer_notify(t_grainbuffer *x, t_symbol *s, t_symbol *msg, void *sender, void *data);

// inlet 1 (to the right of the leftmost) sets read the audio channel from the buffer
void grainbuffer_in1(t_grainbuffer *x, long n);

// tooltips in Max/MSP
void grainbuffer_assist(t_grainbuffer *x, void *b, long m, long a, char *s);

// this lets us double-click on grainbuffer~ to open up the buffer~ it references
void grainbuffer_dblclick(t_grainbuffer *x);

// pointer to the grainbuffer class
static t_class *grainbuffer_class;


// ********** //

// change the read rate of the buffer~
void grainbuffer_rate(t_grainbuffer *x, t_double f);

// change the loop points of the buffer~
void grainbuffer_loop(t_grainbuffer *x, t_double start, t_double end);

// initializes the grain creation parameters
void initialize_grain_param(t_grainbuffer *x);

// initializes the grain array
void initialize_grain_array(t_grain * grain_array);

// makes a new grain, called by the scheduler
void make_grain(t_grainbuffer *x, t_grain * this_grain, t_double cur_index);

// ************* addmess declarations*************** //

// grain frequency
void grain_rand_freq(t_grainbuffer *x, t_double lower_freq, t_double upper_freq);
t_double get_incr(t_grainbuffer *x);

// grain duration
void grain_rand_dur(t_grainbuffer *x, t_double lower_dur, t_double upper_dur);
t_double get_dur(t_grainbuffer *x);

// grain disp
void grain_rand_disp(t_grainbuffer *x, t_double lower_disp, t_double upper_disp);
t_double get_disp(t_grainbuffer *x);

// grain amp
void grain_rand_amp(t_grainbuffer *x, t_double lower_amp, t_double upper_amp);
t_double get_amp(t_grainbuffer *x);

// grain pan
void grain_rand_pan(t_grainbuffer *x, t_double lower_pan, t_double upper_pan);
t_double get_pan(t_grainbuffer *x);
t_double pan_calc(t_double input, int outindex, long numouts);

// amount of randomness in the buffer read
void grain_rand(t_grainbuffer *x, t_double rand_amount);
t_double get_rand_index(t_grainbuffer *x, t_double cur_index);

// grain envelope
void grain_env_type(t_grainbuffer *x,  t_symbol *s, short argc, t_atom *argv);
enum envType get_rand_env_type(t_grainbuffer *x); // random envelopes
enum envType get_grain_env_type(t_grainbuffer *x);
t_double grain_env(t_grain * this_grain); // envelopes the grain in perform

// dump params
void grain_param_dump(t_grainbuffer *x);


int C74_EXPORT main(void){
    
	t_class *c = class_new("grainbuffer~", (method)grainbuffer_new, (method)grainbuffer_free, sizeof(t_grainbuffer), 0L, A_SYM, A_DEFLONG, 0);
	
	class_addmethod(c, (method)grainbuffer_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)grainbuffer_set, "set", A_SYM, 0);
	class_addmethod(c, (method)grainbuffer_in1, "in1", A_LONG, 0);
	class_addmethod(c, (method)grainbuffer_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)grainbuffer_dblclick, "dblclick", A_CANT, 0);
	class_addmethod(c, (method)grainbuffer_notify, "notify", A_CANT, 0);
    
	//seed the random numbers
	srand((unsigned int)time(NULL));
	
	//Map messages to functions
	class_addmethod(c, (method)grainbuffer_rate,"rate", A_FLOAT, 0);
	class_addmethod(c, (method)grainbuffer_loop,"loop", A_FLOAT, A_FLOAT, 0);
	
	//grain params
	class_addmethod(c, (method)grain_rand_freq,"freq", A_FLOAT, A_FLOAT, 0);
	class_addmethod(c, (method)grain_rand_dur,"dur", A_FLOAT, A_FLOAT, 0);
	class_addmethod(c, (method)grain_rand_disp,"disp", A_FLOAT, A_FLOAT, 0);
	class_addmethod(c, (method)grain_rand_amp,"amp", A_FLOAT, A_FLOAT, 0);
	class_addmethod(c, (method)grain_rand_pan,"pan", A_FLOAT, A_FLOAT, 0);
	
	//envelopes
	class_addmethod(c, (method)grain_env_type,"sine", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"linear", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"exponential", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"trapezoid", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"parabolic", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"percussive", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"evissucrep", A_GIMME, 0);
	class_addmethod(c, (method)grain_env_type,"random", A_GIMME, 0);
	
	class_addmethod(c, (method)grain_rand,"rand", A_FLOAT, 0);
    
    //dump
    class_addmethod(c, (method)grain_param_dump,"dump", 0);
    
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	grainbuffer_class = c;
	return 0;
}

////////////////////////////////////////////////////////////////////
// *** THE DSP64 CALLBACK ***
// this is 64-bit perform method for Max 6

void grainbuffer_perform64(t_grainbuffer *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    // used for variable outs
    t_double *out[MAX_OUTPUT_CHANNELS];
    t_double	*out1, *out2;
    

    int			n = sampleframes;
    
    float		*tab;
    long		grainbufferindex, bufchan, frames, nc;
    
    
    t_double index, index_incr, start_loop_point, end_loop_point;
	long start_loop, end_loop;
    
    t_grain * this_grain;
	t_grain * grain_array;
    int number_grains, i, outindex, grain_array_index;
	t_double amp, env, pan;
    
	t_buffer_obj	*b = buffer_ref_getobject(x->l_buffer_reference);
    
    if(!x->t_m4l){
        for(outindex = 0; outindex < numouts; outindex++){
            out[outindex] = outs[outindex];
            memset(out[outindex], 0, n*sizeof(t_double));
        }
    } else {
        out1 = outs[0];
        out2 = outs[1];
        memset(out1, 0, n*sizeof(t_double));
        memset(out2, 0, n*sizeof(t_double));
        
    }
    
    // buffer~
	tab = buffer_locksamples(b);           // samples in the buffer
	if (!tab)
        goto zero;
    
 
    
    bufchan = MIN(x->l_bufchan, 3);		   // the channel of the buffer to be read
	frames = buffer_getframecount(b);      // the number of buffer frames
	nc = buffer_getchannelcount(b);        // the number of buffer channels
    
	///////////////////////
    
    number_grains = 0;
    grain_array_index = 0;
    
    
	// get current state from this object
    
    //set the number of frames in the grain param
	x->grain_param.frames = frames;
    
	grain_array = x->grain_array;           // this Max/MSP object's internal state data
    
    index = x->t_index;						// the current index
    index_incr = x->t_rate;					// the index increment (freq)
    start_loop_point = x->t_start_loop;
    end_loop_point = x->t_end_loop;
	
    start_loop = (long)(frames * start_loop_point);	// set the start loop point, must be < end_loop
    end_loop = (long)(frames * end_loop_point);		// set the end loop point, must be < frames
    
	/////////////////////
    
	while (n--) {
		
		index += index_incr;						// play next index
		
		if (index_incr > 0 && index >= end_loop)	// if out of bounds set to 1 (causes looping)
			index = start_loop + 1;
		else if (index_incr < 0 && index <= start_loop)
			index = end_loop;
        
		// if it's time, make a grain
		if (x->t_time_til_next <= 0){
			
            //set the time til next to current dispersion
			x->t_time_til_next = (x->t_sampling_rate/1000.) * get_disp(x);
			
			grain_array_index = 0;
			this_grain = grain_array;
            
			while (this_grain->count_down > 0) {
				grain_array_index++;
				
				if (grain_array_index < GRAIN_LIST_SIZE)
					this_grain = grain_array + grain_array_index;
				else break;
			}
			
			//create new grain
			if (grain_array_index < GRAIN_LIST_SIZE) {
				make_grain(x, this_grain, index);
				x->grain_param.grain_population = x->grain_param.grain_population + 1;
			}
			
		} else {
			x->t_time_til_next = x->t_time_til_next - 1;
		}
        
		// set some starting points for the loop
		number_grains = x->grain_param.grain_population;
		grain_array_index = 0;
		this_grain = (t_grain *)grain_array;
		
		//start loop here
		for (i = 0; i < number_grains; i++){
            
			while (this_grain->count_down <= 0)	{								// look for a living grain
				
				grain_array_index = grain_array_index + 1;
				
				if (grain_array_index < GRAIN_LIST_SIZE)
					this_grain = (t_grain *)(grain_array + grain_array_index);
				else break;														// no more grains stop looking
			}
			
			if (grain_array_index >= GRAIN_LIST_SIZE) break;					// if we run out of grains stop looking
			
			this_grain->index = this_grain->index + this_grain->index_incr;     // play next index for this grain
            
			// if out of bounds set to 1 (causes looping)
			if (this_grain->index_incr > 0 && this_grain->index >= end_loop)
				this_grain->index = start_loop + 1;
			else if (this_grain->index_incr < 0 && this_grain->index <= start_loop)
				this_grain->index = end_loop;
            
			grainbufferindex = (long)this_grain->index;								// get the buffer channel to use
			
			if (nc > 1)                                                         // If the number of channels in the buffer is > 1
				grainbufferindex = grainbufferindex * nc + bufchan;				// get the channel to play
            
			amp = this_grain->amp;												// amplitude
			
			env = grain_env(this_grain);										// get the grain envelope
            
            if (!x->t_m4l){
                for(outindex = 0; outindex < numouts; outindex++){
                    pan = pan_calc(this_grain->pan, outindex, numouts);
                    *out[outindex] += tab[grainbufferindex] * amp * env * pan;
                }
            } else {
                pan = pan_calc(this_grain->pan, 0, 2);
                *out1 += tab[grainbufferindex] * amp * env * pan;
                
                pan = pan_calc(this_grain->pan, 1, 2);
                *out2 += tab[grainbufferindex] * amp * env * pan;
            }
            
			this_grain->count_down = this_grain->count_down - 1;				//decrement the count
            
			if (this_grain->count_down <= 0) {                                  // remove when dead
				x->grain_param.grain_population = x->grain_param.grain_population - 1;
			}
            
			grain_array_index = grain_array_index + 1;							// get the next grain
			this_grain = (t_grain *)(grain_array + grain_array_index);
            
		} // end grain loop
        if (!x->t_m4l){
            for(outindex = 0; outindex < numouts; outindex++)
                out[outindex]++;
		}else{
            out1++;
            out2++;
        }
	} // end sample loop
    
    x->t_index = index;							// set the new index for next CB
    
	buffer_unlocksamples(b);
    return;
zero:
	while (--n) {								// output zero (silence)
      if (!x->t_m4l){
          for(outindex = 0; outindex < numouts; outindex++)
            *++out[outindex] = 0.;
      }else {
          *++out1 = 0.;
          *++out2 = 0.;
      }
	}
}

// ************************************************ //
// sets the buffer~ we're going to access
void grainbuffer_set(t_grainbuffer *x, t_symbol *s)
{
    if (!x->l_buffer_reference)
		x->l_buffer_reference = buffer_ref_new((t_object*)x, s);
	else
		buffer_ref_set(x->l_buffer_reference, s);
}

// ************************************************ //
//inlet 1 (to the right of the leftmost) sets Audio Channel In buffer~
void grainbuffer_in1(t_grainbuffer *x, long n)
{
	if (n)
		x->l_bufchan = CLAMP(n,1,4) - 1;
	else
		x->l_bufchan = 0;
}

/************************************************/
// *** REGISTER THE DSP CALLBACK ***

// this is the Max 6 version of the dsp method -- it registers a function for the signal chain in Max 6,
// which operates on 64-bit audio signals.
void grainbuffer_dsp64(t_grainbuffer *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    x->t_sampling_rate = samplerate;
	x->grain_param.sampling_rate = samplerate;
    
    //make the grains
	initialize_grain_array(x->grain_array);
    
    dsp_add64(dsp64, (t_object*)x, (t_perfroutine64)grainbuffer_perform64, 0, NULL);
}

// ************************************************ //
// this lets us double-click on grainbuffer~ to open up the buffer~ it references
void grainbuffer_dblclick(t_grainbuffer *x)
{
    buffer_view(buffer_ref_getobject(x->l_buffer_reference));
}


// ************************************************ //
// assist sends Max/MSP status bar messages when the user mouses over the inlets/outlets
void grainbuffer_assist(t_grainbuffer *x, void *b, long m, long a, char *s){
    
	if (m == ASSIST_OUTLET){
        sprintf(s,"(Signal) Grains (ch %d)", (int)(a+1));
    } else {
        switch (a) {
            case 0: sprintf(s,"Input Messages (see help)"); break;
            case 1: sprintf(s,"Audio Channel In buffer~"); break;
        }
    }
}

// ************************************************ //
// *** EXTERNAL INSTANTIATION METHOD ***
void *grainbuffer_new(t_symbol *s, long chan)
{
	int i; //for windows
    int channels;
    t_grainbuffer *x = object_alloc(grainbuffer_class);
    
    if (x) {
        // initialize the variables
        x->t_rate = 1.;
        x->t_time_til_next = 0.;
        x->t_start_loop = 0.;
        x->t_end_loop = 1.;
        x->t_m4l = chan == 0 || chan == 2 ? true: false;
        initialize_grain_param(x);
        
        dsp_setup((t_pxobject *)x, 0);
        intin((t_object *)x,1);
        
        if (!x->t_m4l){
            // dynamic channel instantiation
            chan > MAX_OUTPUT_CHANNELS ? error("grainbuffer~: max channels == %d, defaulting to %d",MAX_OUTPUT_CHANNELS, MAX_OUTPUT_CHANNELS): 0.;
            chan < 1 && chan !=0 ? error("grainbuffer~: min channels == 1, defaulting to 1",0): 0.;
            
            channels = chan != 0 ? CLAMP(chan, 1, MAX_OUTPUT_CHANNELS) : 2;
            
            for (i = 0; i < channels; i++)
                outlet_new((t_object *)x, "signal");
        } else {
            outlet_new((t_object *)x, "signal");
            outlet_new((t_object *)x, "signal");
            post("grainbuffer~: Stereo Max 4 Live Compatiblity mode", 0);
        }
        
        grainbuffer_set(x, s);
        grainbuffer_in1(x, 0);
    }
	return (x);
}


void grainbuffer_free(t_grainbuffer *x)
{
	dsp_free((t_pxobject*)x);
	object_free(x->l_buffer_reference);
}


t_max_err grainbuffer_notify(t_grainbuffer *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
	return buffer_ref_notify(x->l_buffer_reference, s, msg, sender, data);
}


// *********** FUNCTIONS ************** //

// set the incr amount (proportional to frequency)
void grainbuffer_rate(t_grainbuffer *x, t_double f){
	x->t_rate = f;
}

// ************************************************ //
// change the start loop (a float from 0-1 that determines the point in the sample)
void grainbuffer_loop(t_grainbuffer *x, t_double start, t_double end){
	
	// make sure the start point is before the end point, otherwise flip them
	if (start > x->t_end_loop || end < x->t_start_loop){
        x->t_start_loop =  CLAMP(end, 0.,1.);
        x->t_end_loop = CLAMP(start, 0.,1.);
    } else {
		x->t_start_loop = CLAMP(start, 0.,1.);
		x->t_end_loop = CLAMP(end, 0.,1.);
    }
}


// *********** GRAIN LIST FUNCTIONS ************** //

// *** THE GRAIN SCHEDULER ***
void make_grain(t_grainbuffer *x, t_grain * this_grain, t_double cur_index)
{
	t_double dur = get_dur(x);
	t_double pan = get_pan(x);
	
	if (this_grain != NULL){
		this_grain->index = get_rand_index(x, cur_index);	// start index in the buffer
		this_grain->index_incr = get_incr(x);				// frequency (used to derive the index increment)
		this_grain->count_down = (unsigned int) dur;						// time to live (decrements from dur)
		this_grain->dur = dur;								// duration
		this_grain->amp = get_amp(x);						// amplitude
		this_grain->pan = pan;                              // pan
		this_grain->env_type = get_grain_env_type(x);		// envelope
	}
}

/////////////////////////////////////////////////////////////
void initialize_grain_param(t_grainbuffer *x){
    
	//set the initial grain parameters
    
	x->grain_param.upper_freq = 1.;			// frequency (used to derive the index increment)
	x->grain_param.lower_freq = 1.;
	
	x->grain_param.upper_dur = 50.;			// duration in milliseconds
	x->grain_param.lower_dur = 50.;
	
	x->grain_param.upper_disp = 60.;		// grain deltas (current wait time)
	x->grain_param.lower_disp = 60.;
	
	x->grain_param.upper_amp = 1.;			// amplitude
	x->grain_param.lower_amp = 0.;
    
	x->grain_param.upper_pan = 1.;			// pan
	x->grain_param.lower_pan = 0.;
	
	x->grain_param.env_type = sine;			// envelope type
	
	x->grain_param.buf_rand = 0.;			//amount of randomness in the buffer start point
	
	x->grain_param.grain_population = 0;	// population of grain
}


void grain_param_dump(t_grainbuffer *x){
    
    //dump the params
    post("upper freq = %.2f", x->grain_param.upper_freq);
    post("lower freq = %.2f", x->grain_param.lower_freq);
    post("upper dur = %.2f", x->grain_param.upper_dur);
    post("lower dur = %.2f", x->grain_param.lower_dur);
    post("upper disp = %.2f", x->grain_param.upper_disp);
    post("lower disp = %.2f", x->grain_param.lower_disp);
    post("upper amp = %.2f", x->grain_param.upper_amp);
    post("lower amp = %.2f", x->grain_param.lower_amp);
    post("upper pan = %.2f", x->grain_param.upper_pan);
    post("lower pan = %.2f", x->grain_param.lower_pan);
    post("env type = %d", (int)x->grain_param.env_type);
    post("buf rand = %.2f", x->grain_param.buf_rand);
    post("grain population = %d", x->grain_param.grain_population);
    //   post("I think the Sampling Rate = %.2f", x->grain_param.sampling_rate);
}


////////////////////////////////////////////////////////////////////
void initialize_grain_array(t_grain * grain_array){
	int i; //for windows
	for(i = 0; i < GRAIN_LIST_SIZE; i++){
		//grain_array[i].grain_id = i;
		grain_array[i].index = 0;
		grain_array[i].index_incr = 1;
		grain_array[i].count_down = 0;
		grain_array[i].dur = 0;
		grain_array[i].amp = 0;
		grain_array[i].pan = 0.5;
		grain_array[i].env_type = sine;
	}
}

// ********** ADD MESS FUNCTIONS (Set and Get)****************** //

// grain frequency
void grain_rand_freq(t_grainbuffer *x, t_double lower_freq, t_double upper_freq){
	x->grain_param.upper_freq = upper_freq;
	x->grain_param.lower_freq = lower_freq;
    
}

t_double get_incr(t_grainbuffer *x){
	t_double freq, random_value = (t_double) rand()/(t_double) RAND_MAX;
	
	if(x->grain_param.upper_freq == x->grain_param.lower_freq)
        return x->grain_param.lower_freq;
	else if (x->grain_param.upper_freq > x->grain_param.lower_freq)
		freq = ((x->grain_param.upper_freq - x->grain_param.lower_freq) * random_value) + x->grain_param.lower_freq;
	else
		freq = ((x->grain_param.lower_freq - x->grain_param.upper_freq) * random_value) + x->grain_param.upper_freq;
	
	return freq;
}

///////////////////////////////////////////////////////////////////////////
// grain duration
void grain_rand_dur(t_grainbuffer *x, t_double lower_dur, t_double upper_dur){
	x->grain_param.upper_dur = MAX(upper_dur,0.);
	x->grain_param.lower_dur = MAX(lower_dur,0.);
}

t_double get_dur(t_grainbuffer *x){
	t_double dur, random_value = (t_double) rand()/(t_double) RAND_MAX;
	
	if(x->grain_param.upper_dur == x->grain_param.lower_dur)
        return x->grain_param.lower_dur;
	else if (x->grain_param.upper_dur > x->grain_param.lower_dur)
		dur = ((x->grain_param.upper_dur - x->grain_param.lower_dur) * random_value) + x->grain_param.lower_dur;
	else
		dur = ((x->grain_param.lower_dur - x->grain_param.upper_dur) * random_value) + x->grain_param.upper_dur;
    
	return ( (x->grain_param.sampling_rate/1000.0) * dur );
}

///////////////////////////////////////////////////////////////////////////////
// grain disp
void grain_rand_disp(t_grainbuffer *x, t_double lower_disp, t_double upper_disp){
	x->grain_param.upper_disp = MAX(upper_disp,0.);
	x->grain_param.lower_disp = MAX(lower_disp,0.);
}

t_double get_disp(t_grainbuffer *x){
	t_double random_value = (t_double) rand()/(t_double) RAND_MAX;
	
	if(x->grain_param.upper_disp == x->grain_param.lower_disp)
        return x->grain_param.lower_disp;
	else if (x->grain_param.upper_disp > x->grain_param.lower_disp)
		return ( ((x->grain_param.upper_disp - x->grain_param.lower_disp) * random_value) + x->grain_param.lower_disp );
	else
		return ( ((x->grain_param.lower_disp - x->grain_param.upper_disp) * random_value) + x->grain_param.upper_disp );
}

//////////////////////////////////////////////////////////////////////////////////////
// grain amp
void grain_rand_amp(t_grainbuffer *x, t_double lower_amp, t_double upper_amp){
	x->grain_param.upper_amp = upper_amp;
	x->grain_param.lower_amp = lower_amp;
}

t_double get_amp(t_grainbuffer *x){
	t_double random_value = (t_double) rand()/(t_double) RAND_MAX;
	
    if(x->grain_param.upper_amp == x->grain_param.lower_amp)
        return x->grain_param.lower_amp;
	else if (x->grain_param.upper_amp > x->grain_param.lower_amp)
		return ((x->grain_param.upper_amp - x->grain_param.lower_amp) * random_value) + x->grain_param.lower_amp;
	else
		return ((x->grain_param.lower_amp - x->grain_param.upper_amp) * random_value) + x->grain_param.upper_amp;
}

////////////////////////////////////////////////////////////////////////////////////////
// grain pan
void grain_rand_pan(t_grainbuffer *x, t_double lower_pan, t_double upper_pan){
	x->grain_param.upper_pan = CLAMP(upper_pan, 0., 1.);
	x->grain_param.lower_pan = CLAMP(lower_pan, 0., 1.);
}

t_double get_pan(t_grainbuffer *x){
	t_double random_value = (t_double) rand()/(t_double) RAND_MAX;
	
    if(x->grain_param.upper_pan == x->grain_param.lower_pan)
        return x->grain_param.lower_pan;
	else if (x->grain_param.upper_pan > x->grain_param.lower_pan)
		return ((x->grain_param.upper_pan - x->grain_param.lower_pan) * random_value) + x->grain_param.lower_pan;
	else
		return ((x->grain_param.lower_pan - x->grain_param.upper_pan) * random_value) + x->grain_param.upper_pan;
}


t_double pan_calc(t_double input, int outindex, long numouts){
    
    t_double pan = 1.;
    
    if (numouts > 1){
        t_double pan_win = 1./(t_double)(numouts - 1);
        
        // first channel
        if (outindex == 0){
            t_double this_pan;
            
            this_pan = input <= pan_win ? input/pan_win : 1.; // cos(1.) == 0.
            
            pan = cos(this_pan * PIOVERTWO);
            
            // last channel
        } else if (outindex == (int)(numouts - 1)) {
            t_double this_pan, start_num, scaled_pwin;
            
            start_num = (1. - pan_win)/1.;
            
            scaled_pwin = (input - start_num) * (numouts - 1);
            
            this_pan = input >= start_num  ? scaled_pwin : 0.;
            
            pan = sin(this_pan * PIOVERTWO);
            
            // middle channels
        } else {
            
            t_double this_pan, start_num, end_num, scaled_pwin;
            
            start_num = (pan_win * (outindex-1)) + (pan_win/2.);
            end_num = start_num + pan_win;
            
            scaled_pwin = (input - start_num) * (numouts - 1);
            
            this_pan = input >= start_num && input <= end_num ? scaled_pwin : 0.;
            
            pan = sin(this_pan * PI);
        }
        
    }
    
    return pan;
}

///////////////////////////////////////////////////////////////////////
// grain envelope

// set the envelope type
void grain_env_type(t_grainbuffer *x,  t_symbol *s, short argc, t_atom *argv){
    
	if (s == gensym("sine")){
		x->grain_param.env_type = sine;
	} else if (s == gensym("linear")){
		x->grain_param.env_type = linear;
	} else if (s == gensym("exponential")){
		x->grain_param.env_type = exponential;
	} else if (s == gensym("trapezoid")){
		x->grain_param.env_type = trapezoid;
	} else if (s == gensym("parabolic")){
		x->grain_param.env_type = parabolic;
	} else if (s == gensym("percussive")){
		x->grain_param.env_type = percussive;
	} else if (s == gensym("evissucrep")){
		x->grain_param.env_type = evissucrep;
	} else if (s == gensym("random")){
		x->grain_param.env_type = random_env;
    }
    
}

// choose an envelope randomly
enum envType get_rand_env_type(t_grainbuffer *x){
	t_double random = (t_double)rand()/(t_double)RAND_MAX;
	return (enum envType)floor(random * 6.99);
}

// get the envelope type
enum envType get_grain_env_type(t_grainbuffer *x){
	if (x->grain_param.env_type == random_env)
		return get_rand_env_type(x);
        else
            return x->grain_param.env_type;
}

// apply the envelope
t_double grain_env(t_grain * this_grain){
    
	t_double duration, grain_count_down, env;
	enum envType env_type;
    
    grain_count_down = this_grain->count_down;	// grain index
    duration = this_grain->dur;					// duration
    env_type = this_grain->env_type;			// envelope
    
	switch(env_type) {
        case random_env: // unneccesary but just in case...
        case sine:
            env = sin( ( (grain_count_down - 1)/duration ) * PI );
            break;
            
        case linear:
            if (grain_count_down > duration/2 )
                env = (duration - grain_count_down)/(duration/2); // rise
            else
                env = (grain_count_down - 1)/(duration/2); // decay
            break;
            
        case exponential:
            if (grain_count_down > duration/2 )
                env = pow((duration - grain_count_down)/(duration/2), 4); // slope up
            else
                env = pow((grain_count_down - 1)/(duration/2), 4);	// slope down
            break;
            
        case trapezoid:
            if (grain_count_down > ((duration/4) * 3) )
                env = (duration - grain_count_down)/(duration/4); // rise
            else if (grain_count_down <= (duration/4))
                env = (grain_count_down - 1)/(duration/4); // decay
            else
                env = 1; //steady state
            break;
            
        case parabolic:
            if (grain_count_down > ((duration/4) * 3) )
                env = sin( ( (duration - (grain_count_down - 1))/(duration/4) ) * (PI/2) );// rise
            else if (grain_count_down <= (duration/4))
                env = sin( ( (grain_count_down - 1)/(duration/4) ) * (PI/2) ); // decay
            else
                env = 1; //steady state
            break;
            
        case percussive:
            if (grain_count_down > ((duration/20) * 19))
                env = (duration - (grain_count_down - 1))/duration;	// anti-click
            else
                env = pow((grain_count_down - 1)/duration, 7); // exponential decay
            break;
            
        case evissucrep:
            if (grain_count_down > (duration/20))
                env = pow( (duration - (grain_count_down - 1))/duration, 7); // exponential rise
            else
                env = (grain_count_down - 1)/duration;	// anti-click
            break;
    }
    
	return env;
}

//////////////////////////////////////////////////////////////
// amount of randomness in the buffer start point
void grain_rand(t_grainbuffer *x, t_double rand_amount){
    x->grain_param.buf_rand = CLAMP(rand_amount, 0., 1.);
}

// use this to set the randomness
t_double get_rand_index(t_grainbuffer *x, t_double cur_index)
{
	t_double random = (t_double)rand()/(t_double)RAND_MAX;
	t_double min = (x->grain_param.buf_rand*x->t_start_loop*x->grain_param.frames)
    + (1.0-x->grain_param.buf_rand)*cur_index;
	t_double max = (x->grain_param.buf_rand*x->t_end_loop*x->grain_param.frames)
    + (1.0-x->grain_param.buf_rand)*cur_index;
	return min + (random * (max - min));
}