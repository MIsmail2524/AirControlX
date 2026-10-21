#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <queue>
#include <vector>
#include <map>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>

using namespace std;


const char* Flight_Pipe = "/tmp/flight_fifo";      // pipe for flight data
const char* Violation_Pipe= "/tmp/violation_fifo"; // pipe for violation data

// Global variables 
int GlobalAVNID = 0;                  
const int Total_Time = 300;   
bool simulation_running = true;        
int TotalFlights = 14;                 
double timePassed = 0;                 
vector<thread> ChangePhaseThread;       
mutex violation_write_mutex;            
int violation_fd;                      


class airline {
public:
    string name;    
    string type;    
    int aircrafts;  
    int flights;      

    airline() : name(""), type(""), aircrafts(0), flights(0) {}
    airline(string name, string type, int aircrafts, int flights) 
        : name(name), type(type), aircrafts(aircrafts), flights(flights) {}
};


class airplane {
public:
    int id;          
    int speed;        
    int height;      
    airline airLine;  
    bool violation;   
    bool faulty;      
    string direction; 
    int x, y;         
    float fuel;       

    airplane() : id(0), speed(0), height(0), violation(false), faulty(false), direction(""), x(0), y(0) {}
    airplane(int id, int speed, int height, airline airLine, string dir, string texture, int x, int y) 
        : id(id), speed(speed), height(height), airLine(airLine), violation(false), faulty(false), direction(dir), x(x), y(y) {}
};


class runway {
public:
    mutex RWYMutex;   
    char type;        
    bool free;        

    runway() : type(' '), free(true) {}
    runway(char type) : type(type), free(true) {}

    // Request access to the runway 
    bool request_access() {
        lock_guard<mutex> lock(RWYMutex);
        if (free) {
            free = false;
            return true;
        }
        return false;
    }

    // Release the runway
    void release() {
        lock_guard<mutex> lock(RWYMutex);
        free = true;
    }

    // Initialize runway state for simulation
    void MakeThread() {
        this->type = ' ';
        this->free = true;
    }
};

/*
// Get the current date and time as a formatted string
string getCurrentDateTime() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buffer);
}

// Calculate due date for a violation (3 days from issuance date)
string calculateDueDate(const string& issuanceDate) {
    int year, month, day, hour, minute, second;
    char dash, colon, space;
    istringstream iss(issuanceDate);
    iss >> year >> dash >> month >> dash >> day >> space >> hour >> colon >> minute >> colon >> second;

    // Add 3 days to the issuance date
    day += 3;
    int daysInMonth;
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: daysInMonth = 31; break;
        case 4: case 6: case 9: case 11: daysInMonth = 30; break;
        case 2: daysInMonth = 28; break; // Simplified: doesn't account for leap years
        default: daysInMonth = 30;
    }
    if (day > daysInMonth) {
        day -= daysInMonth;
        month++;
        if (month > 12) {
            month = 1;
            year++;
        }
    }

    // Format the due date
    ostringstream oss;
    oss << year << "-" << (month < 10 ? "0" : "") << month << "-" << (day < 10 ? "0" : "") << day << " "
        << (hour < 10 ? "0" : "") << hour << ":" << (minute < 10 ? "0" : "") << minute << ":" << (second < 10 ? "0" : "") << second;
    return oss.str();
}

// Extract permissible speed range from violation details
string extractPermissibleSpeed(const string& details) {
    size_t start = details.find('[');
    size_t end = details.find(']');
    if (start != string::npos && end != string::npos && end > start) {
        return details.substr(start, end - start + 1);
    }
    return "Unknown";
}
*/
// Class to represent a violation issued to a flight
class Violation {
public:
    int avnID;             
    string airlineName;    
    int flightNumber;       
    string aircraftType;    
    int recordedSpeed;      
    string permissibleSpeed;
    string issuanceDateTime;
    int totalFineAmount;    
    bool paymentStatus;     
    string dueDate;         
    string details;        

    Violation() : avnID(0), flightNumber(0), recordedSpeed(0), totalFineAmount(0), paymentStatus(false) {}

    Violation(int avnID, int flightID, airplane aplane, bool violation, int recordedSpeed, int fineamount, bool paymentStatus, string details) 
        : avnID(avnID), airlineName(aplane.airLine.name), flightNumber(flightID), aircraftType(aplane.airLine.type), 
          recordedSpeed(recordedSpeed), permissibleSpeed(extractPermissibleSpeed(details)), issuanceDateTime(getCurrentDateTime()), 
          paymentStatus(paymentStatus), dueDate(calculateDueDate(getCurrentDateTime())), details(details) {
        // Calculate fine based on aircraft type
        if(aircraftType == "Commercial")
        {
         int baseFine = 500000;
        }
        else if (aircraftType == "Cargo")
        {
         int baseFine = 700000;
        }
        else
        {
        int baseFine = 500000;
        }

        double serviceFee = baseFine * 0.15;
        totalFineAmount = static_cast<int>(baseFine + serviceFee);
    }
};

/*
vector<Violation> readViolationsFromFile(const string& filename, int& totalViolations) {
    vector<Violation> violations;
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file '" << filename << "' for reading." << endl;
        totalViolations = 0;
        return violations;
    }

    string line;
    // total number of violations from the first line
    if (getline(infile, line)) {
       
            totalViolations = stoi(line);
        
    } else {
        totalViolations = 0;
    }

    Violation v;
    int fieldCount = 0;
    // Parsing the file line by line
    while (getline(infile, line)) {
        if (line.find("AVN ID: ") == 0) {
            v.avnID = stoi(line.substr(8));
            fieldCount = 1;
        } else if (line.find("Airline name: ") == 0) {
            v.airlineName = line.substr(14);
            fieldCount++;
        } else if (line.find("Flight number: ") == 0) {
            v.flightNumber = stoi(line.substr(15));
            fieldCount++;
        } else if (line.find("Aircraft type: ") == 0) {
            v.aircraftType = line.substr(15);
            fieldCount++;
        } else if (line.find("Speed recorded vs. permissible: ") == 0) {
            string speedData = line.substr(32);
            size_t vsPos = speedData.find(" vs. ");
            v.recordedSpeed = stoi(speedData.substr(0, vsPos));
            v.permissibleSpeed = speedData.substr(vsPos + 5);
            fieldCount++;
        } else if (line.find("AVN issuance date and time: ") == 0) {
            v.issuanceDateTime = line.substr(28);
            fieldCount++;
        } else if (line.find("Total fine amount (including service fee): PKR ") == 0) {
            v.totalFineAmount = stoi(line.substr(46));
            fieldCount++;
        } else if (line.find("Payment status: ") == 0) {
            v.paymentStatus = (line.substr(16) == "Paid");
            fieldCount++;
        } else if (line.find("Due date: ") == 0) {
            v.dueDate = line.substr(10);
            fieldCount++;
        } else if (line.find("----------------------------------------") == 0) {
            if (fieldCount >= 9) {
                violations.push_back(v);
            }
            v = Violation();
            fieldCount = 0;
        }
    }
    infile.close();
    return violations;
}

// Append a violation to the violations file
void appendViolationToFile(const Violation& violation, const string& filename) {
    // Getting current total violations count 
    int totalViolations = 0;
    vector<string> existingContent;
    ifstream infile(filename);
    string line;

    if (infile.is_open()) {
        if (getline(infile, line)) {
          
                totalViolations = stoi(line);
            
        }
        while (getline(infile, line)) {
            existingContent.push_back(line);
        }
        infile.close();
    }

    //Incrementing the total violations count
    totalViolations++;

    // New Violation Entry
    string violationStr = "AVN ID: " + to_string(violation.avnID) + "\n" +
                         "Airline name: " + violation.airlineName + "\n" +
                         "Flight number: " + to_string(violation.flightNumber) + "\n" +
                         "Aircraft type: " + violation.aircraftType + "\n" +
                         "Speed recorded vs. permissible: " + to_string(violation.recordedSpeed) + " vs. " + violation.permissibleSpeed + "\n" +
                         "AVN issuance date and time: " + violation.issuanceDateTime + "\n" +
                         "Total fine amount (including service fee): PKR " + to_string(violation.totalFineAmount) + "\n" +
                         "Payment status: " + (violation.paymentStatus ? "Paid" : "Unpaid") + "\n" +
                         "Due date: " + violation.dueDate + "\n" +
                         "----------------------------------------\n";

    //  Writting back to the file
    ofstream outfile(filename, ios::trunc); 
    if (!outfile.is_open()) {
        cerr << "Error: Could not open file '" << filename << "' for writing." << endl;
        return;
    }

    outfile << totalViolations << "\n"; 
    for (const auto& contentLine : existingContent) {
        outfile << contentLine << "\n"; 
    }
    outfile << violationStr; 
    outfile.close();
}


void appendViolationFromData(int flightID, string airlineName, string aircraftType, int recordedSpeed, string details, const string& filename) {
    // Get current total violations count
    int totalViolations = 0;
    vector<string> existingContent;
    ifstream infile(filename);
    string line;

    if (infile.is_open()) {
        if (getline(infile, line)) {

                totalViolations = stoi(line);
           
        }
        while (getline(infile, line)) {
            existingContent.push_back(line);
        }
        infile.close();
    }

    //  Increment the total violations count 
    totalViolations++;
    int avnID = totalViolations;

    // New Entry Object
    airplane dummyPlane;
    dummyPlane.airLine.name = airlineName;
    dummyPlane.airLine.type = aircraftType;
    dummyPlane.speed = recordedSpeed;
    Violation newViolation(avnID, flightID, dummyPlane, true, recordedSpeed, 0, false, details);

    //  new violation entry as a string
    string violationStr = "AVN ID: " + to_string(newViolation.avnID) + "\n" +
                         "Airline name: " + newViolation.airlineName + "\n" +
                         "Flight number: " + to_string(newViolation.flightNumber) + "\n" +
                         "Aircraft type: " + newViolation.aircraftType + "\n" +
                         "Speed recorded vs. permissible: " + to_string(newViolation.recordedSpeed) + " vs. " + newViolation.permissibleSpeed + "\n" +
                         "AVN issuance date and time: " + newViolation.issuanceDateTime + "\n" +
                         "Total fine amount (including service fee): PKR " + to_string(newViolation.totalFineAmount) + "\n" +
                         "Payment status: " + (newViolation.paymentStatus ? "Paid" : "Unpaid") + "\n" +
                         "Due date: " + newViolation.dueDate + "\n" +
                         "----------------------------------------\n";

// Writting back to file
    ofstream outfile(filename, ios::trunc); 
    if (!outfile.is_open()) {
        cerr << "Error: Could not open file '" << filename << "' for writing." << endl;
        return;
    }

    outfile << totalViolations << "\n"; 
    for (const auto& contentLine : existingContent) {
        outfile << contentLine << "\n";
    }
    outfile << violationStr; 
    outfile.close();
}
*/
// Global containers for violations and simulation state
vector<Violation> violations;

// Define airline objects with their properties
airline Airline1("PIA", "Commercial", 6, 4);
airline Airline2("AirBlue", "Commercial", 4, 4);
airline Airline3("Fed EX", "Cargo", 3, 2);
airline Airline4("Pakistan AirForce", "Military", 2, 1);
airline Airline5("Blue Dart", "Cargo", 2, 2);
airline Airline6("AghaKhan AirAmbulance", "Medical", 2, 1);

// Queues for managing arriving and departing airplanes
queue<airplane*> arrival_queue;
queue<airplane*> departure_queue;
mutex queue_mutex; // Mutex for thread-safe queue access


runway RWYA('A'); // Runway A ( arrivals)
runway RWYB('B'); // Runway B ( departures)
runway RWYC('C'); // Runway C (for emergency and cargo flights)


const int NUM_FLIGHTS = 14;
queue<thread> arrivalThreadFlights;   
queue<thread> departureThreadFlights; 
queue<thread> emergencyThreadFlights; 
queue<thread> cargoThreadFlights;     
thread FlightThreads[NUM_FLIGHTS];   
const int NUM_PLANES = 19;
airplane* AirPlanes[NUM_PLANES];      


class flights {
public:
    char RunwayUsed;       
    int flightID;          
    airplane* aplane;      
    string flightType;        
    string direction;         
    int flightInterval;       
    int emergencyProbability; 
    int startingTime;         
    int remainingTime;        
    bool Pushed;              
    string phase;             
    int x, y;                 

    flights() : flightID(0), aplane(nullptr), flightType(""), direction(""), flightInterval(0),
                emergencyProbability(0), startingTime(0), remainingTime(0), Pushed(false), phase(""), RunwayUsed(' '), x(0), y(0) {}
    flights(int flightID, airplane* aplane, string flightType, string direction, int startingTime, 
            int emergencyProbability, bool pushed, string phase, char RunwayUsed) 
        : flightID(flightID), aplane(aplane), flightType(flightType), direction(direction), startingTime(startingTime), 
          emergencyProbability(emergencyProbability), Pushed(pushed), phase(phase), RunwayUsed(RunwayUsed) {
        this->x = aplane->x;
        this->y = aplane->y;
    }


    void MakeFlightThread() {}

    // Arrival violations based on speed and phase
    bool checkArrivalViolation(string& violationDetails) {
        violationDetails = "";
        if (phase == "Holding" && (aplane->speed > 600 || aplane->speed < 400)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [400, 600] in Holding phase";
            return true;
        } else if (phase == "Approach" && (aplane->speed > 290 || aplane->speed < 240)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [240, 290] in Approach phase";
            return true;
        } else if (phase == "Landing" && (aplane->speed > 240 || aplane->speed < 30)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [30, 240] in Landing phase";
            return true;
        } else if (phase == "Taxi" && (aplane->speed > 30 || aplane->speed < 15)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [15, 30] in Taxi phase";
            return true;
        } else if (phase == "At Gate" && (aplane->speed > 5 || aplane->speed < 0)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [0, 5] in At Gate phase";
            return true;
        }
        return false;
    }

    // Departure violations based on speed, height, and phase
    bool checkDepartureViolation(string& violationDetails) {
        violationDetails = "";
        if (phase == "At Gate" && (aplane->speed > 5 || aplane->speed < 0)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [0, 5] in At Gate phase";
            return true;
        } else if (phase == "Taxi" && (aplane->speed > 30 || aplane->speed < 15)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [15, 30] in Taxi phase";
            return true;
        } else if (phase == "Takeoff Roll" && (aplane->speed > 290 || aplane->speed < 0)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [0, 290] in Takeoff Roll phase";
            return true;
        } else if (phase == "Climb" && (aplane->speed > 463 || aplane->speed < 250)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [250, 463] in Climb phase";
            return true;
        } else if (phase == "Cruise" && (aplane->speed > 900 || aplane->speed < 800)) {
            violationDetails = "Speed " + to_string(aplane->speed) + " out of range [800, 900] in Cruise phase";
            return true;
        } else if (phase == "Cruise" && aplane->height > 35000) {
            violationDetails = "Height " + to_string(aplane->height) + " exceeds 35000 in Cruise phase";
            return true;
        }
        return false;
    }

    // Continuously change flight phases until completion or fault
    void continuosChangePhases() {
        while (true) {
            if (aplane->faulty) {
                cout << "Plane " << flightID << " is faulty and will be grounded.\n";
                if (RunwayUsed == 'A') RWYA.release();
                else if (RunwayUsed == 'B') RWYB.release();
                else if (RunwayUsed == 'C') RWYC.release();
                phase = "Grounded";
                break;
            }
            if ((direction == "North" || direction == "South") && phase != "At Gate") {
                ChangeArrivalPhases();
                if (phase == "At Gate") {
                    cout << "Plane: " << flightID << " landed Safely\n";
                    if (RunwayUsed == 'A') RWYA.release();
                    if (RunwayUsed == 'C') RWYC.release();
                    break;
                }
            } else if (!(direction == "North" || direction == "South") && phase != "Cruise") {
                ChangeDeparturePhases();
                if (phase == "Climb") {
                    cout << "Plane " << flightID << " Left Runway Safely\n";
                    if (RunwayUsed == 'B') RWYB.release();
                    if (RunwayUsed == 'C') RWYC.release();
                }
                if (phase == "Cruise") break;
            } else break;
            this_thread::sleep_for(chrono::seconds(2));
        }
    }

    // Change phases for departing flights
    void ChangeDeparturePhases() {
        if (phase != "Cruise") {
            string currentPhase = phase;
            // Check for faults in early phases
            if (phase == "At Gate" || phase == "Taxi") {
                int fault = rand() % 4;
                if (fault == 0) {
                    aplane->faulty = true;
                    cout << "Faulty Plane: " << aplane->id << " has shown fault in " << phase << " phase\n";
                    return;
                }
            }
            // Transition through departure phases
            if (phase == "At Gate") {
                phase = "Taxi";
                aplane->speed = rand() % 6;
                aplane->height = 0;
            } else if (phase == "Taxi") {
                phase = "Takeoff Roll";
                aplane->speed = 15 + rand() % 16;
                aplane->height = rand() % 100;
            } else if (phase == "Takeoff Roll") {
                phase = "Climb";
                aplane->speed = 150 + rand() % 140;
                aplane->height = rand() % 10000;
            } else if (phase == "Climb") {
                phase = "Cruise";
                aplane->speed = 250 + rand() % 214;
                aplane->height = 30000 + rand() % 50000;
            }

            cout << "Phase of " << flightID << " changes from " << currentPhase
                 << " to: " << phase << " | Speed: " << aplane->speed
                 << " | Height: " << aplane->height << endl;

            // Violations after phase change
            string violationDetails;
            if (checkDepartureViolation(violationDetails)) {
                cout << "Departure Violation detected for " << flightID << ": " << violationDetails << endl;
                string message = to_string(flightID) + ":" + aplane->airLine.name + ":" + aplane->airLine.type + ":" +
                                 to_string(aplane->speed) + ":" + violationDetails + "\n";
                lock_guard<mutex> lock(violation_write_mutex);
                write(violation_fd, message.c_str(), message.length());
            }
        }
    }

    // Change phases for arriving flights
    void ChangeArrivalPhases() {
        if (phase != "At Gate") {
            string currentPhase = phase;
            // Check for faults in critical phases
            if (phase == "Landing" || phase == "Taxi") {
                int fault = rand() % 4;
                if (fault == 0) {
                    aplane->faulty = true;
                    cout << "Faulty Plane: " << aplane->id << " has shown fault in " << phase << " phase\n";
                    return;
                }
            }
            // Transition through arrival phases
            if (phase == "Cruise") {
                phase = "Holding";
                aplane->speed = 400 + rand() % 201;
                aplane->height = 25000 + rand() % 5000;
            } else if (phase == "Holding") {
                phase = "Approach";
                aplane->speed = 240 + rand() % 51;
                aplane->height = 5000 + rand() % 5000;
            } else if (phase == "Approach") {
                phase = "Landing";
                aplane->speed = 30 + rand() % 211;
                aplane->height = rand() % 500;
            } else if (phase == "Landing") {
                phase = "Taxi";
                aplane->speed = 15 + rand() % 16;
                aplane->height = 0;
            } else if (phase == "Taxi") {
                phase = "At Gate";
                aplane->speed = rand() % 6;
                aplane->height = 0;
            }

            cout << "Phase of " << flightID << " changes from " << currentPhase
                 << " to: " << phase << " | Speed: " << aplane->speed
                 << " | Height: " << aplane->height << endl;

            //Violations after phase change
            string violationDetails;
            if (checkArrivalViolation(violationDetails)) {
                cout << "Arrival Violation detected for " << flightID << ": " << violationDetails << endl;
                string message = to_string(flightID) + ":" + aplane->airLine.name + ":" + aplane->airLine.type + ":" +
                                 to_string(aplane->speed) + ":" + violationDetails + "\n";
                lock_guard<mutex> lock(violation_write_mutex);
                write(violation_fd, message.c_str(), message.length());
            }
        }
    }
};

//  Map to associate threads with flights
map<thread::id, flights*> getFlightsFromThreads;
flights* Flights[NUM_FLIGHTS]; 

// Initialize airplanes 
void InitializePlanes(airplane* AirPlanes[]) {
    AirPlanes[0] = new airplane(1, 0, 0, Airline1, "North", "plane1.png", 0, 300);
    AirPlanes[1] = new airplane(2, 0, 0, Airline1, "North", "plane1.png", 80, 300);
    AirPlanes[2] = new airplane(3, 0, 0, Airline1, "East", "plane1.png", 160, 300);
    AirPlanes[3] = new airplane(4, 0, 0, Airline1, "East", "plane1.png", 240, 300);
    AirPlanes[4] = new airplane(5, 0, 0, Airline1, "North", "plane1.png", 320, 300);
    AirPlanes[5] = new airplane(6, 0, 0, Airline1, "North", "plane1.png", 400, 300);
    AirPlanes[6] = new airplane(7, 0, 0, Airline2, "South", "plane1.png", 480, 300);
    AirPlanes[7] = new airplane(8, 0, 0, Airline2, "South", "plane1.png", 560, 300);
    AirPlanes[8] = new airplane(9, 0, 0, Airline2, "West", "plane1.png", 640, 300);
    AirPlanes[9] = new airplane(10, 0, 0, Airline2, "West", "plane1.png", 720, 300);
    AirPlanes[10] = new airplane(11, 0, 0, Airline3, "East", "plane1.png", 800, 300);
    AirPlanes[11] = new airplane(12, 0, 0, Airline3, "East", "plane1.png", 880, 300);
    AirPlanes[12] = new airplane(13, 0, 0, Airline3, "East", "plane1.png", 960, 300);
    AirPlanes[13] = new airplane(14, 0, 0, Airline4, "East", "plane1.png", 1040, 300);
    AirPlanes[14] = new airplane(15, 0, 0, Airline4, "East", "plane1.png", 1120, 300);
    AirPlanes[15] = new airplane(16, 0, 0, Airline5, "West", "plane1.png", 1200, 300);
    AirPlanes[16] = new airplane(17, 0, 0, Airline5, "West", "plane1.png", 1280, 300);
    AirPlanes[17] = new airplane(18, 0, 0, Airline6, "South", "plane1.png", 1360, 300);
    AirPlanes[18] = new airplane(19, 0, 0, Airline6, "South", "plane1.png", 1440, 300);
}

// Initialize flights
void InitializeFlights(flights* Flights[], airplane* AirPlanes[]) {
    int flight_count = 0;
    Flights[flight_count++] = new flights(flight_count, AirPlanes[0], "International", "North", 180, 10, false, "Approach", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[1], "International", "North", 180, 10, false, "Holding", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[2], "International", "East", 150, 15, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[3], "International", "East", 150, 15, false, "At Gate", ' ');   
    Flights[flight_count++] = new flights(flight_count, AirPlanes[6], "Domestic", "South", 120, 5, false, "Landing", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[7], "Domestic", "South", 120, 5, false, "Taxi", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[8], "Domestic", "West", 240, 20, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[9], "Domestic", "West", 240, 20, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[10], "Cargo", "East", 150, 15, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[11], "Cargo", "East", 150, 15, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[13], "Military", "East", 150, 15, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[15], "Cargo", "West", 240, 20, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[16], "Cargo", "West", 240, 20, false, "At Gate", ' ');
    Flights[flight_count++] = new flights(flight_count, AirPlanes[17], "Medical", "South", 120, 5, false, "Holding", ' ');
}

// Sort flights ..... on emergency probability
void ProbabilitySort(queue<thread>& flightThreads, flights* Flights[], int totalFlights) {
    vector<pair<thread, int>> threadPriorityPairs;
    while (!flightThreads.empty()) {
        thread t = move(flightThreads.front());
        flightThreads.pop();
        thread::id tid = t.get_id();
        flights* flight = getFlightsFromThreads[tid];
        threadPriorityPairs.emplace_back(move(t), flight->emergencyProbability);
    }
    sort(threadPriorityPairs.begin(), threadPriorityPairs.end(),
         [](const auto& a, const auto& b) { return a.second > b.second; });
    for (auto& pair : threadPriorityPairs) {
        flightThreads.push(move(pair.first));
    }
}

// Send flight data to the flight pipe  for visualization
void moveFlight(flights* Flight[], int index, char runway, bool beingProcessed) {
    if (index >= 0 && index < NUM_FLIGHTS && Flight[index] && Flight[index]->aplane && !Flight[index]->aplane->faulty) {
        Flight[index]->RunwayUsed = runway;
        string direction = Flight[index]->direction;
        string airline = Flight[index]->aplane->airLine.name;
        char position = runway;
        string phase = Flight[index]->phase;
        string height = to_string(Flight[index]->aplane->height);


        int fd = open(Flight_Pipe, O_WRONLY);
        if (fd == -1) {
            cout << "Error\n" << endl;
            return;
        }
        int stdout_dup = dup(1);
        if (stdout_dup == -1) {
            cerr << "Error\n" << endl;
            close(fd);
            return;
        }
        if (dup2(fd, 1) == -1) {
            cerr << "Error\n" << endl;
            close(stdout_dup);
            close(fd);
            return;
        }

        string pipeOutput = to_string(Flight[index]->flightID) + ":" + direction + ":" + position + ":" + phase + ":" + airline + ":" + (beingProcessed ? "1" : "0") + ":" + height + "\n";
        write(STDOUT_FILENO, pipeOutput.c_str(), pipeOutput.length());
        dup2(stdout_dup, STDOUT_FILENO);
        close(stdout_dup);
        close(fd);
    }
}

// Process flights from queues 
void moveDepartureFlight(flights* Flight[], int index) {
    // emergency flights on Runway C
    if (!emergencyThreadFlights.empty()) {
        if (RWYC.request_access()) {
            thread t = move(emergencyThreadFlights.front());
            emergencyThreadFlights.pop();
            thread::id tid = t.get_id();
            flights* CurrentFlight = getFlightsFromThreads[tid];
            ChangePhaseThread.emplace_back(&flights::continuosChangePhases, CurrentFlight);
            CurrentFlight->RunwayUsed = 'C';
            moveFlight(Flight, CurrentFlight->flightID - 1, 'C', true);
            TotalFlights--;
            this_thread::sleep_for(chrono::seconds(2));
            moveFlight(Flight, CurrentFlight->flightID - 1, 'C', false);
            if (t.joinable()) t.join();
        }
    } 
    // Process cargo flights on Runway C if no emergency flights
    else if (!cargoThreadFlights.empty()) {
        if (RWYC.request_access()) {
            thread t = move(cargoThreadFlights.front());
            cargoThreadFlights.pop();
            thread::id tid = t.get_id();
            flights* CurrentFlight = getFlightsFromThreads[tid];
            ChangePhaseThread.emplace_back(&flights::continuosChangePhases, CurrentFlight);
            CurrentFlight->RunwayUsed = 'C';
            moveFlight(Flight, CurrentFlight->flightID - 1, 'C', true);
            TotalFlights--;
            this_thread::sleep_for(chrono::seconds(2));
            moveFlight(Flight, CurrentFlight->flightID - 1, 'C', false);
            if (t.joinable()) t.join();
        }
    }

    // Process arrival flights on Runway A
    if (!arrivalThreadFlights.empty()) {
        if (RWYA.request_access()) {
            thread t = move(arrivalThreadFlights.front());
            arrivalThreadFlights.pop();
            thread::id tid = t.get_id();
            flights* CurrentFlight = getFlightsFromThreads[tid];
            ChangePhaseThread.emplace_back(&flights::continuosChangePhases, CurrentFlight);
            CurrentFlight->RunwayUsed = 'A';
            moveFlight(Flight, CurrentFlight->flightID - 1, 'A', true);
            TotalFlights--;
            this_thread::sleep_for(chrono::seconds(2));
            moveFlight(Flight, CurrentFlight->flightID - 1, 'A', false);
            if (t.joinable()) t.join();
        }
    }

    // Process departure flights on Runway B
    if (!departureThreadFlights.empty()) {
        if (RWYB.request_access()) {
            thread t = move(departureThreadFlights.front());
            departureThreadFlights.pop();
            thread::id tid = t.get_id();
            flights* CurrentFlight = getFlightsFromThreads[tid];
            ChangePhaseThread.emplace_back(&flights::continuosChangePhases, CurrentFlight);
            CurrentFlight->RunwayUsed = 'B';
            moveFlight(Flight, CurrentFlight->flightID - 1, 'B', true);
            TotalFlights--;
            this_thread::sleep_for(chrono::seconds(2));
            moveFlight(Flight, CurrentFlight->flightID - 1, 'B', false);
            if (t.joinable()) t.join();
        }
    }
}

// Main simulation function to manage flights
void Simulate(flights* Flights[], int total_flights) {
    auto start = chrono::steady_clock::now();
    vector<int> activeFlights; 

    // Initialize all flights in the flight FIFO
    for (int i = 0; i < total_flights; i++) {
        moveFlight(Flights, i, ' ', false);
    }


    while (timePassed <= Total_Time) {
        if (TotalFlights <= 0) { // Exit if all flights are processed
            break;
        }
        
        // Output simulation status to console
        int console_fd = open("/dev/tty", O_WRONLY);
        if (console_fd != -1) {
            int stdout_dup = dup(STDOUT_FILENO);
            dup2(console_fd, STDOUT_FILENO);
            string cliOutput = "[" + to_string(timePassed) + "s] Flights Remaining: " + to_string(TotalFlights) 
                             + " Active Flights: " + to_string(activeFlights.size()) + "\n";
            write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
            dup2(stdout_dup, STDOUT_FILENO);
            close(stdout_dup);
            close(console_fd);
        }

        // Update elapsed time
        auto now = chrono::steady_clock::now();
        timePassed = chrono::duration_cast<chrono::milliseconds>(now - start).count() / 1000.0;


        if (timePassed >= 180) {
            for (int i = 0; i < total_flights; i++) {
                if (Flights[i]->direction == "North" && !Flights[i]->Pushed) {
                    lock_guard<mutex> lock(queue_mutex);
                    thread t(&flights::MakeFlightThread, Flights[i]);
                    getFlightsFromThreads[t.get_id()] = Flights[i];
                    bool isEmergency = (rand() % 100) < Flights[i]->emergencyProbability;
                    int console_fd_inner = open("/dev/tty", O_WRONLY);
                    if (console_fd_inner != -1) {
                        int stdout_dup_inner = dup(STDOUT_FILENO);
                        dup2(console_fd_inner, STDOUT_FILENO);
                        string cliOutput;
                        if (isEmergency) {
                            emergencyThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Emergency International Arrival (North) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else if (Flights[i]->aplane->airLine.type != "Commercial") {
                            cargoThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Non-Commercial International Arrival (North) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else {
                            arrivalThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] International Arrival (North) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        }
                        write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
                        dup2(stdout_dup_inner, STDOUT_FILENO);
                        close(stdout_dup_inner);
                        close(console_fd_inner);
                    }
                    Flights[i]->Pushed = true;
                    activeFlights.push_back(i);
                } else {
                    cout << Flights[i]->aplane->id << " is Busy Waiting \n";
                }
            }
        }

        //  flights to queue after 2 seconds
        if (timePassed >= 120) {
            for (int i = 0; i < total_flights; i++) {
                if (Flights[i]->direction == "East" && !Flights[i]->Pushed) {
                    lock_guard<mutex> lock(queue_mutex);
                    thread t(&flights::MakeFlightThread, Flights[i]);
                    getFlightsFromThreads[t.get_id()] = Flights[i];
                    bool isEmergency = (rand() % 100) < Flights[i]->emergencyProbability;
                    int console_fd_inner = open("/dev/tty", O_WRONLY);
                    if (console_fd_inner != -1) {
                        int stdout_dup_inner = dup(STDOUT_FILENO);
                        dup2(console_fd_inner, STDOUT_FILENO);
                        string cliOutput;
                        if (isEmergency) {
                            emergencyThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Emergency International Departure (East) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else if (Flights[i]->aplane->airLine.type != "Commercial") {
                            cargoThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Non-Commercial International Departure (East) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else {
                            departureThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] International Departure (East) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        }
                        write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
                        dup2(stdout_dup_inner, STDOUT_FILENO);
                        close(stdout_dup_inner);
                        close(console_fd_inner);
                    }
                    Flights[i]->Pushed = true;
                    activeFlights.push_back(i);
                } else {
                    cout << Flights[i]->aplane->id << " is Busy Waiting \n";
                }
            }
        }

        // flights to queue after 5 seconds
        if (timePassed >= 300) {
            for (int i = 0; i < total_flights; i++) {
                if (Flights[i]->direction == "South" && !Flights[i]->Pushed) {
                    lock_guard<mutex> lock(queue_mutex);
                    thread t(&flights::MakeFlightThread, Flights[i]);
                    getFlightsFromThreads[t.get_id()] = Flights[i];
                    bool isEmergency = (rand() % 100) < Flights[i]->emergencyProbability;
                    int console_fd_inner = open("/dev/tty", O_WRONLY);
                    if (console_fd_inner != -1) {
                        int stdout_dup_inner = dup(STDOUT_FILENO);
                        dup2(console_fd_inner, STDOUT_FILENO);
                        string cliOutput;
                        if (isEmergency) {
                            emergencyThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Emergency Domestic Arrival (South) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else if (Flights[i]->aplane->airLine.type != "Commercial") {
                            cargoThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Non-Commercial Domestic Arrival (South) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else {
                            arrivalThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Domestic Arrival (South) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        }
                        write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
                        dup2(stdout_dup_inner, STDOUT_FILENO);
                        close(stdout_dup_inner);
                        close(console_fd_inner);
                    }
                    Flights[i]->Pushed = true;
                    activeFlights.push_back(i);
                } else {
                    cout << Flights[i]->aplane->id << " is Busy Waiting \n";
                }
            }
        }

        // flights to queue after 4 seconds
        if (timePassed >= 240) {
            for (int i = 0; i < total_flights; i++) {
                if (Flights[i]->direction == "West" && !Flights[i]->Pushed) {
                    lock_guard<mutex> lock(queue_mutex);
                    thread t(&flights::MakeFlightThread, Flights[i]);
                    getFlightsFromThreads[t.get_id()] = Flights[i];
                    bool isEmergency = (rand() % 100) < Flights[i]->emergencyProbability;
                    int console_fd_inner = open("/dev/tty", O_WRONLY);
                    if (console_fd_inner != -1) {
                        int stdout_dup_inner = dup(STDOUT_FILENO);
                        dup2(console_fd_inner, STDOUT_FILENO);
                        string cliOutput;
                        if (isEmergency) {
                            emergencyThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Emergency Domestic Departure (West) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else if (Flights[i]->aplane->airLine.type != "Commercial") {
                            cargoThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Non-Commercial Domestic Departure (West) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        } else {
                            departureThreadFlights.push(move(t));
                            cliOutput = "[" + to_string(timePassed) + "s] Domestic Departure (West) Plane " + to_string(Flights[i]->aplane->id) + " added\n";
                        }
                        write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
                        dup2(stdout_dup_inner, STDOUT_FILENO);
                        close(stdout_dup_inner);
                        close(console_fd_inner);
                    }
                    Flights[i]->Pushed = true;
                    activeFlights.push_back(i);
                } else {
                    cout << Flights[i]->aplane->id << " is Busy Waiting \n";
                }
            }
        }

        // Sort emergency flights by priority
        if (!emergencyThreadFlights.empty()) {
            ProbabilitySort(emergencyThreadFlights, Flights, total_flights);
        }

        // Process flights from queues
        moveDepartureFlight(Flights, 0);

        // Update active flights list
        vector<int> newActiveFlights;
        for (int i : activeFlights) {
            if (!Flights[i]->aplane->faulty &&
                ((Flights[i]->direction == "North" || Flights[i]->direction == "South") && Flights[i]->phase != "At Gate") ||
                (!(Flights[i]->direction == "North" || Flights[i]->direction == "South") && Flights[i]->phase != "Cruise")) {
                char runway = Flights[i]->RunwayUsed;
                if (runway == ' ') runway = (Flights[i]->direction == "North" || Flights[i]->direction == "South") ? 'A' : 'B';
                moveFlight(Flights, i, runway, false);
                newActiveFlights.push_back(i);
            }
        }
        activeFlights = newActiveFlights;

        // Update flights that haven't been pushed to the queue
        for (int i = 0; i < total_flights; i++) {
            if (!Flights[i]->Pushed) {
                moveFlight(Flights, i, ' ', false);
            }
        }

        this_thread::sleep_for(chrono::milliseconds(16)); // Simulation tick
    }

    // Join all phase change threads
    for (auto& t : ChangePhaseThread) {
        if (t.joinable()) t.join();
    }

    // Output simulation completion message
    int console_fd = open("/dev/tty", O_WRONLY);
    if (console_fd != -1) {
        int stdout_dup = dup(STDOUT_FILENO);
        dup2(console_fd, STDOUT_FILENO);
        string cliOutput = "\nSimulation completed.\n";
        write(STDOUT_FILENO, cliOutput.c_str(), cliOutput.length());
        dup2(stdout_dup, STDOUT_FILENO);
        close(stdout_dup);
        close(console_fd);
    }
}


int main() {
    srand(time(nullptr)); 
    InitializePlanes(AirPlanes);
    InitializeFlights(Flights, AirPlanes); 


    int totalViolations;
    violations = readViolationsFromFile("violations.txt", totalViolations);
    GlobalAVNID = totalViolations + 1;

    // Create flight FIFO for communication with SFML process
    unlink(Flight_Pipe);
    if (mkfifo(Flight_Pipe, 0666) == -1) {
        perror("mkfifo failed for flight FIFO");
        return 1;
    }

    // Fork SFML process for visualization
    pid_t sfml_pid = fork();
    if (sfml_pid == -1) {
        perror("fork failed for SFML process");
        return 1;
    }

    if (sfml_pid == 0) {
        execlp("./sfml_process", "./sfml_process", NULL);
        perror("execlp failed for SFML process");
        exit(1);
    }


    unlink(Violation_Pipe);
    if (mkfifo(Violation_Pipe, 0666) == -1) {
        perror("mkfifo failed for violation FIFO");
        return 1;
    }

    // Fork violation handler process
    pid_t violation_pid = fork();
    if (violation_pid == -1) {
        perror("fork failed for violation handler");
        return 1;
    }

    if (violation_pid == 0) {
        // Violation handler process: reads from violation FIFO and writes to file
        int violation_read_fd = open(Violation_Pipe, O_RDONLY);
        if (violation_read_fd == -1) {
            perror("open violation FIFO for reading failed");
            exit(1);
        }
        char buffer[512];
        while (true) {
            ssize_t bytesRead = read(violation_read_fd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                string line(buffer);
                stringstream ss(line);
                string token;
                vector<string> tokens;
                while (getline(ss, token, ':')) {
                    tokens.push_back(token);
                }
                if (tokens.size() == 5) {
                    int flightID = stoi(tokens[0]);
                    string airlineName = tokens[1];
                    string aircraftType = tokens[2];
                    int recordedSpeed = stoi(tokens[3]);
                    string details = tokens[4];
                    appendViolationFromData(flightID, airlineName, aircraftType, recordedSpeed, details, "violations.txt");
                }
            } else if (bytesRead == 0) {
                break; 
            } else {
                perror("read from violation FIFO failed");
                break;
            }
        }
        close(violation_read_fd);
        exit(0);
    }


    violation_fd = open(Violation_Pipe, O_WRONLY);
    if (violation_fd == -1) {
        perror("open violation FIFO for writing failed");
        return 1;
    }


    thread RunWay1(&runway::MakeThread, &RWYA);
    thread RunWay2(&runway::MakeThread, &RWYB);
    thread RunWay3(&runway::MakeThread, &RWYC);
    thread simulateThread(Simulate, Flights, NUM_FLIGHTS);

    // Wait for all threads to complete
    RunWay1.join();
    RunWay2.join();
    RunWay3.join();
    simulateThread.join();


    close(violation_fd);
    waitpid(sfml_pid, NULL, 0);    // Wait for SFML process 
    waitpid(violation_pid, NULL, 0); // Wait for violation handler

    // Free Memory
    for (int i = 0; i < NUM_PLANES; i++) delete AirPlanes[i];
    for (int i = 0; i < NUM_FLIGHTS; i++) delete Flights[i];


    unlink(Flight_Pipe);
    unlink(Violation_Pipe);
    return 0;
}
