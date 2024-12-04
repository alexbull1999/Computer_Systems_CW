#include <iostream>
#include <thread>
#include <semaphore>
#include <vector> // to store and keep track of threads
#include <chrono> // for use of timers
#include <cstdlib> //for random numbers
#include <ctime> //for seeding random number
#include <system_error> //for checking and trying to catch system errors

using namespace std;

// Create a Job struct, to enable us to assign jobIDs to each job
// ensuring each job is produced only once, and consumed only once
struct Job {
	int jobID; // Unique jobID
	int processingTime; // to be randomly assigned between 1-10
};

// Create a CircularQueue class using arrays

class CircularQueue{
	private:
		Job* queue; // pointer to the queue, which is an array of Job structs
		int front; // tracking the index position of the front of the queue
		int rear; // tracking the index position of the rear of the queue
		int num_items; // tracking the number of items in the queue
		int capacity; // the total size of the queue / array 

		// static int counters to keep track of # of times producers
		// add an item to queue/consumers remove an item from queue
		static int totalProduced;
		static int totalConsumed;
	

	public:
		//Constructor: front and rear set to -1 to indicate queue empty
		CircularQueue(int capacity) : front(-1), rear(-1), num_items(0),
			capacity(capacity) {
				queue = new Job[capacity];
			}

		//Destructor, as creating CircularQueue queue array on the heap
		~CircularQueue() {
			delete[] queue;
			cout << "Circular Queue object deleted\n";
		}

		// method to add a value (job) to the queue
		void addToQueue(const Job& job) {
			// check if queue is empty, if so update front. Otherwise only update rear
			if (front == -1) {
				front = 0;
			}
			// % capacity allows the queue to be circular, as when rear reaches
			// capacity, it will circle back round to 0, provided the array is not full
			rear = (rear + 1) % capacity;
			queue[rear] = job; //insert the job into the queue
			num_items++;
			totalProduced++;
		}

		Job removeFromQueue() {
			// update front index to take into account item removed, and spaces at
			// front of queue are available again for the rear if it circles back
			Job job = queue[front];
			front = (front + 1) % capacity;
			num_items--;
			totalConsumed++;
			//if queue is completely empty, reset front/rear to -1
			if (num_items == 0) {
				front = rear = -1;
			}
			return job;
		}
		
		// Getter functions
		static int getTotalProduced() {
			return totalProduced;
		}

		static int getTotalConsumed() {
			return totalConsumed;
		}

};

int CircularQueue::totalProduced = 0; //initializing the static int counters
int CircularQueue::totalConsumed = 0;

// Declaring the producer function, to be defined later
void producer(int id, int numJobs, CircularQueue& queue, binary_semaphore& queueMutex,
		counting_semaphore<>& spacesInQueue, counting_semaphore<>& itemsInQueue, 
		binary_semaphore& coutMutex, int& globalJobID);

//Declaring the consumer function, to be defined later
void consumer(int id, CircularQueue& queue, binary_semaphore& queueMutex,
		counting_semaphore<>& spacesInQueue, counting_semaphore<>& itemsInQueue, 
		binary_semaphore& coutMutex); 

// Writing the main function that takes in 4 command line arguments and calls
// the consumer and producer functions
int main(int argc, char* argv[]) {
	if (argc != 5) {
		cerr << argv[0] << " requires 4 command line arguments: queue size, "
			<< "number of jobs, number of producers and number of consumers\n";
		return 1;
	}

	int capacity = stoi(argv[1]);
	int numJobs = stoi(argv[2]);
	int numProducers = stoi(argv[3]);
	int numConsumers = stoi(argv[4]);

	// Construct circular queue and semaphores
	
	CircularQueue circQueue = CircularQueue(capacity);
	binary_semaphore queueMutex(1);
	// initialise semaphore for num of spaces in queue to total queue capacity,
	// as queue starts empty
	counting_semaphore<> spacesInQueue(capacity);
	counting_semaphore<> itemsInQueue(0);
	// initialise semaphore to guarantee mutual exclusive output to cout during
	// printing of timeout messages
	binary_semaphore coutMutex(1);

	int globalJobID = 1; // counter for unique Job IDs 

	srand(time(0)); // seeding random number to be used in producer function
							
	// create a vector of thread objects to store and manage the threads
	vector<thread> threads;

	// create producer threads and add them to the vector - passing each
	// producer thread created the args required by the producer function
	// Use try/catch to check for system errors
	for (int count = 0; count < numProducers; count++) {
		try {
			threads.push_back(thread(producer, count + 1, numJobs, ref(circQueue),
						ref(queueMutex), ref(spacesInQueue), ref(itemsInQueue), 
						ref(coutMutex), ref(globalJobID)));
		}
		catch (const system_error& error) {
			cerr << "Error creating producer thread " << count + 1 << ": " <<
				error.what() << endl;
			return 1;
		}
	}

	//Create consumer threads and add them to the vector - passing each
	//consumer thread the arguments required for the consumer function
	//Use try/catch to check for system errors
	for (int count = 0; count < numConsumers; count++) {
		try{
			threads.push_back(thread(consumer, count + 1, ref(circQueue),
						ref(queueMutex), ref(spacesInQueue), ref(itemsInQueue), 
						ref(coutMutex)));
		}
		catch (const system_error& error) {
		cerr << "Error creating consumer thread " << count + 1 << ": " <<
			error.what() << endl;
		return 1;
		}
	}

	// join all threads to ensure they all complete their tasks / finish
	// execution (or timeout) before the main program exits
	// Use try/catch together with .joinable() method to check for system
	// errors and catch anything going wrong
	for (thread& t : threads) {
		try {
			if (t.joinable()) {
				t.join();
			}
		}
		catch (const system_error& error) {
			cerr << "Error during thread join: " << error.what() << endl;
			return 1;
		}
	}

	// if code reaches this stage, all jobs should have successfully been produced
	// and consumed - or we will have received appropriate error messages


	cout << circQueue.getTotalProduced() << " jobs produced and " <<
		circQueue.getTotalConsumed() << " jobs consumed successfully\n";
	return 0;
}

/* Writing the Producer function: Each producer has an id (different for each producer
 * to help identify which, if any, producer times out), the number of jobs 
 * they need to complete (constant across producers), and then references to
 * "shared resources" between producers and consumers (e.g. the queue, the
 * semaphores) */

void producer(int id, int numJobs, CircularQueue& queue, binary_semaphore& queueMutex,
		counting_semaphore<>& spacesInQueue, counting_semaphore<>& itemsInQueue, 
		binary_semaphore& coutMutex, int& globalJobID) {

	// loop to carry out the number of jobs required of each producer
	for (int i = 0; i < numJobs; i++) {
		int processingTime = rand() % 10 + 1; //job processingTime is random 1-10
		
		// Try to acquire an empty space in the queue
		if (!spacesInQueue.try_acquire_for(chrono::seconds(10))) {
			coutMutex.acquire();
			cout << "Producer " << id << " timed out waiting for a queue space\n";
			coutMutex.release();
			return;
		}

		// If empty space successfully acquired, lock the queue for mutual exclusion
		queueMutex.acquire();
		int jobID = globalJobID++;
		Job job = {jobID, processingTime};
		queue.addToQueue(job); 
		cout << "Producer " << id << " added job with ID " << jobID <<
			" and processing time " << processingTime << endl;

		// we can now release the queue semaphore, for others to access
		queueMutex.release();

		// we can now also increment the itemsInQueue semaphore to allow a consumer
		// to consume the job we have just put on the queue
		itemsInQueue.release();
	}

}


/* Writing the Consumer function: Each consumer has an id (different for each
 * consumer), and references to the "shared resources" between producers and
 * consumers (e.g. the queue, the semaphores) */

void consumer(int id, CircularQueue& queue, binary_semaphore& queueMutex,
		counting_semaphore<>& spacesInQueue, counting_semaphore<>& itemsInQueue, 
		binary_semaphore& coutMutex) {

	// Creating an infinite loop, so only way Consumers terminate is if they timeout
	// and no further jobs have been added in the 10 second period
	while (true) {
		// Wait for an item to be added to queue, to consume, or timeout
		// We only time consumers out if there are items in queue to be
		// consumed that they can access due to semaphores (not if all items
		// produced have already been consumed)
		if (!itemsInQueue.try_acquire_for(chrono::seconds(10))) {
			coutMutex.acquire();
			cout << "Consumer " << id << " timed out waiting for a job\n";
			coutMutex.release();
			return;
		}

		// If item in queue sucessfully acquired through above semaphore, lock the
		// queue for mutual exclusion
		
		queueMutex.acquire();
		Job job = queue.removeFromQueue();
		cout << "Consumer " << id << " is consuming job with ID " << job.jobID << 
			" with time " << job.processingTime << " seconds.\n";
		// release the mutex queue semaphore
		queueMutex.release();
		// increment the empty slots semaphore, as we have just taken a job
		spacesInQueue.release();
		// Consumers are supposed to 'sleep' for the value indicated by the job
		this_thread::sleep_for(chrono::seconds(job.processingTime));
	}
}


