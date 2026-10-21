#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sys/file.h>

using namespace sf;
using namespace std;

bool isPushed = false;             
const int Total_Flights = 14;        
mutex flightMutex;                 

struct Flight {
    int id;                         
    Sprite sprite;                   
    Vector2f position;               
    string direction;                
    string airline;                 
    char runway;                     
    string phase;                   
    bool active;                    
    bool pushed;                    
    float timeActive;               
    string height;                  
    bool beingProcessed;            
    bool isdone;                    

    Flight() : id(0), runway(' '), active(false), pushed(false), timeActive(0.0f), beingProcessed(false), isdone(false) {}
};

void showFlightDetails(RenderWindow& window, Font& font, const vector<Flight>& flights) {
    string details;                  // Get flight details
    for (const auto& flight : flights) {
        details += "ID: " + to_string(flight.id) +
                   ", Dir: " + flight.direction +
                   ", RWY: " + string(1, flight.runway) +
                   ", Airline: " + flight.airline +
                   ", Phase: " + flight.phase +
                   ", Height: " + flight.height +
                   ", Proc: " + (flight.beingProcessed ? "Yes" : "No") +
                   ", Active: " + (flight.isdone ? "Yes" : "No") + "\n";
    }
    Text text(details, font, 15);
    text.setFillColor(Color::Black);
    text.setPosition(10, 500);
    window.draw(text);
}

void updateFromPipe(vector<Flight>& flights, int pipeFd, Texture& planeTexture) {
    char buffer[512];
    int bytesRead = read(pipeFd, buffer, sizeof(buffer)-1);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        stringstream ss(buffer);
        string line;

        while (getline(ss, line)) {
            stringstream lineStream(line);
            string token;
            vector<string> tokens;

            while (getline(lineStream, token, ':')) {
                tokens.push_back(token);
            }

            if (tokens.size() >= 5) {

                    lock_guard<mutex> lock(flightMutex);
                    int id = stoi(tokens[0]);
                    string direction = tokens[1];
                    char runway = tokens[2][0];
                    string airline = tokens[4];
                    string phase = tokens[3];
                    string height = tokens[6];
                    string beingProcessed = tokens[5];

                    bool found = false;
                    for (auto& flight : flights) {
                        if (flight.id == id) {
                            flight.direction = direction;
                            flight.runway = runway;
                            flight.airline = airline;
                            flight.phase = phase;
                            flight.height = height;
                            flight.pushed = true;
                            isPushed = true;
                            flight.active = (phase == "Taxi" || phase == "Takeoff Roll" || phase == "Landing");
                            if (phase != "At Gate") {
                                flight.isdone = true;
                            } else {
                                flight.isdone = false;
                            }

                            float runwayX;
                            if (runway == 'A') runwayX = 1000;
                            else if (runway == 'B') runwayX = 1000;
                            else if (runway == 'C') runwayX = 1000;

                            if (phase == "At Gate") {
                                // flight.position = Vector2f(100, 100.0f);
                            } else if (phase == "Taxi") {
                                if (direction == "North" || direction == "South") {
                                    if (runway == 'C') 
                                    {
                                      flight.position = Vector2f(1000, 800);
                                    }
                                    else if (runway == 'B')
                                    {
                                      flight.position = Vector2f(1000, 500);
                                      }
                                    else if (runway == 'A')
                                    {
                                      flight.position = Vector2f(1000, 250);
                                      }
                                } else {
                                    if (runway == 'C') 
                                    {
                                      flight.position = Vector2f(1000, 800);
                                      }
                                    else if (runway == 'B')
                                    {
                                      flight.position = Vector2f(1000, 500);
                                      }
                                    else if (runway == 'A') 
                                    {
                                      flight.position = Vector2f(1000, 250);
                                      }
                                }
                            } else if (phase == "Takeoff Roll") {
                                if (runway == 'C') 
                                {
                                  flight.position = Vector2f(1000, 800);
                                  }
                                else if (runway == 'B') 
                                {
                                  flight.position = Vector2f(1000, 500);
                                  }
                                else if (runway == 'A') 
                                {
                                  flight.position = Vector2f(1000, 250);
                                  }
                            } else if (phase == "Landing") {
                                if (runway == 'C') 
                                { 
                                  flight.position = Vector2f(1000, 800);
                                  }
                                else if (runway == 'B') 
                                {
                                  flight.position = Vector2f(1000, 500);
                                  }
                                else if (runway == 'A') 
                                {
                                  flight.position = Vector2f(1000, 250);
                                  }
                            }

                            flight.sprite.setPosition(flight.position);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        Flight newFlight;
                        newFlight.id = id;
                        newFlight.sprite.setTexture(planeTexture);
                        newFlight.sprite.setScale(0.2f, 0.2f);
                        newFlight.direction = direction;
                        newFlight.runway = runway;
                        newFlight.airline = airline;
                        newFlight.phase = phase;
                        newFlight.pushed = true;
                        newFlight.active = (phase == "Taxi" || phase == "Takeoff Roll" || phase == "Landing");
                        newFlight.timeActive = 0.0f;

                        float runwayX;
                        if (runway == 'A') 
                        {
                          runwayX = 1000;
                          }
                        else if (runway == 'B') 
                        {
                          runwayX = 1000;
                          }
                        else if (runway == 'C') 
                        {
                          runwayX = 1000;
                          }

                        if (phase == "At Gate") {
                            // newFlight.position = Vector2f(runwayX, 100.0f);
                        } else if (phase == "Taxi") {
                            if (direction == "North" || direction == "South") {
                                if (runway == 'C')
                                {
                                  newFlight.position = Vector2f(1000, 800);
                                  }
                                else if (runway == 'B')
                                {
                                  newFlight.position = Vector2f(1000, 500);
                                  }
                                else if (runway == 'A') 
                                {
                                  newFlight.position = Vector2f(1000, 250);
                                  }
                            } else {
                                if (runway == 'C') 
                                {
                                  newFlight.position = Vector2f(1000, 800);
                                  }
                                else if (runway == 'B') 
                                {
                                  newFlight.position = Vector2f(1000, 500);
                                  }
                                else if (runway == 'A') 
                                {
                                  newFlight.position = Vector2f(1000, 250);
                                  }
                            }
                        } else if (phase == "Takeoff Roll") {
                            if (runway == 'C') 
                            {
                              newFlight.position = Vector2f(1000, 800);
                              }
                            else if (runway == 'B') 
                            {
                              newFlight.position = Vector2f(1000, 500);
                              }
                            else if (runway == 'A') 
                            {
                              newFlight.position = Vector2f(1000, 250);
                              }
                        } else if (phase == "Landing") {
                            if (runway == 'C') 
                            {
                              newFlight.position = Vector2f(1000, 800);
                              }
                            else if (runway == 'B')
                            {
                              newFlight.position = Vector2f(1000, 500);
                              }
                            else if (runway == 'A') 
                            {
                              newFlight.position = Vector2f(1000, 250);
                              }
                        }

                        newFlight.sprite.setPosition(newFlight.position);
                        flights.push_back(newFlight);
                    }
                
            }
        }
    }
}
/*
string getCurrentDateTime() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buffer);
}

string calculateDueDate(const string& issuanceDate) {
    int year, month, day, hour, minute, second;
    char dash, colon, space;
    istringstream iss(issuanceDate);
    iss >> year >> dash >> month >> dash >> day >> space >> hour >> colon >> minute >> colon >> second;
    day += 3;
    int daysInMonth;
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: daysInMonth = 31; break;
        case 4: case 6: case 9: case 11: daysInMonth = 30; break;
        case 2: daysInMonth = 28; break;
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
    ostringstream oss;
    string monthStr, dayStr, hourStr, minuteStr, secondStr;
    if (month < 10) monthStr = "0" + to_string(month); else monthStr = to_string(month);
    if (day < 10) dayStr = "0" + to_string(day); else dayStr = to_string(day);
    if (hour < 10) hourStr = "0" + to_string(hour); else hourStr = to_string(hour);
    if (minute < 10) minuteStr = "0" + to_string(minute); else minuteStr = to_string(minute);
    if (second < 10) secondStr = "0" + to_string(second); else secondStr = to_string(second);
    oss << year << "-" << monthStr << "-" << dayStr << " "
        << hourStr << ":" << minuteStr << ":" << secondStr;
    return oss.str();
}

string extractPermissibleSpeed(const string& details) {
    size_t start = details.find('[');
    size_t end = details.find(']');
    if (start != string::npos && end != string::npos && end > start) {
        return details.substr(start, end - start + 1);
    }
    return "Unknown";
}
*/
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
    Violation(int index, int flightID, string airlineName, string aircraftType, int recordedSpeed, string details) 
        : avnID(index + 1), airlineName(airlineName), flightNumber(flightID), aircraftType(aircraftType), 
          recordedSpeed(recordedSpeed), permissibleSpeed(extractPermissibleSpeed(details)), 
          issuanceDateTime(getCurrentDateTime()), paymentStatus(false), 
          dueDate(calculateDueDate(getCurrentDateTime())), details(details) {
        int baseFine;
        if (aircraftType == "Commercial") 
        {
          baseFine = 500000;
          }
        else if (aircraftType == "Cargo") 
        {
          baseFine = 700000;
          }
        else 
        {
          baseFine = 500000;
          }
        double serviceFee = baseFine * 0.15;
        totalFineAmount = static_cast<int>(baseFine + serviceFee);
    }
};
/*
vector<Violation> readViolationsFromFile(const string& filename) {
    vector<Violation> violations;
    ifstream infile(filename, ios::in);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file '" << filename << "' for reading." << endl;
        return violations;
    }

    string line;
    Violation v;
    int fieldCount = 0;
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
    cout << "Read " << violations.size() << " violations from '" << filename << "'." << endl;
    return violations;
}

void storeViolationsInFile(const vector<Violation>& violations, const string& filename) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        cerr << "Error: Could not open file '" << filename << "' for writing." << endl;
        return;
    }

    if (flock(fd, LOCK_EX) == -1) {
        cerr << "Error: Could not lock file '" << filename << "'." << endl;
        close(fd);
        return;
    }

    int stdout_copy = dup(STDOUT_FILENO);
    if (dup2(fd, STDOUT_FILENO) == -1) {
        cerr << "Error: dup2 failed." << endl;
        close(fd);
        return;
    }

    cout << violations.size() << endl;
    for (const Violation& v : violations) {
        cout << "AVN ID: " << v.avnID << "\n";
        cout << "Airline name: " << v.airlineName << "\n";
        cout << "Flight number: " << v.flightNumber << "\n";
        cout << "Aircraft type: " << v.aircraftType << "\n";
        cout << "Speed recorded vs. permissible: " << v.recordedSpeed << " vs. " << v.permissibleSpeed << "\n";
        cout << "AVN issuance date and time: " << v.issuanceDateTime << "\n";
        cout << "Total fine amount (including service fee): PKR " << v.totalFineAmount << "\n";
        cout << "Payment status: " << (v.paymentStatus ? "Paid" : "Unpaid") << "\n";
        cout << "Due date: " << v.dueDate << "\n";
        cout << "----------------------------------------\n";
    }

    cout.flush();
    dup2(stdout_copy, 1);
    close(stdout_copy);
    close(fd);
    flock(fd, LOCK_UN);
    cout << "Violations successfully written to '" << filename << "'." << endl;
}
*/
void showViolation() {
    RenderWindow window(VideoMode(1500, 900), "Violation Screen");
    Font fontV;
    if (!fontV.loadFromFile("fontType.ttf")) {
        cout << "Error in Loading Font\n";
        return;
    }

    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("bg2.png")) {
        cout << "Failed to load background image\n";
    }
    sf::Sprite bgSprite(bgTexture);

    Text instV("", fontV, 20);
    instV.setPosition(30, 30);
    instV.setFillColor(Color::White);

    Text inputText("", fontV, 18);
    inputText.setPosition(30, 150);
    inputText.setFillColor(Color::Green);

    Text violationsText("", fontV, 16);
    violationsText.setPosition(30, 200);
    violationsText.setFillColor(Color::Black);

    string input = "";
    string label = "";
    bool enteringID = false;
    bool enteringPayment = false;
    bool IDEntered = false;
    bool PaymentEntered = false;
    int ANVID = 0;
    int payment = 0;

    vector<Violation> violations = readViolationsFromFile("violations.txt");

    stringstream violationsDisplay;
    for (const auto& v : violations) {
        violationsDisplay << "AVN ID: " << v.avnID << ", Flight: " << v.flightNumber
                         << ", Airline: " << v.airlineName << ", Fine: PKR " << v.totalFineAmount
                         << ", Status: " << (v.paymentStatus ? "Paid" : "Unpaid") << "\n";
    }
    violationsText.setString(violationsDisplay.str());

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed || (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape)) {
                storeViolationsInFile(violations, "violations.txt");
                window.close();
            }

            if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::A) {
                    enteringID = true;
                    enteringPayment = false;
                    label = "Enter ANVID: ";
                    input = "";
                } else if (event.key.code == Keyboard::P) {
                    enteringID = false;
                    enteringPayment = true;
                    label = "Enter Payment: ";
                    input = "";
                } else if (event.key.code == Keyboard::Enter) {
                    if (enteringID && !input.empty()) {
                        
                            ANVID = stoi(input);
                            bool validID = false;
                            for (const auto& v : violations) {
                                if (v.avnID == ANVID) {
                                    validID = true;
                                    break;
                                }
                            }
                            if (validID) {
                                cout << "ANVID Entered: " << ANVID << "\n";
                                IDEntered = true;
                                input = "";
                                label = "Press P to enter Payment";
                                enteringID = false;
                            } else {
                                label = "Invalid ANVID! Press A to try again.";
                                input = "";
                            }
                        
                    } else if (enteringPayment && !input.empty() && IDEntered) {
                       
                            payment = stoi(input);
                            for (auto& v : violations) {
                                if (v.avnID == ANVID) {
                                    if (payment >= v.totalFineAmount) {
                                        v.paymentStatus = true;
                                        cout << "Payment of " << payment << " for ANVID " << ANVID << " accepted.\n";
                                        PaymentEntered = true;
                                        violationsDisplay.str("");
                                        for (const auto& viol : violations) {
                                            violationsDisplay << "AVN ID: " << viol.avnID << ", Flight: " << viol.flightNumber
                                                             << ", Airline: " << viol.airlineName << ", Fine: PKR " << viol.totalFineAmount
                                                             << ", Status: " << (viol.paymentStatus ? "Paid" : "Unpaid") << "\n";
                                        }
                                        violationsText.setString(violationsDisplay.str());
                                        storeViolationsInFile(violations, "violations.txt");
                                    } else {
                                        label = "Insufficient payment! Press P to try again.";
                                        input = "";
                                    }
                                    break;
                                }
                            }
                            if (PaymentEntered) {
                                input = "";
                                label = "Press A to enter ANVID";
                                enteringPayment = false;
                            }
                        
                    }
                }
            }

            if (PaymentEntered && IDEntered) {
                label = "Payment Added Successfully\nPress A or P for new entry";
                input = "";
                enteringID = false;
                enteringPayment = false;
                IDEntered = false;
                PaymentEntered = false;
            }

            if (event.type == Event::TextEntered) {
                if (event.text.unicode < 128) {
                    char c = static_cast<char>(event.text.unicode);

                    if (c == '\b') {
                        if (!input.empty()) {
                            input.pop_back();
                        }
                    } else if (c >= '0' && c <= '9') {
                        input += c;
                    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                        input += c;
                    } else if (c == ' ') {
                        input += c;
                    }
                }
            }
        }

        inputText.setString(label + input);
        window.clear(Color::Black);
        window.draw(bgSprite);
        window.draw(instV);
        window.draw(inputText);
        window.draw(violationsText);
        window.display();
    }
}

void SignInScreen() {
    RenderWindow window(VideoMode(1000, 800), "Sign In");
    Font font;
    if (!font.loadFromFile("fontType.ttf")) {
        cerr << "Font load failed.\n";
        return;
    }

    Text instruction("Press A to enter AVNID\nPress D to enter Date\nPress Enter to confirm\nPress Tab to OpenPortal", font, 20);
    instruction.setPosition(30, 30);
    instruction.setFillColor(Color::White);

    Text inputText("", font, 18);
    inputText.setPosition(30, 150);
    inputText.setFillColor(Color::Green);

    string currentInput = "";
    string currentLabel = "";
    bool IDEntered = false;
    bool DateEntered = false;
    bool enteringAVNID = false, enteringDate = false;

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed)
                window.close();

            if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::A) {
                    enteringAVNID = true;
                    enteringDate = false;
                    currentInput = "";
                    IDEntered = true;
                    currentLabel = "Enter AVNID: ";
                    continue;
                } else if (event.key.code == Keyboard::D) {
                    enteringAVNID = false;
                    enteringDate = true;
                    currentInput = "";
                    DateEntered = true;
                    currentLabel = "Enter Date: ";
                    continue;
                } else if (event.key.code == Keyboard::Enter) {
                    if (enteringAVNID && !currentInput.empty()) {
                        cout << "AVNID: " << currentInput << "\n";
                    } else if (enteringDate && !currentInput.empty()) {
                        cout << "Date: " << currentInput << "\n";
                    }
                    currentInput = "";
                    currentLabel = "Saved! Press A or D to enter again.";
                    enteringAVNID = enteringDate = false;
                } else if (event.key.code == Keyboard::Tab) {
                    if (IDEntered && DateEntered) {
                        window.close();
                        return;
                    }
                }
            }
        }

        inputText.setString(currentLabel + currentInput);
        window.clear();
        window.draw(instruction);
        window.draw(inputText);
        window.display();
    }
}

void SimulationScreen() {
    RenderWindow window(VideoMode(1700, 900), "Flight Simulation");
    window.setFramerateLimit(60);
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("bg1.png")) {
        std::cerr << "Failed to load background image\n";
    }
    sf::Sprite bgSprite(bgTexture);

    const char* pipePath = "/tmp/flight_fifo";
    int pipeFd = open(pipePath, O_RDONLY | O_NONBLOCK);
    if (pipeFd == -1) {
        cerr << "Failed to open pipe: " << pipePath << endl;
        return;
    }

    Texture planeTexture;
    if (!planeTexture.loadFromFile("plane1.png")) {
        cerr << "Failed to load plane texture!" << endl;
        close(pipeFd);
        return;
    }

    vector<Flight> flights;
    Clock clock;

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
            }
            if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::V) {
                    window.close();
                    showViolation();
                }
            }
        }

        float dt = clock.restart().asSeconds();
        updateFromPipe(flights, pipeFd, planeTexture);

        for (auto& flight : flights) {
            if (flight.active) {
                flight.timeActive += dt;
                float speed = 100.0f;

                if (flight.phase == "Taxi") {
                    if (flight.direction == "North") {
                        if (flight.runway == 'C') {
                            flight.position.x += speed ;
                        } else {
                            flight.position.x += speed ;
                        }
                    } else if (flight.direction == "South") {
                        if (flight.runway == 'C') {
                            flight.position.x += speed ;
                        } else {
                            flight.position.x += speed ;
                        }
                    } else if (flight.direction == "East") {
                        if (flight.runway == 'C') {
                            flight.position.x += speed ;
                        } else {
                            flight.position.x += speed ;
                        }
                    } else if (flight.direction == "West") {
                        if (flight.runway == 'C') {
                            flight.position.x += speed ;
                        } else {
                            flight.position.x += speed ;
                        }
                    }
                } else if (flight.phase == "Takeoff Roll") {
                    float runwayX;
                    if (flight.runway == 'A') {
                        flight.position = Vector2f(1000, 250);
                    } else if (flight.runway == 'B') {
                    } else {
                        flight.position = Vector2f(1000, 500);
                    }
                } else if (flight.phase == "Landing") {
                    float runwayX;
                    if (flight.runway == 'A') {
                    } else if (flight.runway == 'B') {
                    } else {
                        flight.position = Vector2f(1000, 800);
                    }
                }

                flight.sprite.setPosition(flight.position);
            }
        }

        window.clear(Color::Black);
        Font font;
        if (!font.loadFromFile("fontType.ttf")) {
            cerr << "Font load failed.\n";
            return;
        }

        window.draw(bgSprite);
        lock_guard<mutex> lock(flightMutex);
        for (int i=0; i< flights.size();i++) {
            if (flights[i].active && flights[i].pushed && flights[i].position.y >= 200.0f) {
                window.draw(flights[i].sprite);
            }
        }
        showFlightDetails(window, font, flights);
        window.display();

        this_thread::sleep_for(chrono::milliseconds(10));
    }

    close(pipeFd);
}

int main() {
    SignInScreen();
    //Main Simulation Screen
    SimulationScreen();
    return 0;
}
