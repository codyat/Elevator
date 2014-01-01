////////////////////////////////////////////////////////////////////////
// 
// Name: Cody Troyer
// Username: ctroy001
// SID: 860992873
// 
// I hereby certify that all of the work done in this submission 
// is entirely my own.
//
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <iomanip>
#include "cpp.h"
#include <string.h>
#include <cmath>
using namespace std;

const int NUM_SEATS = 10;

const double BASE_SPEED = 6.4;
const double FLOOR_SPEED = 1.6;

int seats_used[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int people_inside = 0;
int kicked = 0;

event *no_one_left;
event_set **events;
mailbox_set **get_out;
mailbox_set *mb_up, *mb_dn;
event_set *up_shaft, *down_shaft;
bool busy[15];

void arrivals();
void traveler();
void elevator();
void elevator_clockwise(int,const long&);
void travel(const long&, int&, int&, bool&);
void scan_next(const int&, int&, const bool&, const long&);
void call_to_floor(const int&);
void wait_for_correct_elevator(const int&, const int&, long&);
void in_n_out(const int&, int&, long&);
void travel_when_kicked(const long&, int&, int, bool&);

qtable *qtd;

double wait_up = 0;
double wait_down = 0;
double global_trip_time = 0;

int num_wait_up = 0;
int num_wait_down = 0;
int global_num_trip = 0;

int FLOORS = 15;
int CUSTS;
int ELEVATORS;

#define TINY 1

extern "C" void sim()
{
	create("sim");
	max_processes(20000);
	max_mailboxes(20000);
	max_events(20000);
	
	// Now ask the user for number of floors and max number of people
	// here
	
	cout << "enter a number of people: ";
	cin >> CUSTS;
	
	cout << "enter a number of elevators: ";
	cin >> ELEVATORS;	
	
 	qtd = new qtable("queue length");
 	mb_up = new mailbox_set("mbup", FLOORS);
 	mb_dn = new mailbox_set("mbdown", FLOORS);
	no_one_left = new event("No people left!");
	events = new event_set*[ELEVATORS];
	get_out = new mailbox_set*[ELEVATORS];
	up_shaft = new event_set("us", FLOORS);
	down_shaft = new event_set("ds", FLOORS);
  kicked = 2*FLOORS/ELEVATORS;
  
  for(int i = 0; i < ELEVATORS; i++) 
  {
		events[i] = new event_set("called", FLOORS+1);
	  get_out[i] = new mailbox_set("get out", FLOORS);
  }
	
	qtd->add_histogram(10, 0, 10);

	arrivals();
 	elevator();
	no_one_left->wait();

 	cout << "Average trip time: " << global_trip_time/global_num_trip << endl;
	cout << "Average waiting time up: " << wait_up/num_wait_up << endl;
	cout << "Average waiting time down: " << wait_down/num_wait_down << endl;

	qtd->report();
}

void arrivals()
{
	create("arrival");
	int count = 0;
	int batch = 0;

	while(count < CUSTS)
	{
		// Get random number of people who enter the building
		// Note: You must not exceed the global number of people
		// as entered by the user.
		//
		//Hint: if the random number is bigger than what you can allow in
		//simply pick the latter
		batch = static_cast<int>(uniform(1, 12));

    if(count + batch > CUSTS) batch = CUSTS - count; 

		for(int i=0;i < batch;i++)
		{
			//Create passenger here
			traveler();
			//Increment variables when appropriate
			people_inside++;
		  count++;
		}
		hold(60);
	}
}

void call_to_floor(const int &curr) 
{
	for(int i = 0; i < ELEVATORS; i++) (*events[i])[curr].set();
}

void wait_for_correct_elevator(const int &curr, const int &next, long &ID)
{
	// Calculate wait time up and wait time down in here
	if(next > curr) 
	{
		double temp = clock;
		(*mb_up)[curr].receive(&ID);
		wait_up += clock - temp;
		num_wait_up++;
	}
	else if(curr > next)
	{ 
		double temp = clock;
		(*mb_dn)[curr].receive(&ID);
		wait_down += clock - temp;
		num_wait_down++;
	}
}

void in_n_out(const int &next, int &curr, long &ID)
{
	// You must do note_entry and note_exit appropriately in here
	// Also, trip time
  long message;
  (*events[ID])[next].set();
  double temp = clock;
  (*qtd).note_entry();
  (*get_out[ID])[next].receive(&message);
  (*qtd).note_exit();
  global_trip_time += clock - temp;
  curr = next;
  global_num_trip++;
}

void traveler()
{
	create("traveler");

	// When person first gets into the building, he starts at floor #0
	double enter_time = clock;
	int curr = 0;
	int next;
	long ID;

	// Work for 8 hours
	while(clock <= enter_time + 8*3600)
	{
		// Find out where he wants to go next
		do
		{
			next = static_cast<int>(uniform(0, FLOORS - 1));
		}
		while(next==curr);
		call_to_floor(curr);
		//printf("%9.1f: Calling elevator at floor %d to go to floor %d!\n", clock, curr, next);
	  wait_for_correct_elevator(curr, next, ID);
		//printf("%9.1f: Elevator %d door opens at floor %d!\n", clock, ID, curr);
    in_n_out(next, curr, ID);	
    //printf("%9.1f: Getting out at floor %d!\n", clock, curr);
		hold(abs(normal(1800,480)));
	}

	// When it is time to leave
	if(curr > 0)
	{
		next = 0;
		// What do you need the passenger to do?
		call_to_floor(curr);
		wait_for_correct_elevator(curr, next, ID);
		in_n_out(next, curr, ID);
	}

	people_inside--;
	if(people_inside == 0)
	{
		//Let the simulation knows that it can terminate
		no_one_left->set();
	}
}

void elevator()
{
  create("elevator");
  
  for(int i = 0; i < ELEVATORS; i++)
  {
    elevator_clockwise(i, i);
  }
}

// This function gets the next destination for the elevator 
// ON THE SAME SIDE OF THE BUILDING
void scan_next(const int &curr, int &next, const bool &going_up, const long &ID)
{
	next = -1;
	int i;
	if(going_up)
	{
		// Search upward for a floor to stop at. Pick the nearest floor
		// where someone wants to leave the elevator OR there are spaces
		// and there are people who wants to get in on that floor
		for(i = curr; i < FLOORS; i++)
		{
			if((*get_out[ID])[i].queue_cnt() > 0 || ((*mb_up)[i].queue_cnt() > 0 && seats_used[ID] < NUM_SEATS))
			{
        next = i;
        break;
			}
    }
		// If no floor is found, move to top
		if(next == -1) next = FLOORS - 1;
	}
	else
	{
		// Search downward for a floor to stop at. Pick the nearest floor
		// where someone wants to leave the elevator OR there are spaces
		// and there are people who wants to get in on that floor
		for(i = curr; i >= 0; i--)
			if((*get_out[ID])[i].queue_cnt() > 0 || ((*mb_dn)[i].queue_cnt() > 0 && seats_used[ID] < NUM_SEATS))
			{
        next = i;
        break;
			}

		// If no floor is found, move to bottom
		if(next == -1) next = 0;
	}
}

void travel_when_kicked(const long &ID, int &curr, int move, bool &going_up)
{
	int next;

	while(move > 0)
	{
		int temp = curr;
		if(going_up)
			if(move + curr > 14)
				next = 14;
			else
				next = move+curr;
		else
			if(curr - move < 0)
				next = 0;
			else
				next = curr-move;
		travel(ID, curr, next, going_up);
		move = move - abs(curr - temp);
	}
}

void travel(const long &ID, int &curr, int &next, bool &going_up)
{
	long temp = 0;
	temp = (ID+1 >= ELEVATORS ? 0 : ID+1);	
	
	if(curr != next) hold(BASE_SPEED);
	
	while(1)
	{
		if((*events[ID])[curr].state() == OCC)
	  {
		  while((*get_out[ID])[curr].queue_cnt() > 0)
		  {
			// Let them off one by one. Use synchronous_send instead of send
			// for mailbox
			  (*get_out[ID])[curr].synchronous_send(ID);
			  seats_used[ID]--;
		  }
		  while(seats_used[ID] < NUM_SEATS && going_up && (*mb_up)[curr].queue_cnt() > 0)
		  {
			// Let them in one by one. Use synchronous_send instead of send
			// for mailbox
			  (*mb_up)[curr].synchronous_send(ID);
			  seats_used[ID]++;
			  hold(1);
		  }
		  while(seats_used[ID] < NUM_SEATS && !going_up && (*mb_dn)[curr].queue_cnt() > 0)
		  {
		  	(*mb_dn)[curr].synchronous_send(ID);
		  	seats_used[ID]++;
		  	hold(1);
		  }
		  if((*get_out[ID])[curr].queue_cnt() == 0 && (*mb_up)[curr].queue_cnt() == 0 && (*mb_dn)[curr].queue_cnt() == 0)
		    (*events[ID])[curr].clear();
	  }
	  if(curr == next)
			break;
	  if(curr != next)
		{
			hold(FLOOR_SPEED);
			if(going_up)
			{
				if((*up_shaft)[curr+1].state() == OCC)
				{
					temp = (ID+1 >= ELEVATORS ? 0 : ID+1);
					if(busy[temp])
					{
						(*up_shaft)[curr+1].wait();
						(*up_shaft)[curr].clear();
						(*up_shaft)[curr+1].set();
					}
					else
					{
						(*events[temp])[FLOORS].set();
						(*up_shaft)[curr+1].wait();
						(*up_shaft)[curr].clear();
						(*up_shaft)[curr+1].set();
					}
					break;
				}
				else
				{
					(*up_shaft)[curr].clear();
					(*up_shaft)[curr+1].set();					
				}
				curr++;
			}
			else
			{
				
				if((*down_shaft)[curr-1].state() == OCC)
				{
					temp = (ID+1 >= ELEVATORS ? 0 : ID+1);
					if(busy[temp])
					{
						(*down_shaft)[curr-1].wait();
						(*down_shaft)[curr].clear();
						(*down_shaft)[curr-1].set();
					}
					else
					{
						(*events[temp])[FLOORS].set();
						(*down_shaft)[curr-1].wait();
						(*down_shaft)[curr].clear();
						(*down_shaft)[curr-1].set();						
					}
					break;
				}
				else
				{
					(*down_shaft)[curr].clear();
					(*down_shaft)[curr-1].set();					
				}
				curr--;
			}
		}

	  if(curr == 14 && going_up)
	  {
			if((*down_shaft)[curr].state() == OCC)
			{
				temp = (ID+1 >= ELEVATORS ? 0 : ID+1);
				if(busy[temp])
				{
					(*down_shaft)[curr].wait();
					(*up_shaft)[curr].clear();
					(*down_shaft)[curr].set();
				}
				else
				{
					(*events[temp])[FLOORS].set();
					(*down_shaft)[curr].wait();
					(*up_shaft)[curr].clear();
					(*down_shaft)[curr].set();
				}
			}
			else
			{
				(*up_shaft)[curr].clear();
				(*down_shaft)[curr].set();					
			}
			going_up = !going_up;
			break;
		}
		
	  if(curr == 0 && !going_up)
		{
			if((*up_shaft)[curr].state() == OCC)
			{
				temp = (ID+1 >= ELEVATORS ? 0 : ID+1);
				if(busy[temp])
				{
					(*up_shaft)[curr].wait();
					(*down_shaft)[curr].clear();
					(*up_shaft)[curr].set();
				}
				else
				{
					(*events[temp])[FLOORS].set();
					(*up_shaft)[curr].wait();
					(*down_shaft)[curr].clear();
					(*up_shaft)[curr].set();
				}
			}
			else
			{
				(*down_shaft)[curr].clear();
				(*up_shaft)[curr].set();					
			}
			going_up = !going_up;
			break;
		}
	}

}

void elevator_clockwise(int start, const long &ID)
{
	create("elevator up!");
	int curr = start;
	int next = start;
	bool going_up = true;
	(*up_shaft)[start].set();
	while(1)
	{
		long whichone = events[ID]->wait_any();
		(*events[ID])[whichone].set();   //Have to do this to make sure the event is
		busy[ID] = true;  							 //still set for service later
		
		// Find out where to go next. If destination is on the other side of
		// the building, go to the top (or bottom) so it can switch side
		if((*events[ID])[FLOORS].state() == OCC)
		{
			(*events[ID])[FLOORS].clear();
			travel_when_kicked(ID, curr, kicked, going_up);
		}
		else
		{
			scan_next(curr,next,going_up,ID);
			travel(ID,curr,next,going_up);
			busy[ID] = false; 
		}
	}
}
