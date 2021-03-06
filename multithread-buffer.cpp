// Author : Beier Chen
// Create Date: April 28, 2018
// Application: 554 HW 6

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <vector>
using namespace std;

mutex m1;
condition_variable prod_cv, part_cv;
vector<int> buffer(4, 0);

int seed = 10; // for generating unrepeated random numbers

// for printing 
int print_buffer_type = 0;
int print_place_type = 1;
int print_pickup_type = 2;

// for detecting deadlock
int wait_prodW_num = 0;
int wait_partW_num = 0;
int alive_partW_num = 0;
int alive_prodW_num = 0;

void printState(int print_type, bool is_print_updated, const vector<int> * vec = & buffer)
{
	if (is_print_updated)
	{
		cout << "Updated ";
	}

	switch (print_type)
	{
	case 0: cout << "Buffer State: ("; break;
	case 1: cout << "Place Request: ("; break;
	case 2: cout << "Pickup Request: ("; break;
	}

	for (int count = 0; count < 4; count++)
	{
		if (count != 3)
		{
			cout << (*vec)[count] << ",";
		}
		else
		{
			cout << (*vec)[count] << ")" << endl;
		}
	}
}

// generate four pieces of parts from exactly two types of parts such as (3,1,0,0), (2,2,0,0), (0,0,1,3)
void generatePickupReq(vector<int>* pickup_req)
{
	srand(seed++);
	vector<int> vec(4);

	// pick two types randomly
	int ran_type_1 = rand() % 4; // generate a number in (0, 3)
	int ran_type_2 = rand() % 4;
	while (ran_type_1 == ran_type_2)
	{
		ran_type_2 = rand() % 4;
	}

	// distribute the four parts to the picked two types randomly
	int ran_num = rand() % 3 + 1; // // generate a number in (1, 3)
	(*pickup_req)[ran_type_1] += ran_num;
	(*pickup_req)[ran_type_2] += (4 - ran_num);
}

// generate three types of parts(A, B, C, D) randomly, such as(1,1,1,0), (3,0,0,0), (1,2,0,0)s
void generatePlaceReq(vector<int>* place_req)
{
	srand(seed++);
	for (int i = 0; i < 3; i++)
	{
		int ran_num = rand() % 4; // generate a number in (0, 3)
		(*place_req)[ran_num]++;
	}
}

// check the state of buffer, if it's full, return true
bool checkBufferFull() 
{
	if (buffer[0] == 6 && buffer[1] == 5 && buffer[2] == 4 && buffer[3] == 3)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// check the state of buffer, if it's empty, return true
bool checkBufferEmpty()
{
	if (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 0)
	{
		cout << "buffer empty" << endl;
		return true;
	}
	else
	{
		return false;
	}
}

// place parts to buffer, if a place request is finished, return true
bool placeToBuffer(vector<int> * place_req)
{
	bool req_fin = true;
	for (int index = 0; index < 4; index++)
	{
		if ((*place_req)[index] != 0)
		{
			switch (index)
			{
			case 0: // A-type, capacity is 6
			{
				if (buffer[0] + (*place_req)[0] <= 6)
				{
					buffer[0] += (*place_req)[0];
					(*place_req)[0] = 0;
				}
				else
				{
					buffer[0] = 6;
					(*place_req)[0] = (*place_req)[0] - (6 - buffer[0]);
					req_fin = false;
				}
				break;
			}

			case 1: // B-type, capacity is 5
			{
				if (buffer[1] + (*place_req)[1] <= 5)
				{
					buffer[1] += (*place_req)[1];
					(*place_req)[1] = 0;
				}
				else
				{
					buffer[1] = 5;
					(*place_req)[1] = (*place_req)[1] - (5 - buffer[1]);
					req_fin = false;
				}
				break;
			}

			case 2: // C-type, capacity is 4
			{
				if (buffer[2] + (*place_req)[2] <= 4)
				{
					buffer[2] += (*place_req)[2];
					(*place_req)[2] = 0;
				}
				else
				{
					buffer[2] = 4;
					(*place_req)[2] = (*place_req)[2] - (4 - buffer[2]);
					req_fin = false;
				}
				break;
			}

			case 3: // D-type, capacity is 3
			{
				if (buffer[3] + (*place_req)[3] <= 3)
				{
					buffer[3] += (*place_req)[3];
					(*place_req)[3] = 0;
				}
				else
				{
					buffer[3] = 3;
					(*place_req)[3] = (*place_req)[3] - (3 - buffer[3]);
					req_fin = false;
				}
				break;
			}
			}
		}
	}	
	
	if (req_fin) 
	{
		//cout << " - this place request finished" << endl;
		return true;
	}
	else
	{
		//cout << " - this place request doesn't finish" << endl;
		return false;
	}
}

// pick parts from buffer, if a pickup request is finished, return true
bool pickFromBuffer(vector<int> * pickup_req)
{
	bool req_fin = true;
	for (int index = 0; index < 4; index++)
	{
		if ((*pickup_req)[index] != 0)
		{
			if (buffer[index] - (*pickup_req)[index] >= 0)
			{
				buffer[index] -= (*pickup_req)[index];
				(*pickup_req)[index] = 0;
			}
			else
			{
				(*pickup_req)[index] -= buffer[index];
				buffer[index] = 0;
				req_fin = false;
			}
		}
	}

	if (req_fin)
	{
		//cout << " - this pickup request finished" << endl;
		return true;
	}
	else
	{
		//cout << " - this pick request doesn't finish" << endl;
		return false;
	}
}

// generate new place request and place parts to the buffer
// # if finish a place request, notify product workers; when awaken, begin the next iteration;
// # if not finish, wait; when awaken, try to continue the rest of the request
// # if the buffer is full, notify product workers
void PartWorker(int ID) {

	int iteration = 5;
	vector<int> * place_req = new vector<int>(4);
	bool req_fin = false;

	alive_partW_num++;
	
	while (iteration > 0)
	{
		unique_lock<mutex> ulock1(m1);
		//cout << "\n - alive_partW_num: " << alive_partW_num;

		generatePlaceReq(place_req);

		cout << "\nPart Worker ID: " << ID << endl; // Part Worker ID : 8
		cout << "Iteration: " << iteration << endl; // Iteration : 2
		printState(print_buffer_type, false); // Buffer State : (5, 2, 3, 2)
		printState(print_place_type, false, place_req); // Place Request : (2, 0, 1, 0)

		req_fin = placeToBuffer(place_req);

		printState(print_buffer_type, true);
		printState(print_place_type, true, place_req);

		while (!req_fin)
		{
			wait_partW_num++;
			if (wait_partW_num == alive_partW_num || wait_prodW_num == alive_prodW_num)
			{
				alive_partW_num--;
				wait_partW_num--;
				if (alive_partW_num == 0)
				{
					cout << "Return" << endl;
					return;
				}
				//cout << "- wait_partW_num: " << wait_partW_num << endl;
				//cout << "- alive_partW_num: " << alive_partW_num << endl;
				cout << "Return" << endl;
				return;
			}

			part_cv.wait(ulock1);
			wait_partW_num--;

			cout << "\nPart Worker ID: " << ID << endl; // Part Worker ID : 8
			cout << "Iteration: " << iteration << endl; // Part Worker ID : 8
			printState(print_buffer_type, false); // Buffer State : (5, 2, 3, 2)
			printState(print_place_type, false, place_req); // Place Request : (2, 0, 1, 0)

			req_fin = placeToBuffer(place_req);

			printState(print_buffer_type, true); // Updated Buffer State : (6, 2, 4, 2)
			printState(print_place_type, true, place_req); // Updated Place Request : (1, 0, 0, 0)		
		}

		// if buffer size is (6, 5, 4, 3), notify produce worker, yield the lock
		if (checkBufferFull) 
		{
			prod_cv.notify_one();
		}	

		//cout << "part worker: prod_cv.notify_all()" << endl;
		prod_cv.notify_all(); // better then prod_cv.notify_one(); 

		iteration--;
		//if(iteration == 0)
		//	cout << "Part worker" << ID << ", after 5 iteration, return" << endl;
	}

	if (!checkBufferFull)
	{
		cout << "part_cv.notify_one()" << endl;
		part_cv.notify_one();
	}

	alive_partW_num--;
	delete place_req;
	return;
}

// generate new pick request and pick parts from the buffer
// # if finish a pickup request, notify parts workers; when awaken, begin the next iteration;
// # if not finish, wait; when awaken, try to continue the rest of the request
// # if the buffer is empty, notify product workers
void ProductWorker(int ID)
{
	int iteration = 5;
	vector<int> * pickup_req = new vector<int>(4);
	bool req_fin = false;

	alive_prodW_num++;
	
	while (iteration > 0)
	{
		unique_lock<mutex> ulock1(m1);
		//cout << "\n - alive_prodW_num: " << alive_prodW_num;

		if (checkBufferEmpty)
		{
			part_cv.notify_all();
		}
			
		generatePickupReq(pickup_req);
		cout << "\nProduct Worker ID: " << ID << endl; // Part Worker ID : *
		cout << "Iteration: " << iteration << endl; // Iteration : *
		printState(print_buffer_type, false); // Buffer State : (*, *, *, *)
		printState(print_pickup_type, false, pickup_req); // Pickup Request : (*, *, *, *)

		req_fin = pickFromBuffer(pickup_req);

		printState(print_buffer_type, true); // Updated Buffer State : (*, *, *, *)
		printState(print_pickup_type, true, pickup_req); // Updated Pickup Request : (*, *, *, *)

		while (!req_fin)
		{
			wait_prodW_num++;
			if (wait_prodW_num == alive_prodW_num || wait_partW_num == alive_partW_num)
			{
				alive_prodW_num--;
				wait_prodW_num--;
				if (alive_prodW_num == 0)
				{
					cout << "Return" << endl;
					return;
				}
				//cout << " - wait_prodW_num: " << wait_prodW_num << endl;
				//cout << " - alive_prodW_num: " << alive_prodW_num << endl;
				cout << "Return" << endl;
				return;
			}

			prod_cv.wait(ulock1);
			wait_prodW_num--;

			cout << "\nProduct Worker ID: " << ID << endl; // Part Worker ID : *
			cout << "Iteration: " << iteration << endl; // Iteration : *
			printState(print_buffer_type, false); // Buffer State : (*, *, *, *)
			printState(print_pickup_type, false, pickup_req); // Pickup Request : (*, *, *, *)

			req_fin = pickFromBuffer(pickup_req);

			printState(print_buffer_type, true); // Updated Buffer State : (*, *, *, *)
			printState(print_pickup_type, true, pickup_req); // Updated Pickup Request : (*, *, *, *)
		}

		iteration--;
		part_cv.notify_all();
		//if (iteration == 0)
		//	cout << "Part worker" << ID << ", after 5 iteration, return" << endl;
	}

	if (!checkBufferEmpty)
	{
		prod_cv.notify_one();
	}
	alive_prodW_num --;
	delete pickup_req;
	return;
}

int main() 
{
	const int m = 16, n = 12;
	thread partW[m];
	thread prodW[n];

	for (int i = 0; i < n; i++) {
		partW[i] = thread(PartWorker, i);
		prodW[i] = thread(ProductWorker, i);
	}
	for (int i = n; i<m; i++) {
		partW[i] = thread(PartWorker, i);
	}

	/* Join the threads to the main threads */
	for (int i = 0; i < n; i++) {
		partW[i].join();
		prodW[i].join();
	}

	for (int i = n; i<m; i++) {
		partW[i].join();
	}

	cout << "Finish!" << endl;
	getchar();
	getchar();
	return 0;
}
